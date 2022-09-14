/**
 * @file module_initializer.cc
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

#include "ngx/casper/broker/module.h"

#include "ngx/casper/broker/module/config.h"

#include "ngx/version.h"

#include "ngx/casper/broker/executor.h"
#include "ngx/casper/broker/initializer.h"
#include "ngx/casper/broker/tracker.h"
#include "ngx/casper/broker/i18.h"
#include "ngx/casper/broker/exception.h"

#include "ngx/casper/ev/glue.h"

#include "ev/ngx/bridge.h"

#include "osal/osal_time.h"

#include "http/ngx_http_core_module.h"
#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include <algorithm>
#include <string.h> // strcasestr

#include "ngx/ngx_utils.h"

const char* const ngx::casper::broker::Module::k_content_type_header_key_             = "Content-Type";

const char* const ngx::casper::broker::Module::k_service_id_key_lc_                   = "service_id";
const char* const ngx::casper::broker::Module::k_connection_validity_key_lc_          = "connection_validity";
const char* const ngx::casper::broker::Module::k_redis_ip_address_key_lc_             = "redis_ip_address";
const char* const ngx::casper::broker::Module::k_redis_port_number_key_lc_            = "redis_port_number";
const char* const ngx::casper::broker::Module::k_redis_database_key_lc_               = "redis_database";
const char* const ngx::casper::broker::Module::k_redis_max_conn_per_worker_lc_        = "redis_max_conn_per_worker";
const char* const ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_          = "postgresql_conn_str";
const char* const ngx::casper::broker::Module::k_postgresql_statement_timeout_lc_     = "postgresql_statement_timeout";
const char* const ngx::casper::broker::Module::k_postgresql_max_conn_per_worker_lc_   = "postgresql_max_conn_per_worker";
const char* const ngx::casper::broker::Module::k_postgresql_post_connect_queries_lc_  = "postgresql_post_connect_queries";
const char* const ngx::casper::broker::Module::k_postgresql_min_queries_per_conn_lc_  = "postgresql_min_queries_per_conn";
const char* const ngx::casper::broker::Module::k_postgresql_max_queries_per_conn_lc_  = "postgresql_max_queries_per_conn";
const char* const ngx::casper::broker::Module::k_curl_max_conn_per_worker_lc_         = "curl_max_conn_per_worker";
const char* const ngx::casper::broker::Module::k_beanstalkd_host_key_lc_              = "beanstalkd_host";
const char* const ngx::casper::broker::Module::k_beanstalkd_port_key_lc_              = "beanstalkd_port";
const char* const ngx::casper::broker::Module::k_beanstalkd_timeout_key_lc_           = "beanstalkd_timeout";
const char* const ngx::casper::broker::Module::k_beanstalkd_sessionless_tubes_key_lc_ = "beanstalkd_sessionless_tubes";
const char* const ngx::casper::broker::Module::k_beanstalkd_action_tubes_key_lc_      = "beanstalkd_action_tubes";
const char* const ngx::casper::broker::Module::k_session_cookie_name_key_lc_          = "session_cookie_name";
const char* const ngx::casper::broker::Module::k_session_cookie_domain_key_lc_        = "session_cookie_domain";
const char* const ngx::casper::broker::Module::k_session_cookie_path_key_lc_          = "session_cookie_path";
const char* const ngx::casper::broker::Module::k_gatekeeper_config_file_uri_key_lc_   = "gatekeeper_config_file";

const std::map<uint32_t, std::string> ngx::casper::broker::Module::k_http_methods_map_ = {
    { (uint32_t)NGX_HTTP_GET        , "GET",        },
    { (uint32_t)NGX_HTTP_HEAD       , "HEAD",       },
    { (uint32_t)NGX_HTTP_POST       , "POST",       },
    { (uint32_t)NGX_HTTP_PUT        , "PUT",        },
    { (uint32_t)NGX_HTTP_DELETE     , "DELETE",     },
    { (uint32_t)NGX_HTTP_MKCOL      , "MKCOL",      },
    { (uint32_t)NGX_HTTP_COPY       , "COPY",       },
    { (uint32_t)NGX_HTTP_MOVE       , "MOVE",       },
    { (uint32_t)NGX_HTTP_OPTIONS    , "OPTIONS",    },
    { (uint32_t)NGX_HTTP_PROPFIND   , "PROPFIND",   },
    { (uint32_t)NGX_HTTP_PROPPATCH  , "PROPPATCH",  },
    { (uint32_t)NGX_HTTP_LOCK       , "LOCK",       },
    { (uint32_t)NGX_HTTP_UNLOCK     , "UNLOCK",     },
    { (uint32_t)NGX_HTTP_PATCH      , "PATCH",      }
};

const std::map<uint16_t, std::string> ngx::casper::broker::Module::k_short_http_status_codes_map_ {
    /* 1XX */
    { /* 100 */ NGX_HTTP_CONTINUE                  , "continue"                 },
    { /* 101 */ NGX_HTTP_SWITCHING_PROTOCOLS       , "switching_protocols"      },
    { /* 102 */ NGX_HTTP_PROCESSING                , "processing"               },
    /* 2XX */
    { /* 200 */ NGX_HTTP_OK                        , "ok"                       },
    { /* 201 */ NGX_HTTP_CREATED                   , "created"                  },
    { /* 202 */ NGX_HTTP_ACCEPTED                  , "accepted"                 },
    { /* 204 */ NGX_HTTP_NO_CONTENT                , "no_content"               },
    { /* 206 */ NGX_HTTP_PARTIAL_CONTENT           , "partial_content"          },
    /* 3XX */
    { /* 300 */ NGX_HTTP_SPECIAL_RESPONSE          , "special_response"         },
    { /* 301 */ NGX_HTTP_MOVED_PERMANENTLY         , "moved_permanently"        },
    { /* 302 */ NGX_HTTP_MOVED_TEMPORARILY         , "moved_temporarily"        },
    { /* 303 */ NGX_HTTP_SEE_OTHER                 , "see_other"                },
    { /* 304 */ NGX_HTTP_NOT_MODIFIED              , "not_modified"             },
    { /* 307 */ NGX_HTTP_TEMPORARY_REDIRECT        , "temporary_redirect"       },
    { /* 308 */ NGX_HTTP_PERMANENT_REDIRECT        , "permanent_redirect"       },
    /* 4XX */
    { /* 400 */ NGX_HTTP_BAD_REQUEST               , "bad_request"              },
    { /* 401 */ NGX_HTTP_UNAUTHORIZED              , "unauthorized"             },
    { /* 403 */ NGX_HTTP_FORBIDDEN                 , "forbidden"                },
    { /* 404 */ NGX_HTTP_NOT_FOUND                 , "not_found"                },
    { /* 405 */ NGX_HTTP_NOT_ALLOWED               , "not_allowed"              },
    { /* 408 */ NGX_HTTP_REQUEST_TIME_OUT          , "request_time_out"         },
    { /* 409 */ NGX_HTTP_CONFLICT                  , "conflict"                 },
    { /* 411 */ NGX_HTTP_LENGTH_REQUIRED           , "length_required"          },
    { /* 412 */ NGX_HTTP_PRECONDITION_FAILED       , "precondition_failed"      },
    { /* 413 */ NGX_HTTP_REQUEST_ENTITY_TOO_LARGE  , "request_entity_too_large" },
    { /* 414 */ NGX_HTTP_REQUEST_URI_TOO_LARGE     , "request_uri_too_large"    },
    { /* 415 */ NGX_HTTP_UNSUPPORTED_MEDIA_TYPE    , "unsupported_media_type"   },
    { /* 416 */ NGX_HTTP_RANGE_NOT_SATISFIABLE     , "range_not_satisfiable"    },
    { /* 421 */ NGX_HTTP_MISDIRECTED_REQUEST       , "misdirected_request"      },
    { /* 429 */ NGX_HTTP_TOO_MANY_REQUESTS         , "too_many_requests"        },
    { /* 444 */ NGX_HTTP_CLOSE                     , "close"                    },
    /* 5XX */
    { /* 500 */ NGX_HTTP_INTERNAL_SERVER_ERROR     , "internal_server_error"    },
    { /* 501 */ NGX_HTTP_NOT_IMPLEMENTED           , "not_implemented"          },
    { /* 502 */ NGX_HTTP_BAD_GATEWAY     		   , "bad_gateway"              },
    { /* 503 */ NGX_HTTP_SERVICE_UNAVAILABLE       , "service_unavailable"      },
    { /* 504 */ NGX_HTTP_GATEWAY_TIME_OUT          , "gateway_time_out"         },
    { /* 507 */ NGX_HTTP_INSUFFICIENT_STORAGE      , "insufficient_storage"     }
};

#undef ngx_http_casper_broker_log_msg
#define ngx_http_casper_broker_log_msg(a_r, a_level, ...) \
    ngx_log_error(a_level, a_r->connection->log, /* error code */ 0, __VA_ARGS__);

/**
 * @brief Default constructor.
 *
 * @param a_name
 * @param a_config
 * @param a_params
 */
ngx::casper::broker::Module::Module (const char* const a_name, const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params)
    : loggable_data_{
        /* owner_ptr */ this,
        /* ip_addr_  */ std::string(reinterpret_cast<char const*>(a_config.ngx_ptr_->connection->addr_text.data), a_config.ngx_ptr_->connection->addr_text.len),
        /* module_   */ a_name,
        /* tag_      */ std::to_string(reinterpret_cast<intptr_t>(this))
    },
    logger_client_(nullptr),
    ctx_
    {
        /* ngx_module_ */ a_config.ngx_module_,
        /* ngx_ptr_    */ a_config.ngx_ptr_,
        /* request_    */
        {
            /* uri_                  */ std::string((const char*)a_config.ngx_ptr_->unparsed_uri.data, a_config.ngx_ptr_->unparsed_uri.len),
            /* method_                */ ngx::casper::broker::Module::GetMethodName(a_config.ngx_ptr_),
            /* locale_                */ a_params.locale_,
            /* location_              */ "",
            /* content_type_          */ a_config.rx_content_type_,
            /* headers_               */ {},
            /* body_                  */ nullptr,
            /* body_interceptor_      */ {
                /* dst_    */ nullptr,
                /* writer_ */ nullptr,
                /* flush_  */ nullptr,
                /* close_  */ nullptr
            },
            /* browser_               */ "",
            /* ngx_body_read_handler_ */ a_config.ngx_body_read_handler_,
            /* body_read_completed_   */ nullptr,
            /* connection_validity_   */ 0,
            /* session_cookie_ */
            {
                /* name_      */ "",
                /* value_     */ "",
                /* domain_    */ "",
                /* path_      */ "",
                /* expires_   */ "",
                /* secure_    */ false,
                /* http_only_ */ true,
                /* inline_    */ ""
            }
        },
        /* response_ */
        {
            /* headers_            */ {},
            /* content_type_       */ a_config.tx_content_type_,
            /* body_               */ "",
            /* length_             */ 0,
            /* binary_             */ false,
            /* status_code_        */ NGX_HTTP_INTERNAL_SERVER_ERROR,
            /* return_code_        */ NGX_OK ,
            /* redirect_           */
            {
                /* protocol_     */ "",
                /* host_         */ "",
                /* port_         */ 80,
                /* path_         */ "",
                /* file_         */ "",
                /* uri_          */ "",
                /* internal_     */ false,
                /* asynchronous_ */ false,
                /* location_     */ "",
                /* supported_methods_ */ {
                    NGX_HTTP_GET
                }
            },
            /* landing_page_url_   */ a_config.landing_page_url_,
            /* error_page_url_     */ a_config.error_page_url_,
            /* errors_tracker_     */ { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },
            /* asynchronous_       */ false,
            /* serialize_errors_   */ false
        },
        /* log_token_          */ a_config.log_token_,
        /* log_level_          */ NGX_LOG_DEBUG,
        /* at_rewrite_handler_ */ a_config.at_rewrite_handler_,
        /* at_content_handler_ */ false,
        /* cycle_cnt_          */ 0,
        /* errors_ptr_         */ nullptr,
        /* logger_data_ref_    */ loggable_data_,
        /* log_body_           */ true
    }
{
    ctx_.errors_ptr_ = ngx::casper::broker::Tracker::GetInstance().errors_ptr(ctx_.ngx_ptr_);
    //
    // ... REQUEST ...
    //
    ctx_.request_.connection_validity_ = static_cast<size_t>(std::atoi(a_params.config_.find(ngx::casper::broker::Module::k_connection_validity_key_lc_)->second.c_str()));

    const std::map<std::string, std::string*> map = {
        { ngx::casper::broker::Module::k_session_cookie_name_key_lc_  , &ctx_.request_.session_cookie_.name_   },
        { ngx::casper::broker::Module::k_session_cookie_domain_key_lc_, &ctx_.request_.session_cookie_.domain_ },
        { ngx::casper::broker::Module::k_session_cookie_path_key_lc_  , &ctx_.request_.session_cookie_.path_   },
    };
    for ( auto entry : map ) {
        const auto it = a_params.config_.find(entry.first);
        if ( a_params.config_.end() != it ) {
            (*entry.second) = it->second;
        }
    }
    
    //
    // ... RESPONSE ...
    //
    ctx_.response_.errors_tracker_ = {
        /* add_i18n_ */
        [this] (const char* const a_error_code, const uint16_t a_http_status_code, const char* const a_i18n_key) {
            ctx_.response_.status_code_ = a_http_status_code;
            ctx_.errors_ptr_->Track(a_error_code, a_http_status_code, a_i18n_key);
        },
        /* add_i18n_v_ */
        [this] (const char* const a_error_code, const uint16_t a_http_status_code, const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count) {
            ctx_.response_.status_code_ = a_http_status_code;
            ctx_.errors_ptr_->Track(a_error_code, a_http_status_code, a_i18n_key, a_args, a_args_count);
        },
        /* add_i18n_vi_ */
        [this] (const char* const a_error_code, const uint16_t a_http_status_code, const char* const a_i18n_key,
                                   U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count,
                                   const char* const a_internal_error_msg)
        {
            ctx_.response_.status_code_ = a_http_status_code;
            ctx_.errors_ptr_->Track(a_error_code, a_http_status_code, a_i18n_key, a_args, a_args_count, a_internal_error_msg);
        },
        /* add_i18n_i_ */
        [this] (const char* const a_error_code, const uint16_t a_http_status_code, const char* const a_i18n_key, const char* const a_internal_error_msg) {
            ctx_.response_.status_code_ = a_http_status_code;
            ctx_.errors_ptr_->Track(a_error_code, a_http_status_code, a_i18n_key, a_internal_error_msg);
        },
        /* i18n_jsonify_ */
        [this] (Json::Value& o_object) {
            ctx_.errors_ptr_->jsonify(o_object);
        },
        /* i18n_reset_ */
        [this] () {
            ctx_.errors_ptr_->Reset();
        }    
    };

    ctx_.response_.asynchronous_     = false;
    ctx_.response_.serialize_errors_ = a_config.serialize_errors_;
    
    // ... copy headers ...
    for ( auto it : a_params.in_headers_ ) {
        ctx_.request_.headers_[it.first] = it.second;
    }
    // ... grab request actual Content-Type ...
    const auto content_type_it = ctx_.request_.headers_.find("content-type");
    if ( ctx_.request_.headers_.end() != content_type_it ) {
         ctx_.request_.content_type_ = content_type_it->second;
    }

    // ... set browser ...
    if ( ctx_.ngx_ptr_->headers_in.msie ) {
        ctx_.request_.browser_ = "msie";
    } else if ( ctx_.ngx_ptr_->headers_in.msie6 ) {
        ctx_.request_.browser_ = "msie6";
    } else if ( ctx_.ngx_ptr_->headers_in.opera ) {
        ctx_.request_.browser_ = "opera";
    } else if ( ctx_.ngx_ptr_->headers_in.gecko ) {
        ctx_.request_.browser_ = "gecko";
    } else if ( ctx_.ngx_ptr_->headers_in.chrome ) {
        ctx_.request_.browser_ = "chrome";
    } else if ( ctx_.ngx_ptr_->headers_in.safari ) {
        ctx_.request_.browser_ = "safari";
    }
    
    // ... setup executer ...    
    executor_ = nullptr != a_config.executor_factory_ ? a_config.executor_factory_(ctx_.response_.errors_tracker_) : nullptr;

    // ... sanity check point ...
    OSALITE_ASSERT(true == ngx::casper::broker::Tracker::GetInstance().IsRegistered(ctx_.ngx_ptr_));

    // ... set ngx context ...
    ngx_http_set_ctx(ctx_.ngx_ptr_, this, ctx_.module_);

    // ... track service id ...
    service_id_ = ::ngx::casper::ev::Glue::GetInstance().ServiceID();
        
    // ...
    body_read_supported_methods_ = {
        NGX_HTTP_POST, NGX_HTTP_DELETE, NGX_HTTP_PATCH
    };
    body_read_bypass_methods_ = {
        NGX_HTTP_GET
    };    
    body_read_allow_empty_methods_ = {
      NGX_HTTP_DELETE
    };

    // ... ensure permanent logger ...
    logger_client_ = new ::ev::LoggerV2::Client(loggable_data_);

    // ... track module instance ...
    if ( true == NRS_NGX_CASPER_BROKER_MODULE_CC_MODULES_LOGS_ENABLED ) {
      ::ev::LoggerV2::GetInstance().Register(logger_client_, { "cc-modules", "cc-modules-debug", loggable_data_.module() });
    }
    
    // ... log module data ...
    ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",name=%s,version=%s",
                                      "ST : MODULE",
                                      a_name, NGX_VERSION
    );
    
    // ... output in headers ...
    std::map<std::string, std::string> kv = ctx_.request_.headers_;
    kv["uri"]    = ctx_.request_.uri_.c_str();
    kv["method"] = ctx_.request_.method_.c_str();
    
    for ( auto header : { "user-agent", "host", "uri", "method", "content-type", "content-length", "connection", "accept" } ) {
        const auto it = kv.find(header);
        if ( kv.end() == it ) {
            continue;
        }
        std::string key = header;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                          ( "IN : " + key ).c_str(), it->second.c_str()
        );
        kv.erase(it);
    }
    for ( auto it : kv ) {
        ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s=%s",
                                          "IN : HEADER", it.first.c_str(), it.second.c_str()
        );
    }
    ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",count=" SIZET_FMT,
                                      "ST : CONSTRUCTED",
                                      ::ev::LoggerV2::GetInstance().Count(loggable_data_.module())
     );
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
    
    ngx::casper::broker::Tracker::GetInstance().Bind(a_config.ngx_ptr_,
                                                     [this] (const std::string& a_what, const std::string& a_message) {
                                                        ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules-debug",
                                                                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                                                                          ( "DBG : " + a_what ).c_str(),
                                                                                          a_message.c_str()
                                                        );
                                                     }
    );
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::Module::~Module ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);

    if ( nullptr != executor_ ) {
        delete executor_;
    }
    if ( nullptr != ctx_.request_.body_ ) {
        delete [] ctx_.request_.body_;
    }
    // ... write to permanent log ...
    ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",count=" SIZET_FMT,
                                      "ST : DESTROYED",
                                      ( ::ev::LoggerV2::GetInstance().Count(loggable_data_.module()) - 1 )
    );
    // ... untrack module instance ...
    ::ev::LoggerV2::GetInstance().Unregister(logger_client_);

    // ... dispose logger ...
    delete logger_client_;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief 'Pre-run' setup.
 *
 * @return NGX_OK on success, on failure NGX_ERROR.
 */
ngx_int_t ngx::casper::broker::Module::Setup ()
{
    if ( true == ngx::casper::broker::Tracker::GetInstance().ContainsErrors(ctx_.ngx_ptr_) ) {
        return NGX_ERROR;
    }
    //
    // READ optional ( per request ) configuration
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }
    //
    // [ OPTIONAL ] directive - location mark
    //
    ctx_.request_.location_ = std::string(reinterpret_cast<const char*>(broker_conf->location.data), broker_conf->location.len);
    // ... done ...
    return NGX_OK;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Read request body ( if needed )
 *
 * @param a_callback
 *
 * @return
 */
ngx_int_t ngx::casper::broker::Module::ValidateRequest (std::function<void()> a_callback)
{
    // ... expecting body?
    if ( body_read_supported_methods_.end() !=  body_read_supported_methods_.find(ctx_.ngx_ptr_->method) ) {
        
        const auto validate_body = [this, a_callback] () {
            const bool from_asyn_read = ( NGX_AGAIN == ctx_.response_.return_code_ );
            // ... assuming good reading ...
            ctx_.response_.return_code_ = NGX_OK;
            // ... body was entirely read or an error occurred?
            if ( NGX_OK != ctx_.response_.return_code_ ) {
                if ( false == ngx::casper::broker::Tracker::GetInstance().ContainsErrors(ctx_.ngx_ptr_) ) {
                    const std::string internal = "NGX error " + std::to_string(ctx_.response_.return_code_) + " while reading client body!";
                    ctx_.response_.errors_tracker_.add_i18n_i_("bad_request", ctx_.response_.status_code_,
                                                               "BROKER_MISSING_OR_INVALID_BODY_ERROR", internal.c_str()
                     );
                }
                ctx_.response_.return_code_ = NGX_ERROR;
                ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
            } else if ( true == ngx::casper::broker::Tracker::GetInstance().ContainsErrors(ctx_.ngx_ptr_) ) {
                ctx_.response_.return_code_ = NGX_ERROR;
            }
            // ... empty body ? allowed?
            if ( nullptr == ctx_.request_.body_interceptor_.writer_ && nullptr == ctx_.request_.body_ && body_read_allow_empty_methods_.end() == body_read_allow_empty_methods_.find(ctx_.ngx_ptr_->method) ) {
                ctx_.response_.errors_tracker_.add_i18n_("bad_request", ctx_.response_.status_code_,
                                                         "BROKER_MISSING_OR_INVALID_BODY_ERROR"
                );
                ctx_.response_.return_code_ = NGX_ERROR;
                ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
            }
            
            // ... log
            NGX_BROKER_MODULE_DEBUG_LOG(ctx_.module_, ctx_.ngx_ptr_, ctx_.log_token_.c_str(),
                                        "VR", "BODY",
                                        "%s", nullptr != ctx_.request_.body_ ? ctx_.request_.body_ : ""
            );
            
            // ... notify caller ...
            if ( true == from_asyn_read ) {
                a_callback();
            }
            
        };
        
        ctx_.response_.return_code_ = ngx_http_read_client_request_body(ctx_.ngx_ptr_, ctx_.request_.ngx_body_read_handler_);
        if ( NGX_AGAIN == ctx_.response_.return_code_ ) {
            // ... track callback ...
            ctx_.request_.body_read_completed_ = [a_callback, validate_body] () {
                validate_body();
            };
            ngx_int_t rc;
            // ... signal async ...
            const char* handler;
            if ( true == ctx_.at_rewrite_handler_ ) {
                handler = "rewrite";
                rc      = NGX_AGAIN;
            } else {
                handler = ( true == ctx_.at_content_handler_ ? "content" : "???" );
                rc      = NGX_DONE;
            }
            // ... log ...
            ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modules",
                                              NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",at %s - async reading ( %2ld )",
                                              "ST : BODY",
                                              handler, rc
            );
            // ... we're done ...
            return rc;
        }
        
        validate_body();
        
    } else if ( body_read_bypass_methods_.end() == body_read_bypass_methods_.find(ctx_.ngx_ptr_->method) ) {
        const auto method_it = ngx::casper::broker::Module::k_http_methods_map_.find((uint32_t)ctx_.ngx_ptr_->method);
        if ( ngx::casper::broker::Module::k_http_methods_map_.end() != method_it ) {
            U_ICU_NAMESPACE::Formattable args[] = {
                method_it->second.c_str()
            };
            ctx_.response_.errors_tracker_.add_i18n_v_("bad_request", ctx_.response_.status_code_,
                                                       "BROKER_UNSUPPORTED_METHOD_ERROR", args, 1
            );
        } else {
            U_ICU_NAMESPACE::Formattable args[] = {
                (int32_t)ctx_.ngx_ptr_->method
            };
            ctx_.response_.errors_tracker_.add_i18n_v_("bad_request", ctx_.response_.status_code_,
                                                       "BROKER_UNSUPPORTED_METHOD_ERROR", args, 1
            );
        }
        ctx_.response_.return_code_ = NGX_ERROR;
        ctx_.response_.status_code_ = NGX_HTTP_NOT_ALLOWED;
    }

    // ... we're done ...
    return ctx_.response_.return_code_;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ngx::casper::broker::Module::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Common module initializer.
 *
 * @param a_data
 * @param a_params
 * @param a_module
 */
ngx_int_t ngx::casper::broker::Module::Initialize (const ngx::casper::broker::Module::Config& a_config,
                                                   ngx::casper::broker::Module::Params& a_params,
                                                   const std::function<::ngx::casper::broker::Module*()>& a_module)
{
    //
    // GRAB 'MAIN' CONFIG
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_config.ngx_ptr_, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }
    
    const std::string uri = std::string((const char*)a_config.ngx_ptr_->unparsed_uri.data, a_config.ngx_ptr_->unparsed_uri.len);

    //
    // TRACK REQUEST - TODO: TRACK W/ LOCALE ONLY ONCE
    //
    ngx::casper::broker::Errors* errors_ptr = a_config.errors_factory_(a_params.locale_);
    
    ngx::casper::broker::Tracker::GetInstance().Register(a_config.ngx_ptr_, errors_ptr);
    
    // ... logging purposes ...
    NGX_BROKER_MODULE_DEBUG_LOG(a_config.ngx_module_, a_config.ngx_ptr_, a_config.log_token_.c_str(),
                                "IT", "URI",
                                "%s", uri.c_str()
    );
    const auto method_it = ngx::casper::broker::Module::k_http_methods_map_.find((uint32_t)a_config.ngx_ptr_->method);
    if ( ngx::casper::broker::Module::k_http_methods_map_.end() == method_it ) {
        NGX_BROKER_MODULE_DEBUG_LOG(a_config.ngx_module_, a_config.ngx_ptr_, a_config.log_token_.c_str(),
                                    "IT", "METHOD",
                                    "%u", a_config.ngx_ptr_->method
        );
    } else {
        NGX_BROKER_MODULE_DEBUG_LOG(a_config.ngx_module_, a_config.ngx_ptr_, a_config.log_token_.c_str(),
                                    "IT", "METHOD",
                                    "%s", method_it->second.c_str()
        );
    }
    
    ngx_int_t ngx_return_code  = NGX_OK;
    uint16_t  http_status_code = NGX_HTTP_BAD_REQUEST;
    
    //
    // COMMON CONFIG DATA
    //
    // ... resources dir ...
    if ( 0 == broker_conf->resources_dir.len ) {
        U_ICU_NAMESPACE::Formattable args[] = {
            "",
            "nginx_casper_broker_resources_dir"
        };
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        errors_ptr->Track("config_error", http_status_code,
                          "BROKER_MODULE_CONFIG_MISSING_OR_INVALID_DIRECTIVE_VALUE_ERROR",
                          args, 2
        );
    }
    
    a_params.config_[ngx::casper::broker::Initializer::k_resources_dir_key_lc_]
        = std::string(reinterpret_cast<char const*>(broker_conf->resources_dir.data), broker_conf->resources_dir.len);
    
    //
    // ... CONNECTION ...
    //
    a_params.config_[ngx::casper::broker::Module::k_connection_validity_key_lc_]
        = std::to_string(broker_conf->connection_validity);
    
    //
    // ONE-SHOT INITIALIZATION - first request will be slower that the other ones.
    //                           due to sockets and thread(s) initialization
    if ( false == ngx::casper::broker::Initializer::IsInitialized() ) {
        
        //
        // ... SERVICE ...
        //
        a_params.config_[ngx::casper::broker::Module::k_service_id_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->service_id.data), broker_conf->service_id.len);
        
        //
        // ... REDIS ....
        //
        
        // ... host ...
        if ( 0 == broker_conf->redis_ip_address.len ) {
            U_ICU_NAMESPACE::Formattable args[] = {
                "",
                "nginx_casper_broker_redis_ip_address"
            };
            http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            errors_ptr->Track("config_error", http_status_code,
                              "BROKER_MODULE_CONFIG_MISSING_OR_INVALID_DIRECTIVE_VALUE_ERROR",
                              args, 2
            );
        }
        a_params.config_[ngx::casper::broker::Module::k_redis_ip_address_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->redis_ip_address.data), broker_conf->redis_ip_address.len);
        
        // ... port ...
        a_params.config_[ngx::casper::broker::Module::k_redis_port_number_key_lc_]
            = std::to_string(broker_conf->redis_port_number);
        
        // ... database ...
        if ( -1 != broker_conf->redis_database ) {
            a_params.config_[ngx::casper::broker::Module::k_redis_database_key_lc_]
                = std::to_string(broker_conf->redis_database);
        }
        
        a_params.config_[ngx::casper::broker::Module::k_redis_max_conn_per_worker_lc_]
            = std::to_string(broker_conf->redis_max_conn_per_worker);
        
        //
        // ... POSTGRESQL ...
        //
        if ( 0 == broker_conf->postgresql_conn_str.len ) {
            U_ICU_NAMESPACE::Formattable args[] = {
                "",
                "nginx_casper_broker_postgresql_connection_string"
            };
            http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            errors_ptr->Track("config_error", http_status_code,
                              "BROKER_MODULE_CONFIG_MISSING_OR_INVALID_DIRECTIVE_VALUE_ERROR",
                              args, 2
            );
        }
        a_params.config_[ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->postgresql_conn_str.data), broker_conf->postgresql_conn_str.len);

        a_params.config_[ngx::casper::broker::Module::k_postgresql_statement_timeout_lc_]
            = std::to_string(broker_conf->postgresql_statement_timeout);

        a_params.config_[ngx::casper::broker::Module::k_postgresql_max_conn_per_worker_lc_]
            = std::to_string(broker_conf->postgresql_max_conn_per_worker);
        
        if ( broker_conf->postgresql_post_connect_queries.len > 0 ) {
            a_params.config_[ngx::casper::broker::Module::k_postgresql_post_connect_queries_lc_]
                = std::string(reinterpret_cast<char const*>(broker_conf->postgresql_post_connect_queries.data), broker_conf->postgresql_post_connect_queries.len);
        }
        
        a_params.config_[ngx::casper::broker::Module::k_postgresql_min_queries_per_conn_lc_]
            = std::to_string(broker_conf->postgresql_min_queries_per_conn);
        
        a_params.config_[ngx::casper::broker::Module::k_postgresql_max_queries_per_conn_lc_]
            = std::to_string(broker_conf->postgresql_max_queries_per_conn);
        
        //
        // ... CURL ...
        //
        a_params.config_[ngx::casper::broker::Module::k_curl_max_conn_per_worker_lc_]
            = std::to_string(broker_conf->curl_max_conn_per_worker);
        
        //
        // ... BEANSTALKD ...
        //
        a_params.config_[ngx::casper::broker::Module::k_beanstalkd_host_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->beanstalkd.host.data), broker_conf->beanstalkd.host.len);
        a_params.config_[ngx::casper::broker::Module::k_beanstalkd_port_key_lc_]
           = std::to_string(broker_conf->beanstalkd.port);
        a_params.config_[ngx::casper::broker::Module::k_beanstalkd_timeout_key_lc_]
           = std::to_string(broker_conf->beanstalkd.timeout);
        const std::map<const char* const, const ngx_str_t*> beanstalk_conf_strings_map = {
            { ngx::casper::broker::Module::k_beanstalkd_sessionless_tubes_key_lc_, &broker_conf->beanstalkd.tubes.sessionless },
            { ngx::casper::broker::Module::k_beanstalkd_action_tubes_key_lc_     , &broker_conf->beanstalkd.tubes.action      }
        };
        for ( auto bcsm_it : beanstalk_conf_strings_map ) {
            if ( bcsm_it.second->len > 0 ) {
                a_params.config_[bcsm_it.first] = std::string(reinterpret_cast<char const*>(bcsm_it.second->data), bcsm_it.second->len);
            }
        }
        
        //
        // ... GATEKEEPER ...
        //
        a_params.config_[ngx::casper::broker::Module::k_gatekeeper_config_file_uri_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->gatekeeper.config_file_uri.data), broker_conf->gatekeeper.config_file_uri.len);
        
        //
        // ... THEADING ...
        //
        try {
            ngx::casper::broker::Initializer::GetInstance().Startup(a_config.ngx_ptr_, a_params.config_);
        } catch (const ngx::casper::broker::Exception& a_broker_exception) {
            errors_ptr->Track("config_error", http_status_code,
                              a_broker_exception.code(), a_broker_exception.what()
            );
        }
        
        //
        // ... CLEANUP ...
        //
        // ... after initialization, config map only needs resources dir ....
        const auto keys_to_erase = {
            ngx::casper::broker::Module::k_redis_ip_address_key_lc_,
            ngx::casper::broker::Module::k_redis_port_number_key_lc_,
            ngx::casper::broker::Module::k_redis_database_key_lc_,
            ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_
        };
        for ( auto key : keys_to_erase ) {
            const auto it = a_params.config_.find(key);
            if ( a_params.config_.end() != it ) {
                a_params.config_.erase(it);
            }
        }
    }
    
    //
    // ... SESSION COOKIE ...
    //
    if ( broker_conf->session_cookie_name.len > 0 ) {
        a_params.config_[ngx::casper::broker::Module::k_session_cookie_name_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->session_cookie_name.data), broker_conf->session_cookie_name.len);
    }
    if ( broker_conf->session_cookie_domain.len > 0 ) {
        a_params.config_[ngx::casper::broker::Module::k_session_cookie_domain_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->session_cookie_domain.data), broker_conf->session_cookie_domain.len);
    }
    if ( broker_conf->session_cookie_path.len > 0 ) {
        a_params.config_[ngx::casper::broker::Module::k_session_cookie_path_key_lc_]
            = std::string(reinterpret_cast<char const*>(broker_conf->session_cookie_path.data), broker_conf->session_cookie_path.len);
    }
    
    //
    // REQUEST / EXECUTOR SETUP
    //
    
    const auto async_or_finalize = [] (ngx::casper::broker::Module* a_module_ptr, uint16_t a_http_status_code, ngx_int_t a_ngx_return_code, bool a_force) -> ngx_int_t {
                
        if ( NGX_DECLINED == a_module_ptr->ctx_.response_.return_code_ ) {
            return a_module_ptr->ctx_.response_.return_code_;
        }
        
        // ... if it will be an asynchronous response ...
        if ( NGX_AGAIN == a_module_ptr->ctx_.response_.return_code_ || true == a_module_ptr->ctx_.response_.asynchronous_ ) {
            a_module_ptr->ctx_.cycle_cnt_++;
            // ... we're done for now ...
            return a_module_ptr->ctx_.response_.return_code_;
        }
        
        // .. we're done and we've got a response ready ...
        return ngx::casper::broker::Module::FinalizeRequest(a_module_ptr, &a_http_status_code, &a_ngx_return_code, a_force);

    };
    
    ngx::casper::broker::Module* module_ptr = nullptr;

    try {
        
        // ... 'module' WILL BE RELEASED BY CLEANUP CALL! ...
        module_ptr = a_module();
                
        // ... enable or disable module body data logging ...
        if ( broker_conf->cc_log.set ) {
            module_ptr->ctx_.log_body_ = broker_conf->cc_log.write_body;
        }

        //
        // INSTALL CLEAN-UP HANDLER
        //
        // ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️
        // ☠️   !!!!! CLEANUP HANDLE MUST BE INSTALLED !!!!!  ☠️
        // ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️
        //
        ngx_http_cleanup_t* cleanup = ngx_http_cleanup_add(a_config.ngx_ptr_, 0);
        assert(nullptr != cleanup);
        cleanup->handler = a_config.ngx_cleanup_handler_;
        cleanup->data    = module_ptr;

        // ... 'pre-run' setup ...
        const ngx_int_t setup_rc = module_ptr->Setup();
        if ( NGX_OK != setup_rc ) {
            return ngx::casper::broker::Module::FinalizeWithErrors(module_ptr->ctx_.module_, module_ptr->ctx_.ngx_ptr_,
                                                                   /* a_headers */ nullptr,
                                                                   module_ptr->ctx_.response_.status_code_, setup_rc,
                                                                   module_ptr->ctx_.response_.serialize_errors_, /* a_force */ false,
                                                                   module_ptr->ctx_.log_token_
            );
        }

        // ... take control of 'ngx_ptr_' life cycle ...
        a_config.ngx_ptr_->casper_request = 1;
        
        // ... validate request & read body ...
        const ngx_int_t vr_rc = module_ptr->ValidateRequest([async_or_finalize, module_ptr]() {

            // ... we're ready to run now  ...
            ngx_int_t validate_ngx_return_code  = module_ptr->Run();
            uint16_t  validate_http_status_code = module_ptr->ctx_.response_.status_code_;
            
            ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(
                                                                       /* a_client */
                                                                       module_ptr,
                                                                       /* a_ms */
                                                                       10,
                                                                       /* a_callback */
                                                                       [module_ptr, async_or_finalize, validate_ngx_return_code, validate_http_status_code] () {
                                                                           async_or_finalize(module_ptr, validate_http_status_code, validate_ngx_return_code, /* a_force */ true);
                                                                       }
           );

        });
        
        // ... validation or body reading pending?
        if ( NGX_AGAIN == module_ptr->ctx_.response_.return_code_ ) {
            // ... yes ...
            return vr_rc;
        }
                
        // ... we're ready to run now  ...
        ngx_return_code  = module_ptr->Run();
        http_status_code = module_ptr->ctx_.response_.status_code_;

    } catch (const ::ev::Exception& a_ev_exception) {
        ngx_return_code  = NGX_ERROR;
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        errors_ptr->Track("server_error", http_status_code,
                          "BROKER_CPP_EXCEPTION_CAUGHT",
                          a_ev_exception.what()
        );
    } catch (const std::bad_alloc& a_bad_alloc) {
        ngx_return_code  = NGX_ERROR;
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        errors_ptr->Track("server_error", http_status_code,
                          "BROKER_CPP_EXCEPTION_CAUGHT",
                          a_bad_alloc.what()
        );
    } catch (const std::runtime_error& a_rte) {
        ngx_return_code  = NGX_ERROR;
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        errors_ptr->Track("server_error", http_status_code,
                          "BROKER_CPP_EXCEPTION_CAUGHT",
                          a_rte.what()
        );
    } catch (const std::exception& a_std_exception) {
        ngx_return_code  = NGX_ERROR;
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        errors_ptr->Track("server_error", http_status_code,
                          "BROKER_CPP_EXCEPTION_CAUGHT",
                          a_std_exception.what()
        );
    } catch (...) {
        ngx_return_code  = NGX_ERROR;
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        
        const std::string ierror_str = STD_CPP_GENERIC_EXCEPTION_TRACE();
        errors_ptr->Track("server_error", http_status_code,
                          "BROKER_CPP_EXCEPTION_CAUGHT",
                          ierror_str.c_str()
        );
    }

    NGX_BROKER_MODULE_DEBUG_LOG(a_config.ngx_module_, a_config.ngx_ptr_, a_config.log_token_.c_str(),
                                "IT", "CONTEXT",
                                "%s", ( nullptr != module_ptr ? "new context created..." : "NO context was created..." )
    );
    
    // ... we're done ...
    if ( nullptr == module_ptr ) {
        // ... with errors - module was not created !
        return ngx::casper::broker::Module::FinalizeWithErrors(a_config.ngx_module_, a_config.ngx_ptr_,
                                                               /* a_headers */ nullptr,
                                                               http_status_code, ngx_return_code,
                                                               a_config.serialize_errors_, /* a_force */ false,
                                                               a_config.log_token_
        );
    }
    
    return async_or_finalize(module_ptr, http_status_code, ngx_return_code, /* a_force */ false);
}

/**
 * @brief Read request body.
 *
 * @param a_module
 * @param a_r
 */
void ngx::casper::broker::Module::ReadBody (ngx_module_t& a_module, ngx_http_request_t* a_r)
{
    // ... no context?
    ngx::casper::broker::Module* module = (ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module);
    if ( nullptr == module ) {
        // ... no body!
        return; 
    }
    
    // ... nothing to read?
    if ( NULL == a_r->request_body || NULL == a_r->request_body->bufs ) {
        // ... write to permanent log ...
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                          "IN : BODY",
                                          true == module->ctx_.log_body_
                                            ? ""
                                            : "<filtered>"
        );
        // ... close?
        if ( nullptr != module->ctx_.request_.body_interceptor_.close_ ) {
            if ( 0 != module->ctx_.request_.body_interceptor_.close_() ) {
                module->ctx_.errors_ptr_->Track("server_error", NGX_HTTP_INTERNAL_SERVER_ERROR,
                                                "BROKER_INTERNAL_ERROR",
                                                "An error occurred while intercepting body!"
                );
            }
        }
        // ... no body!
        if ( nullptr != module->ctx_.request_.body_read_completed_ ) {
            module->ctx_.request_.body_read_completed_();
            module->ctx_.request_.body_read_completed_ = nullptr;
        }
        if ( 0 == a_r->request_body_no_buffering && a_r->main->count > 1 ) {
            a_r->main->count--;
        }
        // ... we're done here ....
        return;
    }

    bool interceptor_error = false;

    ngx_chain_t* chain = a_r->request_body->bufs;

    // read from file?
    if ( chain->buf->in_file ) {
        u_char  buffer [1024];
        off_t   offset   = 0;
        ssize_t n        = 0;
        size_t  max_size = 0;
        bool    r_f_fd   = false;
        //  ... read from file ...
        const std::string file_uri = std::string(reinterpret_cast<char const*>(chain->buf->file->name.data), chain->buf->file->name.len);
        // ... get file size ...
        struct stat stat_info;
        if ( stat(file_uri.c_str(), &stat_info) == 0 ) {
            // exists?
            if ( S_ISREG(stat_info.st_mode) != 0 ) {
                // set size
                max_size = static_cast<size_t>(stat_info.st_size);
            } else {
                module->ctx_.errors_ptr_->Track("server_error", NGX_HTTP_INTERNAL_SERVER_ERROR,
                                                "BROKER_INTERNAL_ERROR",
                                                "Unable to read body from file - it doesn't exist!"
                );
            }
        } else {
            // ... directly from fd ...
            r_f_fd = true;
        }
        // ... prepare body buffer or using fw?
        if ( nullptr == module->ctx_.request_.body_interceptor_.writer_ ) {
            if ( nullptr != module->ctx_.request_.body_ ) {
                delete [] module->ctx_.request_.body_;
            }
            module->ctx_.request_.body_ = new char [max_size + 1];
            module->ctx_.request_.body_[0] = '\0';
        } /* else {} - using fw  */
        // ... read it ...
        const size_t max_read = sizeof(buffer) / sizeof(buffer[0]);
        if ( true == r_f_fd ) {
            // ... from fd - no size info available ...
            if ( nullptr != module->ctx_.request_.body_interceptor_.writer_ ) {
                while ( ( n = ngx_read_file(chain->buf->file, buffer, max_read, offset) ) > 0 ) {
                    if ( n != module->ctx_.request_.body_interceptor_.writer_(buffer, static_cast<size_t>(n)) ) {
                        interceptor_error = true;
                        break;
                    }
                    offset += n;
                }
            } else {
                while ( ( n = ngx_read_file(chain->buf->file, buffer, max_read, offset) ) > 0 ) {
                    if ( max_size < static_cast<size_t>(n) ) {
                        size_t swp_size   = static_cast<size_t>(offset) + static_cast<size_t>(n);
                        char*  swp_buffer = new char[swp_size + 1];
                        memcpy(swp_buffer, module->ctx_.request_.body_, static_cast<size_t>(offset));
                        delete [] module->ctx_.request_.body_;
                        module->ctx_.request_.body_ = swp_buffer;
                        max_size += static_cast<size_t>(n);
                    }
                    memcpy(module->ctx_.request_.body_ + offset, reinterpret_cast<char const*>(buffer), static_cast<size_t>(n));
                    offset   += n;
                    max_size -= static_cast<size_t>(n);
                }
            }
        } else {
             // ... from file - size previously calculated ...
            if ( nullptr != module->ctx_.request_.body_interceptor_.writer_ ) {
                while ( ( n = ngx_read_file(chain->buf->file, buffer, max_read, offset) ) > 0 ) {
                    if ( n != module->ctx_.request_.body_interceptor_.writer_(buffer, static_cast<size_t>(n)) ) {
                        interceptor_error = true;
                        break;
                    }
                }
            } else {
                while ( ( n = ngx_read_file(chain->buf->file, buffer, max_read, offset) ) > 0 ) {
                    memcpy(module->ctx_.request_.body_ + offset, reinterpret_cast<char const*>(buffer), static_cast<size_t>(n));
                    offset += n;
                }
            }
        }
        if ( nullptr != module->ctx_.request_.body_interceptor_.flush_ ) {
            if ( 0 != module->ctx_.request_.body_interceptor_.flush_() ) {
                interceptor_error = true;
            }
        } else {
            module->ctx_.request_.body_[offset] = '\0';
        }
        // ... read error?
        if ( n < 0 ) {
            module->ctx_.errors_ptr_->Track("bad_request", NGX_HTTP_BAD_REQUEST,
                                            "BROKER_MISSING_OR_INVALID_BODY_ERROR",
                                            ("Error while reading body near byte " + std::to_string(offset) + "!").c_str()
            );
        } /* else { // reached end } */
    } else {
        //  ... read from memory ...
        size_t chain_size = 0;
        size_t offset     = 0;
        //  ... calculate size ...
        while ( NULL != chain ) {
            chain_size = static_cast<size_t>(chain->buf->last - chain->buf->pos);
            offset    += chain_size;
            chain      = chain->next;
        }
        // ... prepare body buffer or using fw?
        if ( nullptr == module->ctx_.request_.body_interceptor_.writer_ ) {
            if ( nullptr != module->ctx_.request_.body_ ) {
                delete [] module->ctx_.request_.body_;
            }
            module->ctx_.request_.body_ = new char [offset + 1];
            module->ctx_.request_.body_[0] = '\0';
        }
        // ... copy contents ..
        chain      = a_r->request_body->bufs;
        chain_size = 0;
        offset     = 0;
        
        
        if ( nullptr != module->ctx_.request_.body_interceptor_.writer_ ) {
            while ( NULL != chain ) {
                chain_size = static_cast<size_t>(chain->buf->last - chain->buf->pos);
                if ( static_cast<ssize_t>(chain_size) != module->ctx_.request_.body_interceptor_.writer_(chain->buf->pos, chain_size) ) {
                    interceptor_error = true;
                    break;
                }
                offset += chain_size;
                chain = chain->next;
            }
            if ( nullptr != module->ctx_.request_.body_interceptor_.flush_ ) {
                if ( 0 != module->ctx_.request_.body_interceptor_.flush_() ) {
                    interceptor_error = true;
                }
            }
        } else {
            while ( NULL != chain ) {
                chain_size = static_cast<size_t>(chain->buf->last - chain->buf->pos);
                memcpy(module->ctx_.request_.body_ + offset, reinterpret_cast<char const*>(chain->buf->pos), chain_size);
                offset += chain_size;
                chain = chain->next;
            }
            module->ctx_.request_.body_[offset] = '\0';
        }
    }
    
    if ( 0 == a_r->request_body_no_buffering && a_r->main->count > 1 ) {
        a_r->main->count--;
    }

    // ... write to permanent log ...
    if ( nullptr != module->ctx_.request_.body_interceptor_.writer_ ) {
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",file://%s",
                                          "IN : BODY",
                                          true == module->ctx_.log_body_
                                            ? module->ctx_.request_.body_interceptor_.dst_().c_str()
                                            : "<filtered>"
        );
    } else {
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                          "IN : BODY",
                                          true == module->ctx_.log_body_
                                            ? ( nullptr != module->ctx_.request_.body_ ? module->ctx_.request_.body_ : "" )
                                            : "<filtered>"
       );
    }

    // ... body entirely read ...
    // ... close?
    if ( nullptr != module->ctx_.request_.body_interceptor_.close_ ) {
        if ( 0 != module->ctx_.request_.body_interceptor_.close_() ) {
            interceptor_error = true;
        }
    }
    // ... if an error occurred ...
    if ( true == interceptor_error ) {
        module->ctx_.errors_ptr_->Track("server_error", NGX_HTTP_INTERNAL_SERVER_ERROR,
                                        "BROKER_INTERNAL_ERROR",
                                        "An error occurred while intercepting body!"
        );
    }
    // ... notify?
    if ( nullptr != module->ctx_.request_.body_read_completed_ ) {
        module->ctx_.request_.body_read_completed_();
        module->ctx_.request_.body_read_completed_ = nullptr;
    }
}

/**
 * @brief Write a request response.
 *
 * @param a_module
 * @param a_r
 */
void ngx::casper::broker::Module::WriteResponse (ngx_module_t& a_module, ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module* module = (ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module);
	if ( nullptr == module ) {
		return;
	}
    
    ngx::casper::broker::Module::WriteResponse(a_module, a_r, module->ctx_.response_.headers_.size() > 0 ? &module->ctx_.response_.headers_ : nullptr,
                                               module->ctx_.response_.status_code_, module->ctx_.response_.content_type_.c_str(),
                                               module->ctx_.response_.body_.c_str(), module->ctx_.response_.length_, module->ctx_.response_.binary_,
                                               /* a_finalize */ true,
                                               module->ctx_.log_token_.c_str()
    );
}

/**
 * @brief Write a request response.
 *
 * @param a_module
 * @param a_r
 * @param a_status_code
 * @param a_data
 * @param a_length
 * @param a_binary
 * @param a_finalize
 * @param a_data
 */
void ngx::casper::broker::Module::WriteResponse (ngx_module_t& a_module, ngx_http_request_t* a_r, const std::map<std::string, std::string>* a_headers,
                                                 ngx_uint_t a_status_code, const char* const a_content_type,
                                                 const char* const a_data, size_t a_length, const bool a_binary,
                                                 const bool a_finalize,
                                                 const char* const a_log_token, const bool a_plain_module)
{
    ngx_int_t         rc  = NGX_OK;
    const std::string uri = std::string((const char*)a_r->unparsed_uri.data, a_r->unparsed_uri.len);
    
    // ... debug only ...
    NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                "WR", "ENTER",
                                "%s", "writting response..."
    );
    
    // ... first, copy headers ( if any ) ...
    std::map<std::string, std::string, StringMapCaseInsensitiveComparator> ci_headers_map;
    if ( nullptr != a_headers ) {
        for ( auto it : *a_headers ) {
            ci_headers_map[it.first] = it.second;
        }
    }
    // ... grab custom content-type ( if any ) ...
    std::string content_type;
    const auto content_type_p = ci_headers_map.find("content-type");
    if ( ci_headers_map.end() != content_type_p ) {
        content_type = content_type_p->second;
        // ... Content-Type must not be duplicated ...
        ci_headers_map.erase(content_type_p);
    } else {
        content_type = nullptr != a_content_type ? std::string(a_content_type) : "";
    }
    
    // ... log write intentions ...
    NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                "WR", "CONTENT TYPE",
                                "%s", content_type.c_str()
    );

    if ( NGX_HTTP_HEAD != a_r->method ) {
        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "BODY",
                                    "%s", ( a_binary ? "<filtered - marked as binary data>" : a_data )
        );

        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "BODY SIZE",
                                    "" SIZET_FMT, a_length
        );
    }

    NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                "WR", "STATUS CODE",
                                "%u", a_status_code
    );
    
    ngx::casper::broker::Module* module = ( false == a_plain_module ? static_cast<ngx::casper::broker::Module*>(ngx_http_get_module_ctx(a_r, a_module)) : nullptr );

    try {
                
        if ( NGX_HTTP_NO_CONTENT != a_status_code ) {
            
            ngx_buf_t* buffer = (ngx_buf_t*) ngx_pcalloc(a_r->pool, sizeof(ngx_buf_t));
            if ( buffer == NULL ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate response buffer."
                );
            }
            
            ngx_chain_t* chain = (ngx_chain_t*) ngx_pcalloc(a_r->pool, sizeof(ngx_chain_t));
            if ( chain == NULL ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate response chain."
                );
            }
            
            ngx_str_t* payload = (ngx_str_t*)ngx_pcalloc(a_r->pool, sizeof(ngx_str_t));
            if ( NULL == payload ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate response payload."
                );
            }
            
            payload->len  = a_length;
            payload->data = (u_char*)ngx_palloc(a_r->pool, payload->len);
            if ( NULL == payload->data ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate response payload data."
                );
            }
            ngx_memcpy(payload->data, a_data, payload->len);
            
            buffer->pos           = buffer->last = (u_char*) payload->data; // first position in memory of the data
            buffer->last         += payload->len;                           // last position
            buffer->memory        = 1;                                      // content is in read-only memory filters must copy to modify,
            buffer->last_buf      = 1;                                      // there will be no more buffers in the request
            buffer->last_in_chain = 1;
            
            chain->buf            = buffer;
            chain->next           = NULL;
            
            a_r->headers_out.status            = a_status_code;
            a_r->headers_out.content_length_n  = static_cast<off_t>(payload->len);
            
            ngx::casper::broker::Module::SetOutHeaders(a_module, a_r, {
                { "Content-Type", content_type }
            });

            // ... Content-Length...
            const auto content_length_p = ci_headers_map.find("content-length");
            if ( ci_headers_map.end() != content_length_p ) {
                // ... must be changed, if it's a HEAD request ( else trust previous provided info ) ...
                if ( NGX_HTTP_HEAD == a_r->method ) {
                    a_r->headers_out.content_length_n = static_cast<ssize_t>(std::stoull(content_length_p->second));
                }
                // ... and must NOT be duplicated ...
                ci_headers_map.erase(content_length_p);
            }
            
            // ... create filtered headers map
            std::map<std::string, std::string> filtered_headers;
            for ( auto header : ci_headers_map ) {
                filtered_headers[header.first] = header.second;
            }
            
            // ... set out headers ...
            ngx::casper::broker::Module::SetOutHeaders(a_module, a_r, filtered_headers);
            
            // ... write to permanent log ...
            if ( nullptr != module ) {
                for ( auto it : filtered_headers ) {
                    ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s=%s",
                                                      "OUT: HEADER", it.first.c_str(), it.second.c_str()
                      );
                }
            }

            // ... discard 'incomming' body ( if any ) ...
            rc = ngx_http_discard_request_body(a_r);
            if ( rc == NGX_OK ) {
                rc = ngx_http_send_header(a_r);
                if ( rc == NGX_OK ) {
                    if ( NGX_HTTP_HEAD == a_r->method && 0 == payload->len ) {
                        // ... nothing to send ...
                        rc = NGX_OK;
                    } else {
                        // ... there's data to send ...
                        rc = ngx_http_output_filter(a_r, chain);
                    }
                    if ( not ( NGX_OK == rc || NGX_AGAIN == rc ) ) {
                        throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                             "Failed to send body - rc = %ld.",
                                                             rc
                        );
                    }
                } else {
                    throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                         "Failed to send header - %s error, rc = %ld.",
                                                         a_r->header_sent ? "header already sent" : "unknown",
                                                         rc
                    );
                }
            } else {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to discard body request, rc = %ld.", rc
                );
            }
            
            // ... write to permanent log ...
            if ( nullptr != module ) {
                ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                                  "OUT: CONTENT-TYPE", content_type.c_str()
                );

                ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT "," UINT64_FMT,
                                                  "OUT: CONTENT-LENGTH", static_cast<uint64_t>(a_r->headers_out.content_length_n)
                );
                
                if ( NGX_HTTP_HEAD != a_r->method ) {
                    ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT "," SIZET_FMT,
                                                      "OUT: BODY-SIZE", a_length
                    );

                    ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                                      "OUT: BODY",
                                                      true == module->ctx_.log_body_
                                                      ? ( a_binary ? "<filtered - marked as binary data>" : a_data )
                                                      : "<filtered>"
                    );
                } else if ( not ( a_status_code >= 200 && a_status_code < 400 ) ) {
                   ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                                     NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                                     "ERR: ",
                                                     true == module->ctx_.log_body_
                                                     ? ( a_binary ? "<filtered - marked as binary data>" : a_data )
                                                     : "<filtered>"
                   );
                }
            }
            
        } else {
            rc = NGX_HTTP_NO_CONTENT;
        }
        
        if (  a_r->main->count > 1 ) {
            a_r->main->count--;
        }

        if ( nullptr != module ) {
            ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                              NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%u",
                                              "OUT: STATUS-CODE", static_cast<unsigned>(a_status_code)
             );
        }

        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "LEAVING",
                                    "%s", "finished writting response..."
        );
        
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {

        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "LEAVING",
                                    "%s", "error while writting response..."
        );
        
        if ( nullptr != module ) {
            ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                              NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", r: %p, c: %p - %s",
                                              "EXP: ",
                                              a_r, a_r->connection,
                                              a_broker_exception.what()
            );
        }
        
        if ( 1 == a_r->header_sent ) {
            a_r->header_sent = 0;
        }
        
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        
    }
    
    // ...  relinquish control of 'a_r' life cycle ...
    a_r->casper_request = 0;

    ngx::casper::broker::Tracker::GetInstance().Unregister(a_r);

    if ( true == a_finalize ) {
        // ... unregister module ...
        if ( nullptr != module ) {
            ::ev::scheduler::Scheduler::GetInstance().Unregister(module);
        }
        // ... and finalize request ...
        ngx_http_finalize_request(a_r, rc);        
    }
    
}

/**
 * @brief Write a request response.
 *
 * @param a_module
 * @param a_r
 * @param a_location
 * @param a_log_token
 */
void ngx::casper::broker::Module::Redirect (ngx_module_t& a_module, ngx_http_request_t* a_r,
                                            const char* const a_location, const char* const a_log_token)
{
    ngx::casper::broker::Module::Redirect(a_module, a_r, a_location, {}, true, a_log_token);
}

/**
 * @brief Write a request response.
 *
 * @param a_module
 * @param a_r
 * @param a_location
 * @param a_headers
 * @param a_finalize
 * @param a_log_token
 */
void ngx::casper::broker::Module::Redirect (ngx_module_t& a_module, ngx_http_request_t* a_r,
                                            const char* const a_location,
                                            const std::map<std::string, std::string>& a_headers,
                                            const bool a_finalize,
                                            const char* const a_log_token)
{
    ngx_int_t         rc  = NGX_OK;
    const std::string uri = std::string((const char*)a_r->unparsed_uri.data, a_r->unparsed_uri.len);
    
    // ... debug only ...
    NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                "WR", "ENTER",
                                "%s", "redirecting..."
    );
    NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                "WR", "LOCATION",
                                "%s", a_location
    );

    ngx::casper::broker::Module* module = static_cast<ngx::casper::broker::Module*>(ngx_http_get_module_ctx(a_r, a_module));

    try {
        
        // ... write to permanent log ...
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%s",
                                          "OUT: REDIRECT",
                                          a_location
        );
        
        //
        // Set response headers:
        //
        const std::map<std::string, std::string> location_map = {
            { "Location", a_location }
        };
        for ( auto map : {&location_map, &a_headers} ) {
            ngx::casper::broker::Module::SetOutHeaders(a_module, a_r, *map);
        }
        a_r->headers_out.status = NGX_HTTP_MOVED_TEMPORARILY;
        rc                      = NGX_HTTP_MOVED_TEMPORARILY;
        
        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "LEAVING",
                                    "%s", "finished redirect..."
        );
        
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {
        
        NGX_BROKER_MODULE_DEBUG_LOG(a_module, a_r, a_log_token,
                                    "WR", "LEAVING",
                                    "%s", "error while writting redirect..."
        );
        
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;

    }

    // ...  relinquish control of 'a_r' life cycle ...
    a_r->casper_request = 0;

    ngx::casper::broker::Tracker::GetInstance().Unregister(a_r);

    if ( true == a_finalize ) {
        
        // ... ensure request will be finalized ...
        a_r->connection->read->eof  = 1;
        a_r->connection->write->eof = 1;
        a_r->error_page = 0;
        
        // ... unregister module ...
        ::ev::scheduler::Scheduler::GetInstance().Unregister(module);
        // ... and finalize request ...
        ngx_http_finalize_request(a_r, rc);
        
    }

}

/**
 * @brief Set request 'out' headers.
 *
 * @param a_module
 * @param a_r
 * @param a_header
 */
void ngx::casper::broker::Module::SetOutHeaders (ngx_module_t& /* a_module */, ngx_http_request_t* a_r,
                                                 const std::map<std::string, std::string>& a_headers)
{
    const std::map<const char* const, ngx_table_elt_t**, StringMapCaseInsensitiveComparator> special_out_headers_map = {
        { "Location"        , &a_r->headers_out.location      },
        { "Date"            , &a_r->headers_out.date          }
    };
    
    const std::map<const char* const , ngx_str_t*, StringMapCaseInsensitiveComparator> std_out_headers {
        { "Content-Type"    , &a_r->headers_out.content_type },
        { "Charset"         , &a_r->headers_out.charset      },
    };
    
    // ... pre set clean up ...
    if ( a_headers.end() != a_headers.find("Location") ) {
        ngx_http_clear_location(a_r);
    }

    // ... set headers ...
    for ( auto header_it : a_headers ) {
        
        const char* header_name = header_it.first.c_str();

        ngx_table_elt_t* header_elt;
        ngx_str_t*       value_str_ptr;
        
        const auto std_out_headers_it = std_out_headers.find(header_name);
        if ( std_out_headers.end() != std_out_headers_it ) {
            header_elt    = nullptr;
            value_str_ptr = std_out_headers_it->second;
        } else {
            header_elt = (ngx_table_elt_t*)ngx_list_push(&a_r->headers_out.headers);
            if ( NULL == header_elt ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate redirect header '%s' elt.", header_name
                );
            }
            value_str_ptr = &header_elt->value;
        }
        
        // ... set header name?
        if ( nullptr != header_elt ) {
            
            header_elt->hash = 1;
            
            ngx_str_t* name_str = (ngx_str_t*)ngx_pcalloc(a_r->pool, sizeof(ngx_str_t));
            if ( NULL == name_str ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate redirect header name '%s' string.", header_name
                );
            }
            name_str->len  = strlen(header_name);
            name_str->data = (u_char*)ngx_palloc(a_r->pool, name_str->len);
            if ( NULL == name_str->data ) {
                throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                     "Failed to allocate redirect header name '%s' payload.", header_name
                );
            }
            ngx_memcpy(name_str->data, header_name, name_str->len);
            header_elt->key.len  = name_str->len;
            header_elt->key.data = name_str->data;
            
        }
        
        // ... set header value ...
        ngx_str_t* value_str = (ngx_str_t*)ngx_pcalloc(a_r->pool, sizeof(ngx_str_t));
        if ( NULL == value_str ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "Failed to allocate redirect header '%s' value string.", header_name
            );
        }
        value_str->len  = header_it.second.length();
        value_str->data = (u_char*)ngx_palloc(a_r->pool, value_str->len);
        if ( NULL == value_str->data ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "Failed to allocate redirect header '%s' value payload.", header_name
            );
        }
        ngx_memcpy(value_str->data, header_it.second.c_str(), value_str->len);
        value_str_ptr->len  = value_str->len;
        value_str_ptr->data = value_str->data;
        
        const auto special_it = special_out_headers_map.find(header_name);
        if ( special_out_headers_map.end() != special_it ) {
            *special_it->second = header_elt;
        }

    }
    
}

/**
 * @brief Clean-up a request context.
 *
 * @param a_module_t
 * @param a_data Expecting \link ngx::casper::broker::Module \link
 */
void ngx::casper::broker::Module::Cleanup (ngx_module_t& a_module_t, void* a_data)
{
    //
    // ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️
    // ☠️ !!!!! CLEANUP DATA MUST BE SET !!!!!  ☠️
    // ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️ ⚠️
    //
    assert(nullptr != a_data);

    ngx::casper::broker::Module* module = static_cast<ngx::casper::broker::Module*>(a_data);
    ngx_http_request_t*         r       = module->ctx_.ngx_ptr_;
    
    const std::string uri       = std::string((const char*)r->unparsed_uri.data, r->unparsed_uri.len);
    const std::string log_token = nullptr != module ? module->ctx_.log_token_.c_str() : "ngx::casper::broker::Module";
    
    NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                "FH", "ENTER",
                                "%s", uri.c_str()
    );
    
    const auto method_it = ngx::casper::broker::Module::k_http_methods_map_.find((uint32_t)r->method);
    if ( ngx::casper::broker::Module::k_http_methods_map_.end() == method_it ) {
        NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                    "FH", "METHOD",
                                    "%u", r->method
        );
    } else {
        NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                    "FH", "METHOD",
                                    "%s", method_it->second.c_str()
        );
    }
    
    if ( module != NULL ) {
        
        // ... write to permanent log ...
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",",
                                          "ST : CLEAN UP"
        );
        
        NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                    "FH", "CONTEXT",
                                    "%s", "deleting context...", uri.c_str()
        );
        
        ngx_http_set_ctx(r, nullptr, a_module_t);
        
        delete module;
        
    }
    
    NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                "FH", "LEAVING",
                                "%s", "done"
    );
    
    ngx::casper::broker::Tracker::GetInstance().Unregister(r);

    NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, r, log_token.c_str(),
                                "FH", "STATS",
                                "" SIZET_FMT " requests remaining(s)",
                                ngx::casper::broker::Tracker::GetInstance().Count()
    );

}

#ifdef __APPLE__
#pragma mark -
#endif

void ngx::casper::broker::Module::SetCookie (const std::string& a_name, const std::string& a_value,
                                             const std::string& a_domain, const std::string& a_path,
                                             const bool a_secure, const bool a_http_only,
                                             const int64_t&  a_expires_at,
                                             ngx::casper::broker::Module::Cookie& o_cookie)
{
    o_cookie.name_      = a_name;
    o_cookie.value_     = a_value;
    o_cookie.domain_    = a_domain;
    o_cookie.path_      = a_path;
    o_cookie.secure_    = a_secure;
    o_cookie.http_only_ = a_http_only;
    
    if ( a_expires_at > 0 ) {
        osal::posix::Time::HumanReadableTime hr_time;
        if ( false == osal::Time::GetHumanReadableTimeFromUTC(a_expires_at, hr_time) ) {
            throw ::ev::Exception("Could not set cookie expiration date - can't convert to human readable format!");
        }
        // format: Wdy, DD Mon YYYY HH:MM:SS GMT
        char tmp_buff [30] = {0};
        if ( 29 != snprintf(tmp_buff, sizeof(tmp_buff) / sizeof(tmp_buff[0]),
                            "%*.*s, %2d %*.*s %04d %02d:%02d:%02d GMT",
                            3, 3, osal::Time::GetHumanReadableWeekday(hr_time.weekday_),
                            hr_time.day_,
                            3, 3, osal::Time::GetHumanReadableMonth(hr_time.month_),
                            hr_time.year_,
                            hr_time.hours_,
                            hr_time.minutes_,
                            hr_time.seconds_
            )
        ) {
            throw ::ev::Exception("Could not set cookie expiration date - can't format to human readable format!");
        }
        o_cookie.expires_ = tmp_buff;
    } else {
        o_cookie.expires_ = "";
    }
    
    std::stringstream ss;
    
    ss << o_cookie.name_ << "=" << o_cookie.value_;
    
    if ( o_cookie.path_.length() > 0 ) {
        ss << "; Path=" << o_cookie.path_;
    }
    if ( o_cookie.domain_.length() > 0 ) {
        ss << "; Domain=" << o_cookie.domain_;
    }
    if ( o_cookie.expires_.length() > 0 ) {
        ss << "; Expires=" << o_cookie.expires_;
    }
    if ( true == o_cookie.secure_ ) {
        ss << "; Secure";
    }
    if ( true == o_cookie.http_only_ ) {
        ss << "; HttpOnly";
    }
    
    o_cookie.inline_ = ss.str();
}

/**
 * @brief Finalize a request with a text response, an internal redirect or a standard redirect.
 *
 * @param a_module
 * @param a_http_status_code
 * @param a_ngx_return_code
 * @param a_force
 *
 * @return
 */
ngx_int_t ngx::casper::broker::Module::FinalizeRequest (ngx::casper::broker::Module* a_module,
                                                        const uint16_t* a_http_status_code, const ngx_int_t* a_ngx_return_code,
                                                        bool a_force)
{
    
    uint16_t  status_code = nullptr != a_http_status_code ? *a_http_status_code : a_module->ctx_.response_.status_code_;
    ngx_int_t return_code  = nullptr != a_ngx_return_code  ? *a_ngx_return_code  : a_module->ctx_.response_.return_code_;
    
    // ... log
    NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_module->ctx_.log_token_.c_str(),
                                "FR", "ENTER",
                                "%s", "finalizing request..."
    );
    
    //
    // FAILURE or SUCCESS?
    //
    if ( not ( NGX_OK == return_code || NGX_HTTP_MOVED_TEMPORARILY == return_code )  ) {
        
        //
        // FAILURE
        //
        
        return ngx::casper::broker::Module::FinalizeWithErrors(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_,
                                                               &a_module->ctx_.response_.headers_,
                                                               status_code, return_code,
                                                               a_module->ctx_.response_.serialize_errors_, /* a_force */ a_force,
                                                               a_module->ctx_.log_token_
        );

        
    } else {
        
        //
        // SUCCESS
        //
        
        // ... log
        NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_module->ctx_.log_token_.c_str(),
                                    "IT", "LEAVING",
                                    "%s", ""
        );
        
        // ...  relinquish control of 'ngx_ptr_' life cycle?
        a_module->ctx_.ngx_ptr_->casper_request = a_force ? 0 : 1;

        // ... a redirect or just a 'text' response?
        
        if ( NGX_HTTP_MOVED_TEMPORARILY == return_code ) {

            // ... set uri?
            if ( 0 == a_module->ctx_.response_.redirect_.uri_.length() ) {
                a_module->ctx_.response_.redirect_.uri_ = a_module->ctx_.response_.redirect_.protocol_ + "://" + a_module->ctx_.response_.redirect_.host_;
                if ( 80 != a_module->ctx_.response_.redirect_.port_ ) {
                    a_module->ctx_.response_.redirect_.uri_ += ":" + std::to_string(a_module->ctx_.response_.redirect_.port_);
                }
                
                if ( '/' != a_module->ctx_.response_.redirect_.path_.c_str()[0] ) {
                    a_module->ctx_.response_.redirect_.uri_ += '/';
                }
                a_module->ctx_.response_.redirect_.uri_ += a_module->ctx_.response_.redirect_.path_;
            }
            
            // ... now prepare redirect ....
            ngx::casper::broker::Module::Redirect(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_,
                                                  a_module->ctx_.response_.redirect_.uri_.c_str(),
                                                  a_module->ctx_.response_.headers_,
                                                  /* a_finalize */ a_force,
                                                  a_module->ctx_.log_token_.c_str()
            );
            
            return_code = NGX_OK;
            
        } else if ( true == a_module->ctx_.response_.redirect_.internal_ ) {
            
            // ... log ...
            NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_module->ctx_.log_token_.c_str(),
                                        "FR", "INTERNAL REDIRECT",
                                        "%s", "scheduling..."
            );
            
            // ... a 'internal redirect' is the 'response' ...
            if ( true == a_module->ctx_.at_content_handler_ || true == a_module->ctx_.at_rewrite_handler_ ) {
                
                const ngx_uint_t cycle_cnt = a_module->ctx_.cycle_cnt_;
                
                // ... perform it now ...
                return_code = ngx::casper::broker::Module::InternalRedirect(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, /* a_force */ a_force);
                if ( NGX_OK != return_code ) {
                    
                    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(a_module->ctx_, "An error occurred while starting an internal redirect!");
                    
                    return ngx::casper::broker::Module::FinalizeWithErrors(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_,
                                                                           &a_module->ctx_.response_.headers_,
                                                                           status_code, return_code,
                                                                           a_module->ctx_.response_.serialize_errors_,
                                                                           a_force,
                                                                           a_module->ctx_.log_token_
                    );
                    
                } else if ( 0 == cycle_cnt ) {
                    // ... signal rewrite handler that we're done ...
                    return_code = NGX_DONE;
                }
                
            } else {
                // ... 'internal redirect' can only be performed at content or rewrite phase ...
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(a_module->ctx_,
                        "An error occurred while starting an internal redirect - must be called in a content or rewrite handler!"
                );
                return ngx::casper::broker::Module::FinalizeWithErrors(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_,
                                                                       &a_module->ctx_.response_.headers_,
                                                                       status_code, return_code,
                                                                       a_module->ctx_.response_.serialize_errors_,
                                                                       a_force,
                                                                       a_module->ctx_.log_token_
               );
            }
            
        } else {
            
            // ... log ...
            NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_module->ctx_.log_token_.c_str(),
                                        "FR", "CONTENT TYPE",
                                        "%s", a_module->ctx_.response_.content_type_.c_str()
            );
            
            // ... log ...
            NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_module->ctx_.log_token_.c_str(),
                                        "FR", "BODY",
                                        "%s", ( a_module->ctx_.response_.binary_ ? "<filtered - marked as binary data>" : a_module->ctx_.response_.body_.c_str() )
            );
            
            // ... just a response ...
            
            ngx::casper::broker::Module::WriteResponse(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_,
                                                       ( a_module->ctx_.response_.headers_.size() > 0 ) ? &a_module->ctx_.response_.headers_ : nullptr,
                                                       status_code, a_module->ctx_.response_.content_type_.c_str(),
                                                       a_module->ctx_.response_.body_.c_str(), a_module->ctx_.response_.length_, a_module->ctx_.response_.binary_,
                                                       /* a_finalize */ a_force,
                                                       a_module->ctx_.log_token_.c_str()
            );
            
            return_code = NGX_OK;
            
        }
    }

    // ... we're done ...
    return return_code;
}

/**
 * @brief Finalize a request with a n internal redirect
 *
 * @param a_module
 *
 * @return
 */
ngx_int_t ngx::casper::broker::Module::FinalizeWithInternalRedirect (ngx::casper::broker::Module* a_module)
{
    a_module->ctx_.response_.redirect_.asynchronous_ = a_module->ctx_.response_.asynchronous_;
    a_module->ctx_.response_.asynchronous_           = false;
    a_module->ctx_.response_.redirect_.internal_     = true;
    a_module->ctx_.response_.return_code_            = NGX_OK;
    return ngx::casper::broker::Module::FinalizeRequest(a_module);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Trackle content phase response.
 *
 * @param a_r        NGX HTTP request.
 * @param a_module_t NGX module.
 * @param a_token    Log token.
 *
 * @return           A NGX code.
 */
ngx_int_t ngx::casper::broker::Module::ContentPhaseTackleResponse (ngx_http_request_t* a_r, ngx_module_t& a_module_t,
                                                                   const char* const /* a_token */)
{
    ngx::casper::broker::Module* module = (ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module_t);
    if ( nullptr == module ) {
        // ... wtf?
        return NGX_ERROR;
    }
   
    module->SetAtContentHandler();
    
    // ... if not ready yet ...
    if ( true == module->ReadingBody() ) {
        return NGX_DONE;
    } else if ( false == module->SynchronousResponse() ) {
        return NGX_OK; // HOLDING REQUEST
    }
    
    if ( true == module->InternalRedirect() ) {
        return NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(module);
    } else if ( true == module->NoContentResponse() ) {
        return NGX_HTTP_NO_CONTENT;
    }

    // ... wtf - this module shouldn't reach here ! ...
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

/**
 * @brief Trackle rewrite phase response.
 *
 * @param a_r        NGX HTTP request.
 * @param a_module_t NGX module.
 * @param a_token    Log token.
 *
 * @return           A NGX code.
 */
ngx_int_t ngx::casper::broker::Module::RewritePhaseTackleResponse (ngx_http_request_t* a_r, ngx_module_t& a_module_t,
                                                                   const char* const a_token,
                                                                   const std::function<ngx_int_t()> a_factory)
{
    //
    // NOTICE: any return code rather than NGX_DECLINED or NGX_DONE will finalize request ...
    //

    const auto tackle_response = [CC_IF_DEBUG(a_token)] (ngx::casper::broker::Module* a_module) -> ngx_int_t {
        
        if ( true == a_module->SynchronousResponse() ) {
            // ... synchronous response ...
            if ( true == a_module->InternalRedirect() ) {
                NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_token,
                                            "RW", "LEAVING",
                                            "%s", "synchronous response - internal redirect!"
                );
                // ... internal redirect and we're done ...
                return NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(a_module);
            } else {
                NGX_BROKER_MODULE_DEBUG_LOG(a_module->ctx_.module_, a_module->ctx_.ngx_ptr_, a_token,
                                            "RW", "LEAVING",
                                            "%s", "synchronous response - not handled where!"
                );
                // ... so, next phase ( content ) by returning ...
                return NGX_DECLINED;
            }
        } else {
            // ... asynchronous response: stall rewrite phase ...
            return NGX_DECLINED;
        }
        
    };
    
    ngx_int_t rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
    
    ngx::casper::broker::Module* module = (ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module_t);
    if ( nullptr == module ) {
        // ... first time this handler is called, it's time to create this request module's context ...
        rv = a_factory();
        if ( NGX_OK == rv ) {
            rv = tackle_response((ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module_t));
        } else if ( NGX_DONE == rv ) {
            ngx_http_finalize_request(a_r, NGX_DONE);
            rv = NGX_DECLINED;
        }
        if ( NGX_AGAIN == rv ) {
            rv = NGX_DECLINED;
        }
    } else {
        // ... mark as inside rewrite handler ...
        module->SetAtRewriteHandler();
        // ... returning from a previous stalled rewrite phase ...
        rv = tackle_response(module);
    }
    
    return rv;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Write a message to a nginx log.
 *
 * @param a_module_t
 * @param a_r
 * @param a_log
 * @param a_token
 * @param a_location
 * @param a_action
 * @param a_format
 * @param ...
 */
void ngx::casper::broker::Module::Log (ngx_module_t& a_module_t, ngx_http_request_t* a_r, ngx_uint_t a_level,
                                       const char* const a_token, const char* const a_location, const char* const a_action,
                                       const char* const a_format, ...)
{

    const void* module_ptr = (void*) ngx_http_get_module_ctx(a_r, a_module_t);
    
    std::string payload = "";
    
    if ( nullptr != a_format ) {
        auto temp    = std::vector<char> {};
        auto length  = std::size_t { 512 };
        std::va_list args;
        while ( temp.size() <= length ) {
            temp.resize(length + 1);
            va_start(args, a_format);
            const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
            va_end(args);
            if ( status < 0 ) {
                payload = "string formatting error";
            }
            length = static_cast<std::size_t>(status);
        }
        payload = length > 0 ? std::string { temp.data(), length } : "";
    }
    
    static const auto pad = [] (const char* const a_string, size_t a_length) -> std::string {
        const size_t length = strlen(a_string);
        if ( length >= a_length ) {
            return std::string(a_string, a_length);
        } else {
            std::string padded_string;
            for ( size_t idx = length ; idx < a_length ; ++idx ) {
                padded_string += " ";
            }
            padded_string += a_string;
            return padded_string;
        }
    };

    const std::string location_str = pad((nullptr != a_location ? a_location : "?"), 3);
    const std::string action_str   = pad((nullptr != a_action   ? a_action   : "?"), 12);
    const std::string token_str    = pad((nullptr != a_token    ? a_token    : "?"), 18);

    //
    // ... format ...
    //
    // [token, req addr, ctx addr, location, action] : ...
    //
    ngx_http_casper_broker_log_msg(a_r, a_level,
                                   "[%s, 0x%p, 0x%p, %s, %s] : %s ",
                                    token_str.c_str(), a_r, module_ptr, location_str.c_str(), action_str.c_str(), payload.c_str());
}


#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Prepare a module for content handler phase.
 *
 * @param a_module_t
 * @param a_r
 * @param a_log_token
 * @param o_params
 */
ngx_int_t ngx::casper::broker::Module::WarmUp (ngx_module_t& a_module_t, ngx_http_request_t* a_r, const ngx_str_t& a_log_token,
                                               ngx::casper::broker::Module::Params& o_params)
{
    //
    // GRAB 'MAIN' CONFIG
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }
    
    //
    // COLLECT REQUEST HEADERS
    //
    ngx_list_part_t* list_part = &a_r->headers_in.headers.part;
    while ( NULL != list_part ) {
        // ... for each element ...
        ngx_table_elt_t* header = (ngx_table_elt_t*)list_part->elts;
        for ( ngx_uint_t index = 0 ; list_part->nelts > index ; ++index ) {
            // ... if is set ...
            if ( 0 < header[index].key.len && 0 < header[index].value.len ) {
                // ... keep track of it ...
                std::string key   = std::string(reinterpret_cast<char const*>(header[index].key.data), header[index].key.len);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                
                o_params.in_headers_[key] = std::string(reinterpret_cast<char const*>(header[index].value.data), header[index].value.len);
            }
        }
        // ... next...
        list_part = list_part->next;
    }
    
    const std::string log_token = std::string(reinterpret_cast<char*>(a_log_token.data), a_log_token.len);

    ngx_int_t ngx_return_code = NGX_OK;

    //
    // i18n
    //
    if ( false == ngx::casper::broker::I18N::GetInstance().IsInitialized() ) {
        
        if ( 0 == broker_conf->resources_dir.len  ) {
            NGX_BROKER_MODULE_ERROR_LOG(a_module_t, a_r, log_token.c_str(),
                                        "WU", "INITIALIZING",
                                        "%s", "failure: i18n initialization failure - resources dir not set!"
            );
            ngx_return_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        } else {
            
            const std::string resources_dir = std::string(reinterpret_cast<char*>(broker_conf->resources_dir.data), broker_conf->resources_dir.len);
            
            ngx::casper::broker::I18N::GetInstance().Startup(resources_dir,
                                                             [&a_module_t, &a_r, &log_token, &ngx_return_code]
                                                             (const char* const /* a_i18_key */, const std::string& /* a_file */, const std::string& a_reason) {
                                                                 NGX_BROKER_MODULE_ERROR_LOG(a_module_t, a_r, log_token.c_str(),
                                                                                             "WU", "INITIALIZING",
                                                                                             "failure: i18n initialization failure - %s!",
                                                                                             a_reason.c_str()
                                                                 );
                                                                 ngx_return_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                                                             }
            );
        }
        
    }    
    
    //
    // SET LOCALE
    //
    //
    // Multiple types, weighted with the quality value syntax:
    //
    // Accept-Language: fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5
    //
    const auto accept_language_it = o_params.in_headers_.find("accept-language");
    if ( o_params.in_headers_.end() != accept_language_it ) {
        
        ngx::casper::broker::I18N& i18n = ngx::casper::broker::I18N::GetInstance();
        
        std::string acceptable = accept_language_it->second;
        
        acceptable.erase(remove_if(acceptable.begin(), acceptable.end(), isspace), acceptable.end());
        
        const char* sep = strchr(acceptable.c_str(), ':');
        if ( nullptr != sep ) {

            std::stringstream ss;

            ss << std::string(acceptable.c_str() + ( static_cast<size_t>(sep - acceptable.c_str()) + sizeof(char) ));
            
            while( std::getline(ss, acceptable, ',') ) {
                
                const char* weight_ptr = strchr(acceptable.c_str(), ';');
                if ( nullptr != weight_ptr ) {
                    if ( true == i18n.Contains(std::string(acceptable.c_str(), static_cast<size_t>(weight_ptr - acceptable.c_str()))) ) {
                        o_params.locale_ = acceptable;
                        break;
                    }
                } else if ( true == i18n.Contains(acceptable) ) {
                    o_params.locale_ = acceptable;
                    break;
                }
                
            }
            
        }
        
    }
    
    if ( 0 == o_params.locale_.length() ) {
        o_params.locale_ = "en_US";
    }
    
    // ... if an error is alreay set ..
    if ( NGX_OK != ngx_return_code ) {
        // ... nothing else to do ...
        return ngx_return_code;
    }

    //
    // 'Content-Type' Validation
    //
    const auto content_type_it = o_params.in_headers_.find("content-type");
    if ( o_params.in_headers_.end() != content_type_it ) {
        //... explicitly reject 'multipart' content-type ...
        if ( nullptr != strstr(content_type_it->second.c_str(), "multipart/") ) {
            ngx_return_code = NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
        } else {
            ngx_return_code = NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
            if ( o_params.supported_content_types_.size() > 0 ) {
                for ( auto ct : o_params.supported_content_types_ ) {
                    if ( 0 == strcasecmp(ct.c_str(), content_type_it->second.c_str()) || 0 == strcasecmp("*/*", ct.c_str())  ) {
                        ngx_return_code = NGX_OK;
                        break;
                    }
                }
            } else {
                // ... check if content type is allowed ...
                Json::Reader reader;
                Json::Value  array_value;
                if ( false == reader.parse(std::string(reinterpret_cast<const char*>(broker_conf->supported_content_types.data), broker_conf->supported_content_types.len), array_value)
                    || Json::ValueType::arrayValue != array_value.type()
                ) {
                    NGX_BROKER_MODULE_ERROR_LOG(a_module_t, a_r, log_token.c_str(),
                                                "WU", "LEAVING",
                                                "%s", "invalid directive 'supported_content_types' value - unable to parse it as JSON array of strings"
                    );
                } else {
                    for ( Json::ArrayIndex idx = 0 ; idx < array_value.size() ; ++idx ) {
                        if ( false == array_value[idx].isNull() && true == array_value[idx].isString() && nullptr != strcasestr(content_type_it->second.c_str(), array_value[idx].asCString()) ) {
                            ngx_return_code = NGX_OK;
                            break;
                        }
                    }
                }
            }
        }
    }

    return ngx_return_code;
}

/**
 * @brief Ensure provided directives are present.
 *
 * @param a_module_t
 * @param a_r
 * @param a_log_token
 * @param a_map
 * @param a_errors
 *
 * @return
 */
ngx_int_t ngx::casper::broker::Module::EnsureDirectives (ngx_module_t& a_module_t, ngx_http_request_t* a_r, const ngx_str_t& a_log_token,
                                                         const std::map<std::string, ngx_str_t*>& a_map, ngx::casper::broker::Errors* a_errors)
{
    //
    // GRAB 'MAIN' CONFIG
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }

    const std::string log_token = std::string(reinterpret_cast<char*>(a_log_token.data), a_log_token.len);

    size_t errors_count = 0;
    
    for ( auto it : a_map ) {
        
        if ( 0 != it.second->len ) {
            continue;
        }
        
        U_ICU_NAMESPACE::Formattable args [] = {
            "",
            it.first.c_str()
        };
        
        a_errors->Track("config_error", NGX_HTTP_INTERNAL_SERVER_ERROR,
                        "BROKER_MODULE_CONFIG_MISSING_OR_INVALID_DIRECTIVE_VALUE_ERROR", args, 2
                        );
        
        errors_count++;
        
    }
    
    const ngx_int_t ngx_return_code = ( errors_count > 0 ? NGX_HTTP_INTERNAL_SERVER_ERROR : NGX_OK );
    
    if ( NGX_OK != ngx_return_code || a_errors->Count() > 0 ) {
        const std::string data = a_errors->Serialize2JSON();
        ngx::casper::broker::Module::WriteResponse(a_module_t, a_r, nullptr,
                                                   static_cast<ngx_uint_t>(ngx_return_code), a_errors->content_type_.c_str(),
                                                   data.c_str(), data.length(), /* a_binary */ false,
                                                   /* a_finalize */ false,
                                                   log_token.c_str()
        );
    }
        
    return ngx_return_code;
}

/**
 * @brief Translate internal method id to an HTTP method string representation.
 *
 * @param a_r
 *
 * @return The HTTP method name, ??? if no match.
 */
std::string ngx::casper::broker::Module::GetMethodName (const ngx_http_request_t* a_r)
{
    if ( nullptr != a_r->method_name.data && a_r->method_name.len > 0 ) {
        return std::string(reinterpret_cast<const char* const>(a_r->method_name.data), static_cast<size_t>(a_r->method_name.len));
    } else {
        const auto method_it = ngx::casper::broker::Module::k_http_methods_map_.find(static_cast<uint32_t>(a_r->method));
        if ( ngx::casper::broker::Module::k_http_methods_map_.end() != method_it ) {
            return method_it->second.c_str();
        } else {
            return "???";
        }
    }
}

/**
 * @brief Return untouched headers.
 *
 * @param a_r
 * @param o_array Untouched headers as JSON array of strings.
 */
void ngx::casper::broker::Module::GetUntouchedHeaders (const ngx_http_request_t* a_r, Json::Value& o_array)
{
    ngx::utls::nrs_ngx_get_in_untouched_headers(a_r, o_array);
}

#ifdef __APPLE__
#pragma mark -
#endif


/**
 * @brief Finalize a request with an internal redirect.
 *
 * @param a_module_t
 * @param a_r
 * @param a_close_now
 *
 * @return
 */
ngx_int_t ngx::casper::broker::Module::InternalRedirect (ngx_module_t& a_module_t, ngx_http_request_t* a_r,
                                                         bool a_close_now)
{
    ngx::casper::broker::Module* module = (ngx::casper::broker::Module*) ngx_http_get_module_ctx(a_r, a_module_t);
    
    ngx_str_t args = ngx_null_string;
    
    // /<location>/<file>
    if ( 0 == module->ctx_.response_.redirect_.uri_.length() ) {
        if ( '/' != module->ctx_.response_.redirect_.location_.c_str()[0] ) {
            module->ctx_.response_.redirect_.uri_ += '/';
        }
        module->ctx_.response_.redirect_.uri_ += module->ctx_.response_.redirect_.location_;
        if ( '/' != module->ctx_.response_.redirect_.uri_.c_str()[module->ctx_.response_.redirect_.uri_.length() - 1] ) {
            module->ctx_.response_.redirect_.uri_ += '/' + module->ctx_.response_.redirect_.file_;
        } else {
            module->ctx_.response_.redirect_.uri_ += module->ctx_.response_.redirect_.file_;
        }
        // transform all headers to params
        const auto transform = [] (const char a_separator, const std::string& a_name, const std::string& a_value) -> std::string {
            std::string name = a_name;
            std::transform(name.begin(), name.end(), name.begin(),   ::tolower);
            for ( std::string::size_type idx = 0 ; ( idx = name.find('-', idx) ) != std::string::npos ; ) {
                name.replace(idx, 1, "_");
                idx += 1;
            }
            if ( '\0' != a_separator ) {
                return ( a_separator + name + "=" + a_value );
            } else {
                return ( name + "=" + a_value );
            }
        };
        
        if ( module->ctx_.response_.headers_.size() > 0 ) {
            auto it = module->ctx_.response_.headers_.begin();
            std::string args_str = transform('\0', it->first, it->second);
            for ( ++it ; module->ctx_.response_.headers_.end() != it ; ++it ) {
                args_str += transform('&', it->first, it->second);
            }
            
            const size_t unescaped_len = args_str.length();
            u_char*      unescaped_str = (u_char*)ngx_palloc(a_r->pool, unescaped_len);
            if ( nullptr == unescaped_str ) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            memcpy(unescaped_str, args_str.c_str(), unescaped_len);
#if 1
            args.data = unescaped_str;
            args.len  = unescaped_len;
#else
            const uintptr_t number_of_chars_to_escape = ngx_escape_uri(NULL, unescaped_str, unescaped_len, NGX_ESCAPE_URI_COMPONENT);
            if ( 0 != number_of_chars_to_escape ) {
                const size_t escaped_len = ( unescaped_len + ( 2 * number_of_chars_to_escape ) );
                u_char* escaped_str = (u_char*)ngx_palloc(a_r->pool, escaped_len);
                if ( nullptr == escaped_str ) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                (void)ngx_escape_uri(escaped_str, unescaped_str, escaped_len, NGX_ESCAPE_URI_COMPONENT);
                args.data = escaped_str;
                args.len  = escaped_len;
            } else {
                args.data = unescaped_str;
                args.len  = unescaped_len;
            }
#endif
        }
    }

    module->ctx_.ngx_ptr_->uri.len  = module->ctx_.response_.redirect_.uri_.length();
    module->ctx_.ngx_ptr_->uri.data = (u_char*)ngx_palloc(a_r->pool, module->ctx_.ngx_ptr_->uri.len);
    if ( NULL == module->ctx_.ngx_ptr_->uri.data ) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_memcpy(module->ctx_.ngx_ptr_->uri.data, module->ctx_.response_.redirect_.uri_.c_str(), module->ctx_.ngx_ptr_->uri.len);

    // ... if not supported, translate method to GET ...
    if ( module->ctx_.response_.redirect_.supported_methods_.end() == module->ctx_.response_.redirect_.supported_methods_.find(module->ctx_.ngx_ptr_->method) ) {
        module->ctx_.ngx_ptr_->method      = NGX_HTTP_GET;
        module->ctx_.ngx_ptr_->method_name = ngx_http_core_get_method;
    }
    
    // ... log
    NGX_BROKER_MODULE_DEBUG_LOG(module->ctx_.module_, module->ctx_.ngx_ptr_, module->ctx_.log_token_.c_str(),
                                "IR", "ENTER",
                                "%s", module->ctx_.response_.redirect_.uri_.c_str()
    );
    
    //
    // NOTICE: when calling ngx_http_internal_redirect, ngx_http_core_run_phases will be called;
    //         this module next phase will be content phase and it MUST/WILL return NGX_DECLINED
    //         due to handler barrier ( 1 == a_r->internal )
    //
    
    // ... ensure control of 'ngx_ptr_' life cycle ...
    module->ctx_.ngx_ptr_->casper_request = 1;
    
    ngx_uint_t       module_log_level = module->ctx_.ngx_ptr_->connection->log->log_level;
    ngx_open_file_t* module_open_file = module->ctx_.ngx_ptr_->connection->log->file;
    
    const ngx_int_t rv = ngx_http_internal_redirect(module->ctx_.ngx_ptr_, &module->ctx_.ngx_ptr_->uri, ( args.len > 0 ? &args : nullptr ));
    if ( NGX_DONE != rv ) {
        // ...  relinquish control of 'a_r' life cycle ...
        module->ctx_.ngx_ptr_->casper_request = 0;
        // ... write to permanent log ...
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",status_code=:%u}",
                                          "ST : INTERNAL REDIRECT",
                                          NGX_HTTP_INTERNAL_SERVER_ERROR
        );
        // ... wtf?
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    //
    // NOTICE: by calling NGINX internal redirect function
    //         a_r->main->count will be increased by one ( setting it to 2 ),
    //         ngx_http_close_request will be called but connection won't be closed ...
    
    // ... ensure this module context isn't lost ...
    ngx_http_set_ctx(module->ctx_.ngx_ptr_, module, a_module_t);
    // ... and restore log level to debug connection ...
    module->ctx_.ngx_ptr_->connection->log->log_level = module_log_level;
    module->ctx_.ngx_ptr_->connection->log->file      = module_open_file;
    
    // ...  relinquish control of 'a_r' life cycle ...
    a_r->casper_request = 0;
    
    // ... log
    NGX_BROKER_MODULE_DEBUG_LOG(module->ctx_.module_, module->ctx_.ngx_ptr_, module->ctx_.log_token_.c_str(),
                                "IT", "LEAVING",
                                "%s", module->ctx_.response_.redirect_.uri_.c_str()
    );
    
    // ... write to permanent log ...
    ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT "location=%s",
                                      "ST : INTERNAL REDIRECT",
                                      module->ctx_.response_.redirect_.uri_.c_str()
    );
    
    // ... ngx_http_close_request should be called within finalize request ...
    if ( true == a_close_now ) {
        // ... unregister module ...
        ::ev::scheduler::Scheduler::GetInstance().Unregister(module);
        // ... and finalize request ...
        ngx_http_finalize_request(a_r, NGX_HTTP_OK);
    }
    
    //
    // NOTICE: by returning NGX_HTTP_OK previous mentioned counter it will be decreased to 1
    //         and request will be finalized by the code above that will run in the next event loop
    //        ( this must be done at rewrite handler )
    
    // .. and we're almost done ..
    return NGX_OK; 
}

/**
 * @brief Finalize a request with an error.
 *
 * @param a_module_t
 * @param a_r
 * @param a_headers
 * @param a_http_status_code
 * @param a_ngx_return_code
 * @param a_serialize
 * @param a_force
 * @param a_log_token
 */
ngx_int_t ngx::casper::broker::Module::FinalizeWithErrors (ngx_module_t& a_module_t, ngx_http_request_t* a_r,
                                                           const std::map<std::string, std::string>* a_headers,
                                                           uint16_t a_http_status_code, ngx_int_t /* a_ngx_return_code */,
                                                           bool a_serialize, bool a_force,
                                                           const std::string& a_log_token)
{
    
    ngx::casper::broker::Errors* errors_ptr = ngx::casper::broker::Tracker::GetInstance().errors_ptr(a_r);
    
    a_serialize = ( nullptr != errors_ptr );
    
    // .... if no error is set, a new one must be ....
    if ( true == a_serialize && 0 == errors_ptr->Count() ) {
        errors_ptr->Track(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_http_status_code),
                          a_http_status_code, errors_ptr->generic_error_message_key_.c_str()
        );
    }
    
    // ... serialize errors ....
    const std::string json = ( a_serialize ? errors_ptr->Serialize2JSON() : "" );
    
    // log
    NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, a_r, a_log_token.c_str(),
                                "FR", "LEAVING",
                                "%s", "with error(s) set..."
    );
    
    // ...  relinquish control of 'a_r' life cycle ...
    a_r->casper_request = 0;
    
    // ... return 'standard' error page or serialized erros as reponse ?
    if ( true == a_serialize ) {
        
        // ... use serialized errors as response ...
        ngx::casper::broker::Module::WriteResponse(a_module_t, a_r,
                                                   a_headers,
                                                   a_http_status_code, errors_ptr->content_type_.c_str(),
                                                   json.c_str(), json.length(), /* a_binary */ false,
                                                   /* a_finalize */ a_force,
                                                   a_log_token.c_str()
        );
        
    } else {
        
        // .. let nginx respond with the 'standard' error page according to status code ...
        
        // ... write to permanent log ...
        ngx::casper::broker::Module* module = static_cast<ngx::casper::broker::Module*>(ngx_http_get_module_ctx(a_r, a_module_t));
        // ... write to permanent log ...
        if ( nullptr != errors_ptr ) {
            ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                              NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",{\"status_code\":%u,\"content_type\":\"%s\",\"body\":\"%s\"}",
                                              "ST : DEBUG RESPONSE",
                                              static_cast<unsigned>(a_http_status_code), errors_ptr->content_type_.c_str(), json.c_str()
                                              );
        }
        ::ev::LoggerV2::GetInstance().Log(module->logger_client_, "cc-modules",
                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ",%u",
                                          "OUT: STATUS CODE",
                                          static_cast<unsigned>(a_http_status_code)
        );

        // ... ensure request will be finalized ...
        a_r->connection->read->eof  = 1;
        a_r->connection->write->eof = 1;
        
        // log it
        NGX_BROKER_MODULE_DEBUG_LOG(a_module_t, a_r, a_log_token.c_str(),
                                    "IT", "LEAVING",
                                    "%s", json.c_str()
        );

        if ( true == a_force ) {
            // ... unregister module ...
            ::ev::scheduler::Scheduler::GetInstance().Unregister(module);
            // ... and finalize request ...
            ngx_http_finalize_request(a_r, a_http_status_code);
        }
        
    }

    // ... we're done ...
    return NGX_HTTP_CLOSE;
}
