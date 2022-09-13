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

#include "ngx/casper/broker/oauth/server/module.h"

#include "ngx/casper/broker/oauth/server/errors.h"

#include "cc/auth/jwt.h"
#include "cc/auth/exception.h"
#include "cc/easy/json.h"

#include "ev/beanstalk/producer.h"
#include "ev/ngx/bridge.h"

#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

extern "C" {
    #include "ngx_string.h" // ngx_unescape_uri
}
#include "ngx/ngx_utils.h"

const char* const ngx::casper::broker::oauth::server::Module::k_json_oauth_server_content_type_           = "application/json";
const char* const ngx::casper::broker::oauth::server::Module::k_json_oauth_server_content_type_w_charset_ = "application/json;charset=utf-8";

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_log_conf
 */
ngx::casper::broker::oauth::server::Module::Module (const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params,
                                                    ngx_http_casper_broker_oauth_server_module_loc_conf_t& /* a_ngx_loc_conf */)
    : ngx::casper::broker::Module("oauth-server", a_config, a_params)
{
    authorization_code_ = nullptr;
    access_token_       = nullptr;
    refresh_token_      = nullptr;
    client_credentials_ = nullptr;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::oauth::server::Module::~Module ()
{
    ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
    if ( nullptr != authorization_code_ ) {
        delete authorization_code_;
    }
    if ( nullptr != access_token_ ) {
        delete access_token_;
    }
    if ( nullptr != refresh_token_ ) {
        delete refresh_token_;
    }
    if ( nullptr != client_credentials_ ) {
        delete client_credentials_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return
 */
ngx_int_t ngx::casper::broker::oauth::server::Module::Run ()
{
    // ... error set @ constructor?
    if (  NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    
    /*
     * https://tools.ietf.org/html/rfc6749
     */
    
    std::string refresh_token;
    std::string client_id;
    std::string client_secret;
    std::string response_type;
    std::string redirect_uri;
    std::string grant_type;
    std::string code;
    std::string scope;
    std::string state;
    std::string bypass_rfc;
    std::string authorization;
    std::string tmp_str;
    
    // ... read all possible params ...
    const std::map<std::string, std::string&> expected_args = {
        { "client_id"        , client_id        },
        { "client_secret"    , client_secret    },
        { "response_type"    , response_type    },
        { "redirect_uri"     , redirect_uri     },
        { "grant_type"       , grant_type       },
        { "code"             , code             },
        { "refresh_token"    , refresh_token    },
        { "scope"            , scope            },
        { "state"            , state            },
        // ...
        { "bypass_rfc"      , bypass_rfc }
    };
    
    // ... from now on an error should be considered a 'bad request' ...
    ctx_.response_.status_code_  = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_  = NGX_ERROR;
    ctx_.response_.asynchronous_ = false;

    // ... set args ...
    ngx_str_t args;
    switch (ctx_.ngx_ptr_->method) {
        case NGX_HTTP_GET:
            args.data = ctx_.ngx_ptr_->args.data;
            args.len  = ctx_.ngx_ptr_->args.len;
            break;
        case NGX_HTTP_POST:
            args.data = (u_char*)ctx_.request_.body_;
            args.len  = nullptr != ctx_.request_.body_ ? strlen(ctx_.request_.body_) : 0;
            break;
        default:
            return NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);;
    }
    
    // ... no args, nothing to do ...
    if ( 0 == args.len ) {
        return NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Missing arguments!");
    }

    // ... collect all arguments ...
    std::map<std::string, std::string> extra_arguments;
    (void)ngx::utls::nrs_ngx_get_args(ctx_.ngx_ptr_, &args, extra_arguments);

    // ... parse args ...
    for ( auto it : expected_args ) {
        const auto c_it = extra_arguments.find(it.first);
        if ( extra_arguments.end() == c_it ) {
            continue;
        }
        it.second = c_it->second;
        extra_arguments.erase(c_it);
    }

    const bool responses_bypass_rfc = ( 0 == bypass_rfc.compare("1") ? true : false );
    
    // ... unescape 'redirect_uri' argument ...
    if ( NGX_OK == ngx::utls::nrs_ngx_unescape_uri(ctx_.ngx_ptr_, redirect_uri.c_str(), tmp_str) ) {
        redirect_uri = tmp_str;
    }
    
    // ... unescape 'scope' argument?
    if ( scope.length() > 0 ) {
        if ( NGX_OK == ngx::utls::nrs_ngx_unescape_uri(ctx_.ngx_ptr_, scope.c_str(), tmp_str) ) {
            scope = tmp_str;
        }
    }
    
    // ... is an 'authorization' request? ...
    if ( 0 == strcmp(response_type.c_str(), "code") ) {
        
        // ... 'redirect_uri' is required ...
        if ( 0 == redirect_uri.length() ) {
            return NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, "Missing 'redirect_uri' argument!");
        }
        
        // ... asynchronously, generate a new authorization code ...
        authorization_code_ = new ngx::casper::broker::oauth::server::AuthorizationCode(ctx_.loggable_data_ref_,
                                                                                        service_id_, client_id, scope, state, redirect_uri,
                                                                                        responses_bypass_rfc
        );
        authorization_code_->AsyncRun(
                                      /* a_preload_callback */
                                      [this] () {
                                          ctx_.response_.return_code_  = NGX_OK;
                                          ctx_.response_.asynchronous_ = true;
                                      },
                                      /* a_redirect_callback */
                                      [this] (const std::string& a_uri) {
                                          ctx_.response_.redirect_.uri_ = a_uri;
                                          NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT(this);
                                      },
                                      /* a_json_callback */
                                      [this] (const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body) {
                                          ctx_.response_.return_code_  = NGX_OK;
                                          ctx_.response_.content_type_ = k_json_oauth_server_content_type_w_charset_;
                                          ctx_.response_.status_code_  = a_status_code;
                                          for ( auto it : a_headers ) {
                                              ctx_.response_.headers_[it.first] = it.second;
                                          }
                                          ctx_.response_.body_   = json_writer_.write(a_body);
                                          ctx_.response_.length_ = ctx_.response_.body_.length();
                                          if ( true == ctx_.response_.asynchronous_ ) {
                                              NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                          }
                                      },
                                      /* a_log_callback */
                                      [this] (const std::string& a_message) {
                                          ngx::casper::broker::Module::Log(ctx_.module_, ctx_.ngx_ptr_, NGX_LOG_ERR, ctx_.log_token_.c_str(),
                                                                           "CH", "ERROR",
                                                                           a_message.c_str()
                                          );
                                      }
        );
        
    } else {
        
        //
        // grant_type ZONE:
        //
        enum class GrantType {
            AuthorizationCode,
            RefreshToken,
            ClientCredentials,
            NotSupported
        };
        
        GrantType gt = GrantType::NotSupported;

        {
            //
            // GRAB 'MODULE' CONFIG
            //
            ngx_http_casper_broker_oauth_server_module_loc_conf_t* loc_conf =
            (ngx_http_casper_broker_oauth_server_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_oauth_server_module);
            if ( NULL == loc_conf || NULL == loc_conf->grant_types.data || 0 == loc_conf->grant_types.len ) {
                return NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Invalid module config!");
            }
            const ::cc::easy::JSON<::cc::Exception> json; Json::Value supported;
            json.Parse(std::string(reinterpret_cast<const char* const>(loc_conf->grant_types.data), loc_conf->grant_types.len), supported);
            if ( false == supported.isArray() ) {
                return NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Invalid module config!");
            }
            for ( Json::ArrayIndex idx = 0 ; idx < supported.size() ; ++idx ) {
                const Json::Value& s = json.Get(supported, idx, Json::ValueType::stringValue, nullptr);
                if ( 0 != strcasecmp(s.asCString(), grant_type.c_str()) ) {
                    continue;
                }
                if ( 0 == grant_type.compare("authorization_code") ) {
                    gt = GrantType::AuthorizationCode;
                } else if ( 0 == grant_type.compare("refresh_token") ) {
                    gt = GrantType::RefreshToken;
                } else if ( 0 == grant_type.compare("client_credentials") && true == s.isString() ) {
                    gt = GrantType::ClientCredentials;
                }
                break;
            }
        }
        
        // ... common requirements ...
        if ( GrantType::AuthorizationCode == gt || GrantType::RefreshToken == gt || GrantType::ClientCredentials == gt ) {
            
            // ... 'client_*' args are required ...
            
            // ... @ 'Authorization' header?
            if ( 0 == client_id.length() ) {
                // ... not, is it at headers?
                const auto it = ctx_.request_.headers_.find("authorization");
                if ( ctx_.request_.headers_.end() != it ) {
                    std::string type;
                    std::string credentials;
                    // ... 'Basic' authentication is expected ...
                    const ngx_int_t authorization_parsing = ngx::utls::nrs_ngx_parse_authorization((it->first + ": " + it->second), type, credentials);
                    if ( NGX_OK == authorization_parsing ) {
                        if ( 0 == strcasecmp(type.c_str(), "basic") ) {
                            (void)ngx::utls::nrs_ngx_parse_basic_authentication(ctx_.ngx_ptr_, credentials,
                                                                                client_id, client_secret);
                        }
                    }
                }
            } /* else {} - @ params */
            
        }
        
        switch (gt) {
                
            case GrantType::AuthorizationCode:
                // ... asynchronously, exchange an 'code' token one 'access_token' ...
                access_token_ = new ngx::casper::broker::oauth::server::AccessToken(ctx_.loggable_data_ref_,
                                                                                    service_id_, client_id, client_secret, redirect_uri,
                                                                                    code, state, responses_bypass_rfc
                );
                access_token_->AsyncRun(/* a_preload_callback */
                                        [this] () {
                                            ctx_.response_.return_code_  = NGX_OK;
                                            ctx_.response_.asynchronous_ = true;
                                        },
                                        /* a_redirect_callback */
                                        [this] (const std::string& a_uri) {
                                            ctx_.response_.redirect_.uri_ = a_uri;
                                            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT(this);
                                        },
                                        /* a_json_callback */
                                        [this] (const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body) {
                                            ctx_.response_.return_code_  = NGX_OK;
                                            ctx_.response_.content_type_ = k_json_oauth_server_content_type_w_charset_;
                                            ctx_.response_.status_code_  = a_status_code;
                                            for ( auto it : a_headers ) {
                                                ctx_.response_.headers_[it.first] = it.second;
                                            }
                                            ctx_.response_.body_   = json_writer_.write(a_body);
                                            ctx_.response_.length_ = ctx_.response_.body_.length();
                                            if ( true == ctx_.response_.asynchronous_ ) {
                                                NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                            }
                                        },
                                        /* a_log_callback */
                                        [this] (const std::string& a_message) {
                                            ngx::casper::broker::Module::Log(ctx_.module_, ctx_.ngx_ptr_, NGX_LOG_ERR, ctx_.log_token_.c_str(),
                                                                             "CH", "ERROR",
                                                                             a_message.c_str()
                                            );
                                        }
                );
                break;
                
            case GrantType::RefreshToken:
                // ... asynchronously, generate a new access token from the provided 'refresh_token' ...
                refresh_token_ = new ngx::casper::broker::oauth::server::RefreshToken(ctx_.loggable_data_ref_,
                                                                                      service_id_, client_id, client_secret,
                                                                                      refresh_token, scope, responses_bypass_rfc
                );
                refresh_token_->AsyncRun(/* a_preload_callback */
                                         [this] () {
                                             ctx_.response_.return_code_  = NGX_OK;
                                             ctx_.response_.asynchronous_ = true;
                                         },
                                         /* a_redirect_callback */
                                         [this] (const std::string& a_uri) {
                                             ctx_.response_.redirect_.uri_ = a_uri;
                                             NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT(this);
                                         },
                                         /* a_json_callback */
                                         [this] (const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body) {
                                             ctx_.response_.return_code_  = NGX_OK;
                                             ctx_.response_.content_type_ = k_json_oauth_server_content_type_w_charset_;
                                             ctx_.response_.status_code_  = a_status_code;
                                             for ( auto it : a_headers ) {
                                                 ctx_.response_.headers_[it.first] = it.second;
                                             }
                                             ctx_.response_.body_   = json_writer_.write(a_body);
                                             ctx_.response_.length_ = ctx_.response_.body_.length();
                                             if ( true == ctx_.response_.asynchronous_ ) {
                                                 NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                             }
                                         },
                                         /* a_log_callback */
                                         [this] (const std::string& a_message) {
                                             ngx::casper::broker::Module::Log(ctx_.module_, ctx_.ngx_ptr_, NGX_LOG_ERR, ctx_.log_token_.c_str(),
                                                                              "CH", "ERROR",
                                                                              a_message.c_str()
                                             );
                                         }
                );
                break;
                
            case GrantType::ClientCredentials:
                // ... asynchronously, 'client_credentials' flow ...
                client_credentials_ = new ngx::casper::broker::oauth::server::ClientCredentials(ctx_.loggable_data_ref_,
                                                                                                service_id_, client_id, client_secret,
                                                                                                scope, responses_bypass_rfc
                );
                client_credentials_->AsyncRun(/* a_preload_callback */
                                              [this] () {
                                                  ctx_.response_.return_code_  = NGX_OK;
                                                  ctx_.response_.asynchronous_ = true;
                                              },
                                              /* a_redirect_callback */
                                              [this] (const std::string& a_uri) {
                                                  ctx_.response_.redirect_.uri_ = a_uri;
                                                  NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT(this);
                                              },
                                              /* a_json_callback */
                                              [this] (const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body) {
                                                  ctx_.response_.return_code_  = NGX_OK;
                                                  ctx_.response_.content_type_ = k_json_oauth_server_content_type_w_charset_;
                                                  ctx_.response_.status_code_  = a_status_code;
                                                  for ( auto it : a_headers ) {
                                                      ctx_.response_.headers_[it.first] = it.second;
                                                  }
                                                  ctx_.response_.body_   = json_writer_.write(a_body);
                                                  ctx_.response_.length_ = ctx_.response_.body_.length();
                                                  if ( true == ctx_.response_.asynchronous_ ) {
                                                      NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                                  }
                                              },
                                              /* a_log_callback */
                                              [this] (const std::string& a_message) {
                                                  ngx::casper::broker::Module::Log(ctx_.module_, ctx_.ngx_ptr_, NGX_LOG_ERR, ctx_.log_token_.c_str(),
                                                                                   "CH", "ERROR",
                                                                                   a_message.c_str()
                                                 );
                                              }
                );
                break;
                
            default:
                ctx_.response_.status_code_ = NGX_HTTP_NOT_IMPLEMENTED;
                break;
        }
        
    }
    
    // ... we're done  ...
    return ctx_.response_.return_code_;
}


#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief This method will be called when REDIS connection was lost.
 */
void ngx::casper::broker::oauth::server::Module::OnREDISConnectionLost ()
{
    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Unable to connect to REDIS!");
    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
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
ngx_int_t ngx::casper::broker::oauth::server::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_oauth_server_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_oauth_server_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_oauth_server_module);
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
        /* supported_content_types_ */ { "application/json", "application/text", "application/x-www-form-urlencoded", "application/x-www-form-urlencoded;charset=UTF-8" }
    };
    
    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_oauth_server_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_oauth_server_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::oauth::server::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::oauth::server::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::oauth::server::Module::k_json_oauth_server_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::oauth::server::Module::k_json_oauth_server_content_type_w_charset_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [broker_conf] (const std::string& a_locale) {
            return new ngx::casper::broker::oauth::server::Errors(a_locale, broker_conf->awful_conf.ordered_errors_json_serialization_enabled);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ false,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::oauth::server::Module(config, params, *loc_conf);
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
void ngx::casper::broker::oauth::server::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_oauth_server_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::oauth::server::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_oauth_server_module, a_data);
}
