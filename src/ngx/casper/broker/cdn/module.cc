/**
 * @file module.cc
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

#include "ngx/casper/broker/cdn/module.h"

#include "ngx/casper/broker/cdn/errors.h"

#include <algorithm>

const char* const ngx::casper::broker::cdn::Module::sk_rx_content_type_ = "application/json";
const char* const ngx::casper::broker::cdn::Module::sk_tx_content_type_ = "application/json;charset=utf-8";

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_log_conf
 * @param a_ngx_cdn_loc_conf
 */
ngx::casper::broker::cdn::Module::Module (const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params,
                                          ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_module_loc_conf_t& a_ngx_cdn_loc_conf)
    : ngx::casper::broker::Module("cdn", a_config, a_params),
     jwt_offset_(static_cast<size_t>(a_ngx_cdn_loc_conf.jwt_prefix.len)),
     job_(ctx_, this, a_params, a_ngx_loc_conf)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::Module::~Module ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return
 */
ngx_int_t ngx::casper::broker::cdn::Module::Run ()
{
    // ... error set @ constructor?
    if (  NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }

    // ... a 'GET' or 'POST' request is expected ...
    if ( not ( NGX_HTTP_GET == ctx_.ngx_ptr_->method || NGX_HTTP_POST == ctx_.ngx_ptr_->method ) ) {
        // ... method not implemented ...
        return NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    }
    
    // .. handle request ...
    std::string jwt_b64;
    
    switch(ctx_.ngx_ptr_->method) {
        case NGX_HTTP_GET:
        {
            if ( 0 == ctx_.request_.uri_.length() ) {
                return NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
            } else {
                jwt_b64 = std::string(ctx_.request_.uri_.c_str() + jwt_offset_);
            }
            break;
        }
        case NGX_HTTP_POST:
            if ( nullptr == ctx_.request_.body_ ) {
                return NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_BODY_ERROR");
            } else {
                jwt_b64 = ctx_.request_.body_;
            }
            break;
        default:
            // ... method not implemented ...
            return NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    }
    
    // ... log
    NGX_BROKER_MODULE_DEBUG_LOG(ctx_.module_, ctx_.ngx_ptr_, ctx_.log_token_.c_str(),
                                "RUN", "JWT",
                                "%s", jwt_b64.c_str()
    );
    
    // ... we're done  ...
    return job_.Submit(jwt_b64);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Content handler factory.
 *
 * @param a_r
 * @param a_at_rewrite_handler
 */
ngx_int_t ngx::casper::broker::cdn::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
{
    //
    // GRAB 'MAIN' CONFIG
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }

    //
    // GRAB 'MODULE' CONFIG
    //
    ngx_http_casper_broker_cdn_module_loc_conf_t* loc_conf =
        (ngx_http_casper_broker_cdn_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_module);
    if ( NULL == loc_conf ) {
        return NGX_ERROR;
    }
    
    //
    // 'WARM UP'
    //
    ngx::casper::broker::Module::Params params = {
        /* in_headers_              */ {},
        /* config_                  */ {},
        /* locale_                  */ "",
        /* supported_content_types_ */ { "application/json", "application/text" }
    };
    
    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_cdn_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::Errors configuration_errors(params.locale_);
    
    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_module, a_r, loc_conf->log_token,
                                                       {
                                                           { "jwt_iss"               , &broker_conf->jwt_iss                },
                                                           { "jwt_rsa_public_key_uri", &broker_conf->jwt_rsa_public_key_uri },
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::Module::sk_tx_content_type_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [] (const std::string& a_locale) {
            return new ngx::casper::broker::cdn::Errors(a_locale);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ false,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &broker_conf, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::cdn::Module(config, params, *broker_conf, *loc_conf);
                                                     }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Called by nginx a request body is read to be read.
 *
 * @param a_request.
 */
void ngx::casper::broker::cdn::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_cdn_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_cdn_module, a_data);
}
