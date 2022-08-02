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

#include "ngx/casper/broker/api/module.h"

#include "ngx/casper/broker/tracker.h"

#include "ngx/casper/broker/api/errors.h"

#include "ngx/ngx_utils.h"

#include <algorithm>

const char* const ngx::casper::broker::api::Module::k_json_api_content_type_           = "application/vnd.api+json";
const char* const ngx::casper::broker::api::Module::k_json_api_content_type_w_charset_ = "application/vnd.api+json;charset=utf-8";

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_loc_conf
 * @param a_ngx_api_loc_conf
 */
ngx::casper::broker::api::Module::Module (const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params,
                                          ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_api_module_loc_conf_t& a_ngx_api_loc_conf)
    : ngx::casper::broker::Module("api", a_config, a_params),
      json_api_(ctx_.loggable_data_ref_, /* a_enable_task_cancellation */ true, /* a_deferred_response */ false),
      session_(ctx_, this, a_params),
      job_(ctx_, this, a_params, a_ngx_loc_conf)
{
    // ... set JSON writer properties ...
    json_writer_.omitEndingLineFeed();
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::api::Module::~Module ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark - SETUP & RUN STEPS
#endif

ngx_int_t ngx::casper::broker::api::Module::Setup ()
{
    const ngx_int_t crv = ngx::casper::broker::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }

    //
    // JSON API v1 - http://jsonapi.org/format/
    //
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_OK;
    
    // Content Negotiation:
    //
    //  - Client Responsibilities
    //
    //   - Clients MUST send all JSON API data in request documents with the header
    //     Content-Type: application/vnd.api+json without any media type parameters.
    //
    //  - Clients MUST ignore any parameters for the application/vnd.api+json media type received in
    //    the Content-Type header of response documents.
    //
    //  - Server Responsibilities:
    //
    //   - Servers MUST send all JSON API data in response documents with the header
    //     Content-Type: application/vnd.api+json without any media type parameters.
    //
    //   - Servers MUST respond with a 415 Unsupported Media Type status code if a request specifies
    //     the header Content-Type: application/vnd.api+json with any media type parameters.
    //
    //     ⚠️
    //
    //     NOT LONG TRUE - " ON THE FLY" "NEW SPEC" ... ... ...
    //
    //     ⚠️
    //
    //   - Servers MUST respond with a 406 Not Acceptable status code if a request’s Accept header
    //     contains the JSON API media type and all instances of that media type are modified with media type
    //     parameters.
    
    // ... if no error set ...
    if ( not ( NGX_HTTP_GET == ctx_.ngx_ptr_->method || NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) ) {

        std::string search_key = ngx::casper::broker::api::Module::k_content_type_header_key_;
        std::transform(search_key.begin(), search_key.end(), search_key.begin(), ::tolower);

        // ... client content type?
        const auto client_content_type_it = ctx_.request_.headers_.find(search_key);
        if ( ctx_.request_.headers_.end() == client_content_type_it ) {
            U_ICU_NAMESPACE::Formattable args[] = {
                ngx::casper::broker::api::Module::k_content_type_header_key_
            };
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N_V(ctx_, "BROKER_MISSING_HEADER_ERROR", args, 1);
        } else {
            if ( not ( std::string::npos != client_content_type_it->second.find("application/json") ||  std::string::npos != client_content_type_it->second.find("application/vnd.api+json") ) ) {
                U_ICU_NAMESPACE::Formattable args[] = {
                    client_content_type_it->second.c_str()
                };
                NGX_BROKER_MODULE_SET_UNSUPPORTED_MEDIA_TYPE_I18N_V(ctx_, "BROKER_UNSUPPORTED_MEDIA_TYPE_ERROR", args, 1);
            }
        }
    }
    
    //
    // GRAB 'MODULE' CONFIG
    //
    ngx_http_casper_broker_api_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_api_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_api_module);
    if ( NULL == loc_conf ) {
        return NGX_ERROR;
    }

    // ... if no error set ...
    if ( NGX_OK == ctx_.response_.return_code_ ) {
        // ... set json api 'base' config ...
        if ( 0 == loc_conf->json_api_url.len ) {
            // ... missing ...
            U_ICU_NAMESPACE::Formattable args[] = {
                "",
                "nginx_casper_broker_json_api_url"
            };
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N_V(ctx_, "BROKER_MODULE_CONFIG_MISSING_OR_INVALID_DIRECTIVE_VALUE_ERROR", args, 2);
        } else {
            // ... present ...
            json_api_.GetURIs().SetBase(std::string(reinterpret_cast<char const*>(loc_conf->json_api_url.data), loc_conf->json_api_url.len));
        }
    }
    
    // ... if no error set ...
    if ( NGX_OK == ctx_.response_.return_code_ ) {
        // ... validate request URI ...
        if ( ctx_.request_.uri_.length() <= ctx_.request_.location_.length() ) {
            // ... rejected ...
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
        } else {
            // ... accepted ...
            uri_ = json_api_.GetURIs().GetBase() + ( ctx_.request_.uri_.c_str() + ctx_.request_.location_.length() );
        }
    }

    // ... done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return
 */
ngx_int_t ngx::casper::broker::api::Module::Run ()
{
    return session_.Fetch(std::bind(&ngx::casper::broker::api::Module::OnSessionFetchSucceeded, this, std::placeholders::_1, std::placeholders::_2));
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Called when a session data is validated and ready to use.
 *
 * @param a_async_request
 * @param a_session
 */
void ngx::casper::broker::api::Module::OnSessionFetchSucceeded (const bool a_async_request, const ::ev::casper::Session& a_session)
{
    // ... prepare json api ...
    json_api_.SetUserId(a_session.GetValue("user_id", ""));
    json_api_.SetEntityId(a_session.GetValue("entity_id", ""));
    json_api_.SetEntitySchema(a_session.GetValue("entity_schema", ""));
    json_api_.SetShardedSchema(a_session.GetValue("sharded_schema", ""));
    json_api_.SetSubentitySchema(a_session.GetValue("subentity_schema", ""));
    json_api_.SetSubentityPrefix(a_session.GetValue("subentity_prefix", ""));
    
    access_token_ = a_session.Data().token_;
    
    // ... first check if under maintenance flag is set ...
    if ( true == a_session.Data().maintenance_ ) {
        // ... session's entity is under maintenance ...
        NGX_BROKER_MODULE_SET_SERVICE_UNAVAILABLE(ctx_, "Session's entity is under maintenance.");
        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
    } else {
        // ... now check with gatekeeper ...
        const ev::auth::route::Gatekeeper::Status& status = ::ev::auth::route::Gatekeeper::GetInstance().Allow(
                ctx_.request_.method_, uri_, a_session,
                {
                    std::bind(&ngx::casper::broker::api::Module::OnOnGatekeeperDeflectToJob, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                    nullptr
                },
                ctx_.loggable_data_ref_
        );
        if ( NGX_HTTP_OK != status.code_ ) {
            // ... gatekeeper access denied or an error occurred ...
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(this, status.code_, ctx_.response_.content_type_, json_writer_.write(status.data_));
        } else if ( false == status.deflected_ ) {
            // ... now perform request with JSON API ...
            try {
                switch (ctx_.ngx_ptr_->method) {
                    case NGX_HTTP_GET:
                        json_api_.Get(ctx_.loggable_data_ref_,
                                      uri_,
                                      std::bind(&ngx::casper::broker::api::Module::OnJSONAPIReply, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
                        );
                        break;
                    case NGX_HTTP_POST:
                        json_api_.Post(ctx_.loggable_data_ref_,
                                       uri_, ctx_.request_.body_,
                                       std::bind(&ngx::casper::broker::api::Module::OnJSONAPIReply, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
                        );
                        break;
                    case NGX_HTTP_DELETE:
                        json_api_.Delete(ctx_.loggable_data_ref_,
                                         uri_, nullptr != ctx_.request_.body_ ? ctx_.request_.body_ : "",
                                         std::bind(&ngx::casper::broker::api::Module::OnJSONAPIReply, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
                        );
                        break;
                    case NGX_HTTP_PATCH:
                        json_api_.Patch(ctx_.loggable_data_ref_,
                                        uri_, ctx_.request_.body_,
                                        std::bind(&ngx::casper::broker::api::Module::OnJSONAPIReply, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
                        );
                        break;
                    default:
                        NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
                        break;
                }
            } catch (const ::ev::Exception& a_ev_exception) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N(ctx_, "BROKER_POSTGRESQL_REQUEST_SCHEDULE_ERROR", a_ev_exception.what());
            }
            // ... if an error is set ...
            if ( NGX_OK != ctx_.response_.return_code_ ) {
                NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
            }
        }
    }
}

/**
 * @brief Called when the JSONAPI has replied to the previously sent request.
 *
 * @param a_uri
 * @param a_json
 * @param a_error
 * @param a_status
 */
void ngx::casper::broker::api::Module::OnJSONAPIReply (const char* a_uri, const char* a_json, const char* a_error, uint16_t a_status)
{
    if ( nullptr == a_error ) {
        
        // ... log
        NGX_BROKER_MODULE_DEBUG_LOG(ctx_.module_, ctx_.ngx_ptr_, ctx_.log_token_.c_str(),
                                    "AH", "FINALIZING",
                                    "success: %s", nullptr != a_uri ? a_uri : ""
        );
        
        if ( nullptr == a_json || '\0' == a_json[0] ) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "JSONAPI replied with an empty response!");
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
        } else {
            
            Json::Value response_object;
            try {
                if ( false == json_reader_.parse(a_json, response_object) ) {
                    // ... an error occurred ...
                    const auto errors = json_reader_.getStructuredErrors();
                    if ( errors.size() > 0 ) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, ( "An error occurred while parsing JSONAPI response: " +  errors[0].message + "!" ).c_str());
                    } else {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while parsing JSONAPI response!");
                    }
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
                } else {
                    // ... we're good to go ....
                    if ( true == response_object.isMember("meta") && true == response_object["meta"].isMember("job-tube") ) {
                        const ngx_int_t post_rv = PostJob(ctx_.ngx_ptr_->method, a_uri, nullptr != ctx_.request_.body_ ? ctx_.request_.body_ : "",
                                                          /* a_tube */
                                                          response_object["meta"]["job-tube"].asString(),
                                                          /* a_ttr */
                                                          static_cast<ssize_t>(response_object["meta"].get("job-ttr", static_cast<Json::UInt64>(job_.ttr())).asUInt64()),
                                                          /* a_validity */
                                                          static_cast<ssize_t>(response_object["meta"].get("job-validity", static_cast<Json::UInt64>(job_.expires_in())).asUInt64()),
                                                          access_token_
                        );
                        if ( NGX_OK != post_rv ) {
                            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Failed to post job!");
                            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
                        }
                    } else {
                        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(this, a_status, ctx_.response_.content_type_, a_json);
                    }
                }
            } catch (const Json::Exception& a_json_exception) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_json_exception);
                NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
            }
            
        }
        
    } else { // LEGACY ?
        
        // ... log
        NGX_BROKER_MODULE_DEBUG_LOG(ctx_.module_, ctx_.ngx_ptr_, ctx_.log_token_.c_str(),
                                    "AH", "FINALIZING",
                                    "failure: %s", nullptr != a_uri ? a_uri : ""
        );
        
        if ( nullptr == a_error || '\0' == a_error[0] ) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "JSONAPI replied with an empty response!");
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
        } else {
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(this, a_status, ctx_.response_.content_type_, a_error);
        }
        
    }
}

/**
 * @brief Called when the Gatekeeper deflects request to a job.
 *
 * @param a_tube
 * @param a_ttr
 * @param a_validity
 */
::ev::auth::route::Gatekeeper::DeflectorResult
 ngx::casper::broker::api::Module::OnOnGatekeeperDeflectToJob (const std::string& a_tube, ssize_t a_ttr, ssize_t a_valitity)
{
    ::ev::auth::route::Gatekeeper::DeflectorResult rv = {
        /* ttr_            */ ( a_ttr > 0 ? a_ttr : job_.ttr() ),
        /* validity_       */ ( a_valitity > 0 ? a_valitity : job_.expires_in() ),
        /* status_code_    */ NGX_HTTP_INTERNAL_SERVER_ERROR,
        /* status_message_ */ ""
    };
    const ngx_int_t post_rv = PostJob(ctx_.ngx_ptr_->method, uri_, nullptr != ctx_.request_.body_ ? ctx_.request_.body_ : "",
                                      a_tube,
                                      static_cast<ssize_t>(rv.ttr_),
                                      static_cast<ssize_t>(rv.validity_),
                                      access_token_
    );
    // ... set status properties ...
    if ( NGX_OK == post_rv ) {
        rv.status_code_    = NGX_HTTP_OK;
        rv.status_message_ = "job deflected!";
    } else {
        rv.status_code_    = ctx_.response_.status_code_;
        rv.status_message_ = "failed to post job!";
    }
    // ... we're done ...
    return rv;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Asynchronously, send a job to beanstalk and wait for response.
 *
 * @param a_method
 * @param a_urn
 * @param a_body
 * @param a_tube
 * @param a_ttr
 * @param a_validity
 * @param a_access_token
 */
ngx_int_t ngx::casper::broker::api::Module::PostJob (const ngx_int_t a_method,
                                                     const std::string& a_urn, const std::string& a_body,
                                                     const std::string& a_tube, const ssize_t a_ttr, const ssize_t a_validity,
                                                     const std::string& a_access_token)
{
    Json::Value body_object = Json::Value::null;
    if ( a_body.length() > 0 ) {
        if ( false == json_reader_.parse(a_body, body_object) ) {
            // ... an error occurred ...
            const auto errors = json_reader_.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw Json::Exception("An error occurred while parsing JSONAPI response: " +  errors[0].message + "!" );
            } else {
                throw Json::Exception("An error occurred while parsing JSONAPI response!");
            }
        }
    }
    
    Json::Value object = Json::Value(Json::ValueType::objectValue);
    object["id"]              = "";
    object["tube"]            = a_tube;
    object["ttr"]             = static_cast<Json::UInt64>(a_ttr);
    object["validity"]        = static_cast<Json::UInt64>(a_validity);
    object["payload"]         = Json::Value(Json::ValueType::objectValue);
    object["payload"]["urn"]  = a_urn;
    object["payload"]["tube"] = a_tube;
    object["payload"]["body"] = body_object;
    object["payload"]["__nginx_broker__"] = "nginx-broker//api";
    
    switch (a_method) {
        case NGX_HTTP_GET:
            object["payload"]["method"] = "GET";
            break;
        case NGX_HTTP_POST:
            object["payload"]["method"] = "POST";
            break;
        case NGX_HTTP_DELETE:
            object["payload"]["method"] = "DELETE";
            break;
        case NGX_HTTP_PATCH:
            object["payload"]["method"] = "PATCH";
            break;
        default:
            object["payload"]["method"] = nullptr;
            break;
    }
    object["payload"]["user_id"]          = json_api_.GetUserId();
    object["payload"]["entity_id"]        = json_api_.GetEntityId();
    object["payload"]["entity_schema"]    = json_api_.GetEntitySchema();
    object["payload"]["sharded_schema"]   = json_api_.GetShardedSchema();
    object["payload"]["subentity_schema"] = json_api_.GetSubentitySchema();
    object["payload"]["subentity_prefix"] = json_api_.GetSubentityPrefix();
    
    object["payload"]["access_token"]     = a_access_token;

    object["payload"]["__nginx_broker__"] = "nginx-broker//api";
    
    // ... we're done  ...
    return job_.Submit(object);
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
ngx_int_t ngx::casper::broker::api::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
{
    //
    // REJECT KEEP ALIVE
    //
    a_r->keepalive = 0;

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
    ngx_http_casper_broker_api_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_api_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_api_module);
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
        /* supported_content_types_ */ { "application/json", "application/json;charset=utf-8",  "application/vnd.api+json", "application/vnd.api+json;charset=utf-8" }
    };
    
    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_api_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::api::Errors configuration_errors(params.locale_, broker_conf->awful_conf.ordered_errors_json_serialization_enabled);
    
    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_api_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_api_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::api::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::api::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::api::Module::k_json_api_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::api::Module::k_json_api_content_type_w_charset_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [broker_conf] (const std::string& a_locale) {
            return new ngx::casper::broker::api::Errors(a_locale, broker_conf->awful_conf.ordered_errors_json_serialization_enabled);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ true,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &broker_conf, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::api::Module(config, params, *broker_conf, *loc_conf);
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
void ngx::casper::broker::api::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_api_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::api::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_api_module, a_data);
}
