/**
 * @file session.cc
 *
 * Copyright (c) 2017-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-nginx-broker.
 *
 * casper-nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-nginx-broker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-nginx-broker. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ngx/casper/broker/ext/session.h"

#include "ngx/ngx_utils.h"

/**
 * @brief Default constructor.
 *
 * @param a_ctx
 * @param a_module
 * @param a_config
 */
ngx::casper::broker::ext::Session::Session (ngx::casper::broker::Module::CTX& a_ctx, ngx::casper::broker::Module* a_module,
                                            const ngx::casper::broker::Module::Params& a_params)
    : ngx::casper::broker::ext::Base(a_ctx, a_module),
      session_( /* a_loggable_data        */ a_ctx.loggable_data_ref_,
                /* a_iss                  */ "nginx-broker",
                /* a_sid                  */ module_ptr_->service_id_,
                /* a_token_prefix         */ module_ptr_->service_id_ + ":oauth:access_token:",
               /* a_test_maintenance_flag */ true
      )
{
    // ... if no error set ...
    if ( NGX_OK == ctx_.response_.return_code_ ) {
        const uint16_t prev_status_code = a_ctx.response_.status_code_;
        // ... ensure authorization header exists ...
        auto authorization_it = a_params.in_headers_.find("authorization");
        if ( a_params.in_headers_.end() == authorization_it ) {
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_AUTHORIZATION_HEADER");
        } else {
            std::string type;
            std::string credentials;
            const ngx_int_t authorization_parsing = ngx::utls::nrs_ngx_parse_authorization((authorization_it->first + ": " + authorization_it->second), type, credentials);
            if ( NGX_OK == authorization_parsing ) {
                if ( 0 != strcasecmp(type.c_str(), "bearer") ) {
                    // ... unsupported authorization header type ...
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_UNSUPPORTED_AUTHORIZATION_HEADER_TYPE");
                } else if ( 0 == credentials.length() ) {
                    // ... invalid authorization header value ...
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_INVALID_AUTHORIZATION_HEADER_VALUE");
                } else {
                    session_.SetToken(credentials);
                }
            } else {
                // ... invalid ...
                NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_INVALID_AUTHORIZATION_HEADER");
            }
        }
        // ... fallback to session cookie?
        if ( NGX_HTTP_BAD_REQUEST == a_ctx.response_.status_code_ && ctx_.request_.session_cookie_.name_.length() > 0 ) {
            ngx_str_t session_cookie_name;
            
            session_cookie_name.data = (u_char*) ctx_.request_.session_cookie_.name_.c_str();
            session_cookie_name.len  = ctx_.request_.session_cookie_.name_.length();
            ngx_str_t session_cookie_value = ngx_null_string;
            if ( NGX_DECLINED != ngx_http_parse_multi_header_lines(&a_ctx.ngx_ptr_->headers_in.cookies, &session_cookie_name, &session_cookie_value) ) {
                // ... accept cookie value ...
                session_.SetToken(std::string(reinterpret_cast<char const*>(session_cookie_value.data), session_cookie_value.len));
                // ... rollback status ...
                a_ctx.response_.status_code_ = prev_status_code;
                a_ctx.response_.return_code_ = NGX_OK;
                a_ctx.response_.errors_tracker_.i18n_reset_();
            }
        }
    }
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::ext::Session::~Session ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Process the previously defined HTTP request.
 *
 * @param a_success
 * @param a_failure
 * @return
 */
ngx_int_t ngx::casper::broker::ext::Session::Fetch (std::function<void(const bool a_async_request, const ::ev::casper::Session& a_session)> a_success,
                                                    std::function<void()> a_failure)
{
    // ... prepare callbacks ...
    success_callback_ = a_success;
    if ( nullptr == a_failure ) {
        failure_callback_ = [this]() {
            // ... log
            NGX_BROKER_MODULE_DEBUG_LOG(ctx_.module_, ctx_.ngx_ptr_, ctx_.log_token_.c_str(),
                                        "AH", "FINALIZING",
                                        "failure: %s", ctx_.request_.uri_.c_str()
            );
            // ... finalize ...
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
        };
    } else {
        failure_callback_ = a_failure;
    }
    
    // ... error set @ constructor?
    if (  NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    
    // ... assume request will be asynchronous ....
    ctx_.response_.asynchronous_ = true;
    
    // ... fetch session or already set?
    // ... session data is ...
    if ( false == session_.Data().token_is_valid_ ) {
        // ... token is invalid or not validated yet ...
        session_.Fetch(/* a_success_callback */
                       [this] (const ev::casper::Session::DataT& /* a_data */) {
                           // ... notify ready ...
                           success_callback_(/* a_async_request */ true, /* a_session */ session_);
                       },
                       /* a_invalid_callback */
                       [this] (const ev::casper::Session::DataT& /* a_data */ ) {
                           ctx_.response_.headers_["WWW-Authenticate"] =
                           "Bearer realm=\"api\", error=\"invalid_token\", error_description=\"The access token provided is expired, revoked, malformed, or invalid for other reasons.\"";
                           NGX_BROKER_MODULE_SET_ERROR_I18N(ctx_, NGX_HTTP_UNAUTHORIZED, "AUTH_INVALID_SESSION_ID");
                           failure_callback_();
                       },
                       /* a_failure_callback */
                       [this] (const ev::casper::Session::DataT& /* a_data */, const ::ev::Exception& a_ev_exception){
                           NGX_BROKER_MODULE_SET_ERROR_I18N_I(ctx_, NGX_HTTP_INTERNAL_SERVER_ERROR, "BROKER_REDIS_RESPONSE_ERROR", a_ev_exception.what());
                           failure_callback_();
                       }
        );
    } else {
        // ... notify ready ...
        success_callback_(/* a_async_request */ true, /* a_session */ session_);
    }
    
    // ... an error occurred?
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... an errors occurred while preparing async request ...
        // ... reset control flag ...
        ctx_.response_.asynchronous_ = false;
    }
    
    // ... we're done ...
    return ctx_.response_.return_code_;
}
