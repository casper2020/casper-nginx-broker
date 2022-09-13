/**
 * @file module.h
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

#include "ngx/casper/broker/jwt/encoder/module.h"

#include "ngx/casper/broker/jwt/encoder/errors.h"

#include "ngx/casper/broker/exception.h"

#include "cc/auth/jwt.h"
#include "cc/auth/exception.h"

#include "json/json.h"

#include "cc/auth/jwt.h"
#include "cc/auth/exception.h"

#include <algorithm>

const char* const ngx::casper::broker::jwt::encoder::Module::k_json_jwt_encoder_content_type_           = "application/text";
const char* const ngx::casper::broker::jwt::encoder::Module::k_json_jwt_encoder_content_type_w_charset_ = "application/text;charset=utf-8";

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_log_conf
 */
ngx::casper::broker::jwt::encoder::Module::Module (const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params,
                                                   ngx_http_casper_broker_jwt_encoder_module_loc_conf_t& a_ngx_loc_conf)
: ngx::casper::broker::Module("jwt-encoder", a_config, a_params)
{
    jwt_iss_             = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.iss.data), a_ngx_loc_conf.iss.len);
    jwt_public_rsa_key_  = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.rsa_public_key.data), a_ngx_loc_conf.rsa_public_key.len);
    jwt_private_rsa_key_ = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.rsa_private_key.data), a_ngx_loc_conf.rsa_private_key.len);
    jwt_duration_        = static_cast<uint64_t>(a_ngx_loc_conf.duration);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::jwt::encoder::Module::~Module ()
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
ngx_int_t ngx::casper::broker::jwt::encoder::Module::Run ()
{
    // ... error set @ constructor?
    if (  NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    
    // ... a 'POST' request is expected ...
    if ( NGX_HTTP_POST != ctx_.ngx_ptr_->method ) {
        // ... method not implemented ...
       return NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    }

    // ... from now on an error should be considered a 'bad request' ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;    
    
    // ... 'body' must be present ...
    if ( nullptr == ctx_.request_.body_ ) {
        // ... or we're done ...
        return NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_BODY_ERROR");
    }
        
    try {
        
        Json::Value  object ;
        Json::Reader reader;
        if ( false == reader.parse(ctx_.request_.body_, object, false)  ) {
            const auto errors = reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw ngx::casper::broker::Exception("bad_request",
                                                     "Parsing error: %s!",
                                                     errors[0].message.c_str());
            } else {
                throw ngx::casper::broker::Exception("bad_request",
                                                     "Unable to parse request body!");
            }
        }

        cc::auth::JWT jwt(jwt_iss_.c_str());

        // ... check if there are any reseved claims ...
        for ( auto member : object.getMemberNames() ) {
            if ( true == jwt.IsRegisteredClaim(member) ) {
                throw ngx::casper::broker::Exception("bad_request",
                                                     "Invalid JSON field name '%s' - it's a JWT registered claim!",
                                                     member.c_str()
                );
            }
            jwt.SetUnregisteredClaim(member, object[member]);
        }
        
        ctx_.response_.body_         = jwt.Encode(jwt_duration_, jwt_private_rsa_key_);
        ctx_.response_.length_       = ctx_.response_.body_.length();
        ctx_.response_.content_type_ = k_json_jwt_encoder_content_type_;
        
        ctx_.response_.status_code_ = NGX_HTTP_OK;
        ctx_.response_.return_code_ = NGX_OK;
        
    } catch (const Json::Exception& a_json_exception) {
        return NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_json_exception);
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {
        return NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_broker_exception);
    } catch (const cc::auth::Exception& a_auth_exception) {
        return NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_auth_exception);
    }
    
    // ... we're done ...
    return ctx_.response_.return_code_;
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
ngx_int_t ngx::casper::broker::jwt::encoder::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
{
    //
    // GRAB 'MODULE' CONFIG
    //
    ngx_http_casper_broker_jwt_encoder_module_loc_conf_t* loc_conf =
        (ngx_http_casper_broker_jwt_encoder_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_jwt_encoder_module);
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
        /* supported_content_types_ */ { "application/json" }
    };

    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_jwt_encoder_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::jwt::encoder::Errors configuration_errors(params.locale_);
    
    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_jwt_encoder_module, a_r, loc_conf->log_token,
                                                       {
                                                           { "nginx_casper_broker_jwt_encoder_iss"                , &loc_conf->iss             },
                                                           { "nginx_casper_broker_jwt_encoder_rsa_public_key_uri" , &loc_conf->rsa_public_key  },
                                                           { "nginx_casper_broker_jwt_encoder_rsa_private_key_uri", &loc_conf->rsa_private_key },
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_jwt_encoder_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::jwt::encoder::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::jwt::encoder::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::jwt::encoder::Module::k_json_jwt_encoder_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::jwt::encoder::Module::k_json_jwt_encoder_content_type_w_charset_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [] (const std::string& a_locale) {
            return new ngx::casper::broker::jwt::encoder::Errors(a_locale);
        },
        /* executor_factory_      */
        [] (ngx::casper::broker::ErrorsTracker& /* a_errors_tracker */) -> ngx::casper::broker::Executor* {
            return nullptr;
        },
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ true,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::jwt::encoder::Module(config, params, *loc_conf);
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
void ngx::casper::broker::jwt::encoder::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_jwt_encoder_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::jwt::encoder::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_jwt_encoder_module, a_data);
}
