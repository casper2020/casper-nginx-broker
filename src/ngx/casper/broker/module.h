/**
 * @file module_initializer.h
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

#pragma once
#ifndef NRS_NGX_CASPER_BROKER_MODULE_H_
#define NRS_NGX_CASPER_BROKER_MODULE_H_

#include "ev/ngx/includes.h"

extern "C" {
    ngx_int_t ngx_http_access_handler(ngx_http_request_t *r);
}


#include "ev/logger_v2.h"

#include "ev/scheduler/scheduler.h"

#include "ngx/casper/broker/executor.h"
#include "ngx/casper/broker/errors.h"

#include <map>        // std::map
#include <string>     // std::string
#include <map>        // std::map
#include <functional> // std::function

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {

            namespace ext
            {
                class Base;
                class Job;
                class Session;
            }

            class Module : public ::ev::scheduler::Client
            {
                
                friend class ext::Base;
                friend class ext::Job;
                friend class ext::Session;
                
            public: // MACROS
                
#ifndef NGX_BROKER_MODULE_DECLARE_MODULE_ENABLER
#define NGX_BROKER_MODULE_DECLARE_MODULE_ENABLER static bool s_module_enabled_ = false
#endif

#ifndef NGX_BROKER_MODULE_LOC_CONF_MERGED
#define NGX_BROKER_MODULE_LOC_CONF_MERGED() \
    if ( 1 == conf->enable && false == s_module_enabled_ ) { \
        s_module_enabled_ = true; \
    }
#endif
                
#ifndef NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER
#define NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER(a_func) [&] () { \
    /* ... bail out if module is not enabled ... */ \
    if ( false == s_module_enabled_) { \
        return NGX_OK; \
    } \
    /* install content handler */ \
    ngx_http_core_main_conf_t* cmcf = (ngx_http_core_main_conf_t*) ngx_http_conf_get_module_main_conf(a_cf, ngx_http_core_module); \
    ngx_http_handler_pt*       h    = (ngx_http_handler_pt*) ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers); \
    if ( NULL == h ) { \
        return NGX_ERROR; \
    } \
    *h = a_func; \
    return NGX_OK; \
}()
#endif // NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER

#ifndef NGX_BROKER_MODULE_INSTALL_REWRITE_HANDLER
#define NGX_BROKER_MODULE_INSTALL_REWRITE_HANDLER(a_func) [&] () { \
\
    /* ... bail out if module is not enabled ... */ \
    if ( false == s_module_enabled_ ) { \
        return NGX_OK; \
    } \
    /* install rewrite handler */ \
    ngx_http_core_main_conf_t* cmcf = (ngx_http_core_main_conf_t*) ngx_http_conf_get_module_main_conf(a_cf, ngx_http_core_module); \
    ngx_http_handler_pt*       h    = (ngx_http_handler_pt*) ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers); \
    if ( NULL == h ) { \
        return NGX_ERROR; \
    } \
    *h = a_func; \
    return NGX_OK; \
}()
#endif // NGX_BROKER_MODULE_INSTALL_REWRITE_HANDLER

                //
                //
                //
                
#ifndef NGX_BROKER_MODULE_PHASE_HANDLER_BARRIER
#define NGX_BROKER_MODULE_PHASE_HANDLER_BARRIER(a_name, a_r, a_module_t, a_module_conf_t, a_log_token) \
    /* ... */ \
    ngx::casper::broker::Module::Log(a_module_t, a_r, NGX_LOG_DEBUG, a_log_token, \
                                     a_name, "ENTER", \
                                     nullptr \
    ); \
    /* ... bail out if handling a subrequest/redirect ... */ \
    if ( ( 1 == a_r->internal ) && ( a_r == a_r->main ) ) { \
        ngx::casper::broker::Module::Log(a_module_t, a_r, NGX_LOG_DEBUG, a_log_token, \
                                         a_name, "LEAVING", \
                                         "redirect / subrequest ... skipping..." \
        ); \
        /* ... by declining request ... */ \
        return NGX_DECLINED; \
    } \
    /* ... grab local conf ... */ \
    a_module_conf_t* loc_conf = (a_module_conf_t*) ngx_http_get_module_loc_conf(a_r, a_module_t); \
    /* ... and bail out if the module is not enabled ... */ \
    if ( 1 != loc_conf->enable ) { \
        ngx::casper::broker::Module::Log(a_module_t, a_r, NGX_LOG_DEBUG, a_log_token, \
                                         a_name, "LEAVING", \
                                         "module not enabled... skipping..." \
        ); \
        /* ... by declining request ... */ \
        return NGX_DECLINED; \
    }
#endif // NGX_BROKER_MODULE_PHASE_HANDLER_BARRIER
                
#ifndef NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER
#define NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER(a_r, a_module_t, a_module_conf_t, a_log_token) \
        NGX_BROKER_MODULE_PHASE_HANDLER_BARRIER("CH", a_r, a_module_t, a_module_conf_t, a_log_token);
#endif // NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER

#ifndef NGX_BROKER_MODULE_REWRITE_HANDLER_BARRIER
#define NGX_BROKER_MODULE_REWRITE_HANDLER_BARRIER(a_r, a_module_t, a_module_conf_t, a_log_token) \
    const ngx_int_t access_rv = ngx_http_access_handler(a_r); \
    if ( not ( NGX_OK == access_rv || NGX_DECLINED == access_rv ) ) { \
        return access_rv; \
    } \
    NGX_BROKER_MODULE_PHASE_HANDLER_BARRIER("RW", a_r, a_module_t, a_module_conf_t, a_log_token);
#endif // NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER

                //
                //
                //
                
#ifndef NGX_BROKER_MODULE_LOG
    #if defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) || defined(ENABLE_DEBUG_TRACE)
        #define NGX_BROKER_MODULE_LOG(a_ctx, a_location, a_action, a_format, ...) \
                ngx::casper::broker::Module::Log(a_ctx.module_, a_ctx.ngx_ptr_, a_ctx.log_level_, a_ctx.log_token_.c_str(), \
                                                 a_location, a_action, \
                                                 a_format, __VA_ARGS__ \
                );
    #else
        #define NGX_BROKER_MODULE_LOG(a_ctx, a_location, a_action, a_format, ...) \
            if ( NGX_LOG_DEBUG != a_ctx.log_level_ ) { \
                ngx::casper::broker::Module::Log(a_ctx.module_, a_ctx.ngx_ptr_, a_ctx.log_level_, a_ctx.log_token_.c_str(), \
                                                 a_location, a_action, \
                                                 a_format, __VA_ARGS__ \
                ); \
            }
    #endif
#endif // NGX_BROKER_MODULE_LOG

#ifndef NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT
    #define NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT "%-28.28s"
#endif
                
#ifndef NGX_BROKER_MODULE_DEBUG_LOG
    #if defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) // OSALITE_DEBUG
        #define NGX_BROKER_MODULE_DEBUG_LOG(aa_module_t, aa_r, aa_token, aa_location, aa_action, aa_format, ...) \
            ngx::casper::broker::Module::Log(aa_module_t, aa_r, NGX_LOG_DEBUG, aa_token, aa_location, aa_action, aa_format, __VA_ARGS__);
    #else
        #define NGX_BROKER_MODULE_DEBUG_LOG(aa_module_t, aa_r, aa_token, aa_location, aa_action, aa_format, ...)
    #endif // OSALITE_DEBUG
#endif // NGX_BROKER_MODULE_DEBUG_LOG

#ifndef NGX_BROKER_MODULE_ERROR_LOG
    #define NGX_BROKER_MODULE_ERROR_LOG(aa_module_t, aa_r, aa_token, aa_location, aa_action, aa_format, ...) \
        ngx::casper::broker::Module::Log(aa_module_t, aa_r, NGX_LOG_ERR, aa_token, aa_location, aa_action, aa_format, __VA_ARGS__);
#endif // NGX_BROKER_MODULE_ERROR_LOG

                //
                //
                //
                
#ifndef NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR
#define NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_code) [&] () { \
        const auto it = ngx::casper::broker::Module::k_short_http_status_codes_map_.find(a_code); \
        if ( ngx::casper::broker::Module::k_short_http_status_codes_map_.end() != it ) { \
            return it->second.c_str(); \
        } else { \
            return ""; \
        } \
    }()
#endif // NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR
                
#ifndef NGX_BROKER_MODULE_THROW_EXCEPTION
    #define NGX_BROKER_MODULE_THROW_EXCEPTION(a_ctx, a_format, ...) \
            throw ngx::casper::broker::Exception(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_format, __VA_ARGS__);
#endif // NGX_BROKER_MODULE_THROW_EXCEPTION
                
#ifndef NGX_BROKER_MODULE_TRACK_EXCEPTION
    #define NGX_BROKER_MODULE_TRACK_EXCEPTION(a_ctx, a_std_exception) \
                a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), ctx_.response_.status_code_, \
                    a_ctx.errors_ptr_->generic_error_message_key_.c_str(), a_std_exception.what() \
                );
#endif // NGX_BROKER_MODULE_TRACK_EXCEPTION
                
#ifndef NGX_BROKER_MODULE_RESET_REDIRECT
    #define NGX_BROKER_MODULE_RESET_REDIRECT(a_ctx) \
        ctx_.response_.redirect_.protocol_     = "";    \
        ctx_.response_.redirect_.host_         = "";    \
        ctx_.response_.redirect_.port_         = 0;     \
        ctx_.response_.redirect_.path_         = "";    \
        ctx_.response_.redirect_.file_         = "";    \
        ctx_.response_.redirect_.uri_          = "";    \
        ctx_.response_.redirect_.asynchronous_ = false; \
        ctx_.response_.redirect_.internal_     = false; \
        ctx_.response_.redirect_.location_     = "";
#endif // NGX_BROKER_MODULE_RESET_REDIRECT
                
                //
                //
                //
                
#ifndef NGX_BROKER_MODULE_SET_NO_CONTENT
   #define NGX_BROKER_MODULE_SET_NO_CONTENT(a_ctx) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_NO_CONTENT; \
        a_ctx.response_.return_code_ = NGX_OK; \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_NO_CONTENT
                
#ifndef NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED
    #define NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(a_ctx) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_NOT_IMPLEMENTED; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        const char* const http_short_status_code = NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_); \
        const auto method_it = ngx::casper::broker::Module::k_http_methods_map_.find((uint32_t)a_ctx.ngx_ptr_->method); \
        if ( ngx::casper::broker::Module::k_http_methods_map_.end() != method_it ) { \
            U_ICU_NAMESPACE::Formattable args[] = { \
                method_it->second.c_str() \
            }; \
            a_ctx.response_.errors_tracker_.add_i18n_v_(http_short_status_code, a_ctx.response_.status_code_, \
                "BROKER_METHOD_NOT_IMPLEMENTED_ERROR", args, 1 \
            ); \
        } else { \
            U_ICU_NAMESPACE::Formattable args[] = { \
                (int32_t)a_ctx.ngx_ptr_->method \
            }; \
            a_ctx.response_.errors_tracker_.add_i18n_v_(http_short_status_code, a_ctx.response_.status_code_, \
                "BROKER_METHOD_NOT_IMPLEMENTED_ERROR", args, 1 \
            ); \
        } \
        return a_ctx.response_.status_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED
                
#ifndef NGX_BROKER_MODULE_SET_ACTION_NOT_IMPLEMENTED
    #define NGX_BROKER_MODULE_SET_ACTION_NOT_IMPLEMENTED(a_ctx, a_name) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        U_ICU_NAMESPACE::Formattable args [] = { \
            a_name \
        }; \
        a_ctx.response_.errors_tracker_.add_i18n_v_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), ctx_.response_.status_code_, \
            "BROKER_ACTION_NOT_IMPLEMENTED_ERROR", args, 1 \
        ); \
        return a_ctx.response_.return_code_; \
    } ()
#endif // NGX_BROKER_MODULE_SET_ACTION_NOT_IMPLEMENTED
      
#ifndef NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR
    #define NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(a_ctx, a_internal_msg) \
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N(a_ctx, a_ctx.errors_ptr_->generic_error_message_key_.c_str(), a_internal_msg)
#endif // NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR

#ifndef NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR
    #define NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_NOT_FOUND; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_i_( \
            NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
            "BROKER_NOT_FOUND_ERROR", \
            a_internal_msg \
        ); \
        return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR

#ifndef NGX_BROKER_MODULE_SET_FORBIDDEN_ERROR
#define NGX_BROKER_MODULE_SET_FORBIDDEN_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
    a_ctx.response_.status_code_ = NGX_HTTP_FORBIDDEN; \
    a_ctx.response_.return_code_ = NGX_ERROR; \
    a_ctx.response_.errors_tracker_.add_i18n_i_( \
        NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
        "BROKER_FORBIDDEN_ERROR", \
        a_internal_msg \
    ); \
    return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_FORBIDDEN_ERROR
                
#ifndef NGX_BROKER_MODULE_SET_CONFLICT_ERROR
#define NGX_BROKER_MODULE_SET_CONFLICT_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
    a_ctx.response_.status_code_ = NGX_HTTP_CONFLICT; \
    a_ctx.response_.return_code_ = NGX_ERROR; \
    a_ctx.response_.errors_tracker_.add_i18n_i_( \
        NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
        "BROKER_CONFLICT_ERROR", \
        a_internal_msg \
    ); \
    return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_CONFLICT_ERROR

#ifndef NGX_BROKER_MODULE_SET_BAD_REQUEST
#define NGX_BROKER_MODULE_SET_BAD_REQUEST(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
    a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
    a_ctx.response_.return_code_ = NGX_ERROR; \
    a_ctx.response_.errors_tracker_.add_i18n_i_(\
        NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
        "BROKER_BAD_REQUEST_ERROR", \
        a_internal_msg \
    ); \
    return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_BAD_REQUEST

#ifndef NGX_BROKER_MODULE_SET_NOT_ALLOWED
#define NGX_BROKER_MODULE_SET_NOT_ALLOWED(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
    a_ctx.response_.status_code_ = NGX_HTTP_NOT_ALLOWED; \
    a_ctx.response_.return_code_ = NGX_ERROR; \
     a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
           "BROKER_NOT_ALLOWED", \
           a_internal_msg \
       ); \
    return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_NOT_ALLOWED
                
#ifndef NGX_BROKER_MODULE_SET_SERVER_ERROR
#define NGX_BROKER_MODULE_SET_SERVER_ERROR(a_ctx, a_code, a_internal_msg) [&] () -> ngx_int_t { \
    a_ctx.response_.status_code_ = a_code; \
    a_ctx.response_.return_code_ = NGX_ERROR; \
    a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
        "BROKER_AN_ERROR_OCCURRED_MESSAGE", \
        a_internal_msg \
    ); \
    return a_ctx.response_.return_code_; \
}()
#endif // NGX_BROKER_MODULE_SET_SERVER_ERROR

#ifndef NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N
    #define NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N(a_ctx, a_msg_key, a_internal_msg) [&] () -> ngx_int_t { \
            a_ctx.response_.status_code_ = NGX_HTTP_INTERNAL_SERVER_ERROR; \
            a_ctx.response_.return_code_ = NGX_ERROR; \
            a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
                a_msg_key, \
                a_internal_msg \
            ); \
            return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N

#ifndef NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION
    #define NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(a_ctx, a_exception) [&] () -> ngx_int_t { \
            a_ctx.response_.status_code_ = NGX_HTTP_INTERNAL_SERVER_ERROR; \
            a_ctx.response_.return_code_ = NGX_ERROR; \
            a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
                a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
                a_exception.what() \
            ); \
            return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION
                
#ifndef NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR
    #define NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
            a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
            a_internal_msg \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR

#ifndef NGX_BROKER_MODULE_SET_PAYLOAD_TOO_LARGE_ERROR
    #define NGX_BROKER_MODULE_SET_PAYLOAD_TOO_LARGE_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_REQUEST_ENTITY_TOO_LARGE; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
            a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
            a_internal_msg \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_PAYLOAD_TOO_LARGE_ERROR
                
#ifndef NGX_BROKER_MODULE_SET_GATEWAY_TIMEOUT_ERROR
    #define NGX_BROKER_MODULE_SET_GATEWAY_TIMEOUT_ERROR(a_ctx, a_internal_msg) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_GATEWAY_TIME_OUT; \
        a_ctx.response_.return_code_ = NGX_OK; \
        a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
            a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
            a_internal_msg \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_GATEWAY_TIMEOUT_ERROR

#ifndef NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N
    #define NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(a_ctx, a_msg_key) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
                                                  a_msg_key \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N
                
                //
                // NGX_BROKER_MODULE_SET_ERROR_I18N ...
                //

#ifndef NGX_BROKER_MODULE_SET_ERROR_I18N
    #define NGX_BROKER_MODULE_SET_ERROR_I18N(a_ctx, a_status_code, a_msg_key) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = a_status_code; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_status_code), a_status_code, \
                                                  a_msg_key \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_ERROR_I18N
                
#ifndef NGX_BROKER_MODULE_SET_ERROR_I18N_I
    #define NGX_BROKER_MODULE_SET_ERROR_I18N_I(a_ctx, a_status_code, a_msg_key, a_internal_msg) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = a_status_code; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_status_code), a_status_code, \
                                                    a_msg_key, \
                                                    a_internal_msg \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_ERROR_I18N_I
                
#ifndef NGX_BROKER_MODULE_SET_ERROR_I18N_V
    #define NGX_BROKER_MODULE_SET_ERROR_I18N_V(a_ctx, a_status_code, a_msg_key, a_msg_args, a_msg_args_count) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_ = a_status_code; \
        a_ctx.response_.return_code_ = NGX_ERROR; \
        a_ctx.response_.errors_tracker_.add_i18n_v_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_status_code), a_status_code, \
                                                    a_msg_key, a_msg_args, a_msg_args_count \
        ); \
        return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_ERROR_I18N_V
                
#ifndef NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N_V
    #define NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N_V(a_ctx, a_msg_key, a_msg_args, a_msg_args_count) \
                NGX_BROKER_MODULE_SET_ERROR_I18N_V(a_ctx, NGX_HTTP_BAD_REQUEST, a_msg_key, a_msg_args, a_msg_args_count)
#endif // NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N_V
                
#ifndef NGX_BROKER_MODULE_SET_UNSUPPORTED_MEDIA_TYPE_I18N_V
        #define NGX_BROKER_MODULE_SET_UNSUPPORTED_MEDIA_TYPE_I18N_V(a_ctx, a_msg_key, a_msg_args, a_msg_args_count) \
                    NGX_BROKER_MODULE_SET_ERROR_I18N_V(a_ctx, NGX_HTTP_UNSUPPORTED_MEDIA_TYPE, a_msg_key, a_msg_args, a_msg_args_count)
#endif  // NGX_BROKER_MODULE_SET_UNSUPPORTED_MEDIA_TYPE_I18N_V

                //
                //
                //

                
#ifndef NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION
    #define NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(a_ctx, a_exception) [&] () -> ngx_int_t { \
            a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
            a_ctx.response_.return_code_ = NGX_ERROR; \
            a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
                a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
                a_exception.what() \
            ); \
            return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION
                
#ifndef NGX_BROKER_MODULE_TRACK_GENERIC_ERROR
    #define NGX_BROKER_MODULE_TRACK_GENERIC_ERROR(a_ctx, a_internal_msg) [&]() -> ngx_int_t { \
                a_ctx.response_.errors_tracker_.add_i18n_i_(NGX_BROKER_MODULE_HTTP_SHORT_STATUS_CODE_STR(a_ctx.response_.status_code_), a_ctx.response_.status_code_, \
                    a_ctx.errors_ptr_->generic_error_message_key_.c_str(), \
                    a_internal_msg \
                ); \
                return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_TRACK_GENERIC_ERROR
         
#ifndef NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR
    #define NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(a_ctx, a_name) [&] () -> ngx_int_t { \
            a_ctx.response_.status_code_ = NGX_HTTP_BAD_REQUEST; \
            a_ctx.response_.return_code_ = NGX_ERROR; \
            U_ICU_NAMESPACE::Formattable args[] = { \
                a_name \
            }; \
            a_ctx.response_.errors_tracker_.add_i18n_v_("bad_request", a_ctx.response_.status_code_, \
                "BROKER_INVALID_OR_MISSING_JSON_OBJECT_FIELD", args, 1 \
            ); \
            return a_ctx.response_.return_code_; \
    }()
#endif // NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR
                

                //
                // NGX_BROKER_MODULE_SET_RESPONSE
                //

#ifndef NGX_BROKER_MODULE_SET_RESPONSE
#define NGX_BROKER_MODULE_SET_RESPONSE(a_ctx, a_status_code, a_content_type, a_content) [&] () -> ngx_int_t { \
        a_ctx.response_.status_code_  = a_status_code; \
        a_ctx.response_.return_code_  = NGX_OK; \
        a_ctx.response_.content_type_ = a_content_type; \
        a_ctx.response_.body_         = a_content; \
        a_ctx.response_.length_       = a_ctx.response_.body_.length(); \
        return a_ctx.response_.return_code_; \
    } ()
#endif // NGX_BROKER_MODULE_SET_RESPONSE
                
                //
                // NGX_BROKER_MODULE_FINALIZE_REQUEST_XXX
                //

#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST
    #define NGX_BROKER_MODULE_FINALIZE_REQUEST(a_module) \
        a_module->ctx_.response_.asynchronous_ = false; \
        ngx::casper::broker::Module::FinalizeRequest(a_module);
#endif

#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT
#define NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT(a_module) \
        a_module->ctx_.response_.status_code_  = NGX_HTTP_MOVED_TEMPORARILY; \
        a_module->ctx_.response_.return_code_  = NGX_HTTP_MOVED_TEMPORARILY; \
        a_module->ctx_.response_.asynchronous_ = false; \
        ngx::casper::broker::Module::FinalizeRequest(a_module);
#endif // NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_REDIRECT
                
#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE
    #define NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(a_module, a_status_code, a_content_type, a_content) \
        a_module->ctx_.response_.status_code_  = a_status_code; \
        a_module->ctx_.response_.return_code_  = NGX_OK; \
        a_module->ctx_.response_.content_type_ = a_content_type; \
        a_module->ctx_.response_.body_         = a_content; \
        a_module->ctx_.response_.length_       = a_module->ctx_.response_.body_.length(); \
        a_module->ctx_.response_.asynchronous_ = false; \
        ngx::casper::broker::Module::FinalizeRequest(a_module);
#endif // NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE

#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE_AND_LENGTH
    #define NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE_AND_LENGTH(a_module, a_status_code, a_content_type, a_content, a_length, a_binary) \
        a_module->ctx_.response_.status_code_  = a_status_code; \
        a_module->ctx_.response_.return_code_  = NGX_OK; \
        a_module->ctx_.response_.content_type_ = a_content_type; \
        a_module->ctx_.response_.body_         = a_content; \
        a_module->ctx_.response_.length_       = a_length; \
        a_module->ctx_.response_.binary_       = a_binary; \
        a_module->ctx_.response_.asynchronous_ = false; \
        ngx::casper::broker::Module::FinalizeRequest(a_module);
#endif // NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE_AND_LENGTH

                
#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE
    #define NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(a_module) \
            a_module->ctx_.response_.asynchronous_ = false; \
            a_module->ctx_.response_.return_code_ = NGX_ERROR; \
            if ( 0 == a_module->ctx_.errors_ptr_->Count() ) { \
                a_module->ctx_.response_.status_code_ = NGX_HTTP_INTERNAL_SERVER_ERROR; \
                NGX_BROKER_MODULE_TRACK_GENERIC_ERROR(a_module->ctx_, nullptr); \
            } \
            ngx::casper::broker::Module::FinalizeRequest(a_module);
#endif // NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE
            
#ifndef NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT
#define NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(a_module) [&]() -> ngx_int_t { \
            return ngx::casper::broker::Module::FinalizeWithInternalRedirect(a_module); \
    } ()
#endif // NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT
                
                //
                //
                //
                
            protected: // Data Type(s)
                
                typedef std::function<Module*  ()>                                        ModuleFactory;
                typedef std::function<Executor*(broker::ErrorsTracker& a_errors_tracker)> ExecutorFactory;
                typedef std::function<Errors*  (const std::string& a_locale)>             ErrorsFactory;
                
                typedef struct _Config {
                    ngx_module_t&                   ngx_module_;
                    ngx_http_request_t*             ngx_ptr_;
                    ngx_http_client_body_handler_pt ngx_body_read_handler_;
                    ngx_http_cleanup_pt             ngx_cleanup_handler_;
                    const char* const               rx_content_type_;
                    const char* const               tx_content_type_;
                    const std::string               log_token_;
                    ErrorsFactory                   errors_factory_;
                    ExecutorFactory                 executor_factory_;
                    const std::string               landing_page_url_;
                    const std::string               error_page_url_;
                    bool                            serialize_errors_;
                    bool                            at_rewrite_handler_;
                } Config;
                                        
                typedef struct _Params {
                    std::map<std::string, std::string> in_headers_;
                    std::map<std::string, std::string> config_;
                    std::string                        locale_;
                    std::set<std::string>              supported_content_types_;
                } Params;
                
                struct StringMapCaseInsensitiveComparator : public std::binary_function<std::string, std::string, bool> {
                     bool operator()(const std::string& a_lhs, const std::string& a_rhs) const {
                         return strcasecmp(a_lhs.c_str(), a_rhs.c_str()) < 0;
                     }
                 };
                
            protected: // Data Types

                typedef struct _Cookie {
                    std::string name_;      //!<
                    std::string value_;     //!<
                    std::string domain_;    //!<
                    std::string path_;      //!<
                    std::string expires_;   //!< a specific date and time for when the browser should delete the cookie - format: Wdy, DD Mon YYYY HH:MM:SS GMT
                    bool        secure_;    //!< meant to keep cookie communication limited to encrypted transmission
                    bool        http_only_; //!< not to expose cookies through channels other than HTTP (and HTTPS) requests - JavaScript won't be able to access it!
                    std::string inline_;    //!<
                } Cookie;
                
                typedef struct {
                    std::string         protocol_;
                    std::string         host_;
                    ngx_uint_t          port_;
                    std::string         path_;
                    std::string         file_;
                    std::string         uri_;
                    bool                internal_;
                    bool                asynchronous_;
                    std::string         location_;
                    std::set<ngx_int_t> supported_methods_;
                } RedirectData;
                
                typedef struct {
                    std::function<const std::string&()>                                             dst_;
                    std::function<ssize_t(const unsigned char* const a_data, const size_t& a_size)> writer_;
                    std::function<ssize_t()>                                                        flush_;
                    std::function<ssize_t()>                                                        close_;
                } BodyInterceptor;

                typedef struct _Request {
                    const std::string                  uri_;
                    const std::string                  method_;
                    const std::string                  locale_;
                    std::string                        location_;
                    std::string                        content_type_;   //!< Payload HTTP content type.
                    std::map<std::string, std::string> headers_;            //!<
                    char*                              body_;
                    BodyInterceptor                    body_interceptor_;
                    std::string                        browser_;
                    ngx_http_client_body_handler_pt    ngx_body_read_handler_;
                    std::function<void()>              body_read_completed_;
                    size_t                             connection_validity_;
                    Cookie                             session_cookie_;
                } Request;
                
                typedef struct _Response {
                    std::map<std::string, std::string> headers_;            //!<
                    std::string                        content_type_;       //!< Payload content type.
                    std::string                        body_;               //!< Payload.
                    size_t                             length_;             //!< Payload length, optional for binary payloads
                    bool                               binary_;             //!<
                    uint16_t                           status_code_;        //!< HTTP status code.
                    ngx_int_t                          return_code_;        //!<
                    RedirectData                       redirect_;           //!<
                    std::string                        landing_page_url_;   //!<
                    std::string                        error_page_url_;     //!<
                    broker::ErrorsTracker              errors_tracker_;     //!<
                    bool                               asynchronous_;       //!< True when response will be asynchronous, false otherwise.
                    bool                               serialize_errors_;   //!< When true, on error response will be the serialization of the collected errors otherwise standard error pages.
                } Response;
                
                typedef struct {
                    ngx_module_t&         module_;
                    ngx_http_request_t*   ngx_ptr_;
                    Request               request_;
                    Response              response_;
                    const std::string     log_token_;
                    ngx_uint_t            log_level_;
                    bool                  at_rewrite_handler_;
                    bool                  at_content_handler_;
                    ngx_uint_t            cycle_cnt_;
                    Errors*               errors_ptr_;
                    ::ev::Loggable::Data& loggable_data_ref_;
                    bool                  log_body_;
                } CTX;
                
            public: // Static Const Data
                
                static const char* const k_content_type_header_key_;
                
            public: // Static Const Data
                
                static const char* const k_service_id_key_lc_;
                static const char* const k_connection_validity_key_lc_;
                static const char* const k_redis_ip_address_key_lc_;
                static const char* const k_redis_port_number_key_lc_;
                static const char* const k_redis_database_key_lc_;
                static const char* const k_redis_max_conn_per_worker_lc_;
                static const char* const k_postgresql_conn_str_key_lc_;
                static const char* const k_postgresql_statement_timeout_lc_;
                static const char* const k_postgresql_max_conn_per_worker_lc_;
                static const char* const k_postgresql_post_connect_queries_lc_;
                static const char* const k_postgresql_min_queries_per_conn_lc_;
                static const char* const k_postgresql_max_queries_per_conn_lc_;
                static const char* const k_curl_max_conn_per_worker_lc_;
                static const char* const k_beanstalkd_host_key_lc_;
                static const char* const k_beanstalkd_port_key_lc_;
                static const char* const k_beanstalkd_timeout_key_lc_;
                static const char* const k_beanstalkd_sessionless_tubes_key_lc_;
                static const char* const k_beanstalkd_action_tubes_key_lc_;
                static const char* const k_session_cookie_name_key_lc_;
                static const char* const k_session_cookie_domain_key_lc_;
                static const char* const k_session_cookie_path_key_lc_;                
                static const char* const k_gatekeeper_config_file_uri_key_lc_;

                static const std::map<uint32_t, std::string> k_http_methods_map_;
                static const std::map<uint16_t, std::string> k_short_http_status_codes_map_;

            private: //
                
                ::ev::Loggable::Data loggable_data_;

            private: // Ptrs

                ::ev::LoggerV2::Client* logger_client_;

            protected: // Data
                
                CTX                     ctx_;
                Executor*               executor_;
                std::string             service_id_;
                std::set<ngx_int_t>     body_read_supported_methods_;
                std::set<ngx_int_t>     body_read_bypass_methods_;
                std::set<ngx_int_t>     body_read_allow_empty_methods_;
                
            public: // Constructor(s) / Destructor
                
                Module (const char* const a_name, const Config& a_config, const Params& a_params);
                virtual ~Module();
                
            protected: // Virtual Method(s) / Function(s)
                
                virtual ngx_int_t Setup ();
                virtual ngx_int_t Run   () = 0;
                
            protected: // Method(s) / Function(s)
                
                ngx_int_t              ValidateRequest (std::function<void()> a_callback);
                ::ev::scheduler::Task* NewTask         (const EV_TASK_PARAMS& a_callback);
                
                ::ev::LoggerV2::Client* logger_client ();
                
            protected: // Static Method(s) / Function(s)
                
                static ngx_int_t Initialize (const Config& a_config, Params& a_params,
                                             const std::function<::ngx::casper::broker::Module*()>& a_module);
			public:
				
                static void ReadBody      (ngx_module_t& a_module, ngx_http_request_t* a_r);
                static void WriteResponse (ngx_module_t& a_module, ngx_http_request_t* a_r);
                
                static void WriteResponse (ngx_module_t& a_module, ngx_http_request_t* a_r, const std::map<std::string, std::string>* a_headers,
                                           ngx_uint_t a_status_code, const char* const a_content_type,
                                           const char* const a_data, size_t a_length, const bool a_binary,
                                           const bool a_finalize,
                                           const char* const a_log_token, const bool a_plain_module = false);

                static void Redirect      (ngx_module_t& a_module, ngx_http_request_t* a_r,
                                           const char* const a_location, const char* const a_log_token);

                static void Redirect      (ngx_module_t& a_module, ngx_http_request_t* a_r,
                                           const char* const a_location,
                                           const std::map<std::string, std::string>& a_headers,
                                           const bool a_finalize,
                                           const char* const a_log_token);

                static void SetOutHeaders (ngx_module_t& a_module, ngx_http_request_t* a_r,
                                           const std::map<std::string, std::string>& a_headers);
                
                static void Cleanup       (ngx_module_t& a_module_t, void* a_data);
                
                static void SetCookie     (const std::string& a_name, const std::string& a_value,
                                           const std::string& a_domain, const std::string& a_path,
                                           const bool a_secure, const bool a_http_only,
                                           const int64_t&  a_expires_at,
                                           Cookie& o_cookie);

                
                static ngx_int_t FinalizeRequest               (Module* a_module,
                                                                const uint16_t* a_http_status_code = nullptr, const ngx_int_t* a_ngx_return_code = nullptr,
                                                                bool a_force = true);
                static ngx_int_t FinalizeWithInternalRedirect  (Module* a_module);

            public: // Static Method(s) / Function(s)
                
                static void Log (ngx_module_t& a_module_t, ngx_http_request_t* a_r, ngx_uint_t a_level,
                                 const char* const a_token, const char* const a_location, const char* const a_action,
                                 const char* const a_format, ...); // __attribute__((format(printf, 8, 9)));
                
            protected: // Static Method(s) / Function(s)

                static ngx_int_t WarmUp           (ngx_module_t& a_module_t, ngx_http_request_t* a_r, const ngx_str_t& a_log_token,
                                                   Params& o_params);
                
                static ngx_int_t EnsureDirectives (ngx_module_t& a_module_t, ngx_http_request_t* a_r, const ngx_str_t& a_log_token,
                                                   const std::map<std::string, ngx_str_t*>& a_map, Errors* a_errors);
                
                static std::string GetMethodName       (const ngx_http_request_t* a_r);                
                static void        GetUntouchedHeaders (const ngx_http_request_t* a_r, Json::Value& o_array);
                
            private: // Static Mehtod(s) / Function()
                                
                static ngx_int_t InternalRedirect (ngx_module_t& a_module_t, ngx_http_request_t* a_r,
                                                   bool a_close_now);
                
                static ngx_int_t FinalizeWithErrors  (ngx_module_t& a_module_t, ngx_http_request_t* a_r,
                                                      const std::map<std::string, std::string>* a_headers,
                                                      uint16_t a_http_status_code, ngx_int_t a_ngx_return_code,
                                                      bool a_serialize, bool a_force,
                                                      const std::string& a_log_token);
                

            public: // Inline Method(s) / Function(s)
                
                bool SynchronousResponse () const;
                bool InternalRedirect    () const;
                bool NoContentResponse   () const;
                void SetAtContentHandler ();
                void SetAtRewriteHandler ();
                
            }; // end of class 'Module'

            inline bool Module::SynchronousResponse () const
            {
                return ( false == ctx_.response_.asynchronous_ );
            }
            
            inline bool Module::InternalRedirect () const
            {
                return ctx_.response_.redirect_.internal_;
            }
            
            inline bool Module::NoContentResponse () const
            {
                return ( NGX_OK == ctx_.response_.return_code_ && NGX_HTTP_NO_CONTENT == ctx_.response_.status_code_ );
            }
            
            inline void Module::SetAtContentHandler ()
            {
                ctx_.at_rewrite_handler_ = false;
                ctx_.at_content_handler_ = true;
            }
            
            inline void Module::SetAtRewriteHandler ()
            {
                ctx_.at_rewrite_handler_ = true;
                ctx_.at_content_handler_ = false;
            }
        
            inline ::ev::LoggerV2::Client* Module::logger_client ()
            {
                return logger_client_;
            }
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_MODULE_H_
