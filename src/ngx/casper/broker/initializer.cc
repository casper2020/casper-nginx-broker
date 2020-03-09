
/**
 * @file initializer.h
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

#include "ngx/casper/broker/initializer.h"

#include "ngx/version.h"

#include "ngx/casper/broker/module/config.h"
#include "ngx/casper/broker/i18.h"

#include "ngx/casper/broker/exception.h"
#include "ngx/casper/broker/tracker.h"

#include "ngx/casper/ev/glue.h"

#include "ev/signals.h"

#include "cc/fs/file.h"
#include "cc/fs/dir.h"

#include "cc/debug/types.h"

//
// STATIC CONST DATA
//
const char* const ngx::casper::broker::Initializer::k_resources_dir_key_lc_ = "resources_dir";

//
// STATIC DATA
//
bool ngx::casper::broker::Initializer::s_initialized_ = false;

/**
 * @brief One-shot initializer.
 *
 * @param a_r
 * @param a_configs
 */
void ngx::casper::broker::Initializer::Startup (ngx_http_request_t* a_r, const std::map<std::string, std::string>& a_configs)
{
    ngx::casper::ev::Glue::Callbacks glue_callbacks = { nullptr, nullptr };

    try {
          // ... all other stuff ...
          ngx::casper::ev::Glue::GetInstance().Startup(::cc::global::Initializer::GetInstance().loggable_data(), a_configs, glue_callbacks);
    } catch (const ::ev::Exception& a_ev_exception) {
      throw ngx::casper::broker::Exception("BROKER_MODULE_INITIALIZATION_ERROR",
                                           "Unable to initialize ev::Glue!"
      );
    }
    
    // ... casper-connectors // third party libraries ...
    ::cc::global::Initializer::GetInstance().Startup(
         /* a_signals */
         {
             /* register_ */ { SIGTTIN },
             /* callback_ */ [](const int a_sig_no) { // unhandled signals callback
                                 // ... is a 'shutdown' signal?
                                 switch(a_sig_no) {
                                     case SIGQUIT:
                                     case SIGTERM:
                                     {
                                         ngx::casper::broker::Initializer::GetInstance().Shutdown(/* a_for_cleanup_only */ false);
                                         ngx::casper::broker::Initializer::Destroy();
                                     }
                                         return true;
                                     default:
                                         return false;
                                 }
                 }
         },
         /* a_callbacks */
         {
             /* call_on_main_thread_    */ std::move(glue_callbacks.call_on_main_thread_),
             /* on_fatal_exception_     */ std::move(glue_callbacks.on_fatal_exception_)
         }
     );
      
    ngx::casper::broker::Tracker::GetInstance().Startup();
    
    if ( false == ngx::casper::broker::I18N::GetInstance().IsInitialized() ) {
        ngx::casper::broker::I18N::GetInstance().Startup(a_configs.find(k_resources_dir_key_lc_)->second,
                                                        [a_r](const char* const a_i18_key, const std::string& a_file, const std::string& a_reason) {
                                                            U_ICU_NAMESPACE::Formattable args[] = {
                                                                a_file.c_str()
                                                            };
                                                            ngx::casper::broker::Tracker::GetInstance().errors_ptr(a_r)->Track("server_error", NGX_HTTP_INTERNAL_SERVER_ERROR,
                                                                                                                               a_i18_key, args, 1, a_reason.c_str()
                                                            );
                                                        }
        );
    }
    
    if ( true == ngx::casper::broker::Tracker::GetInstance().ContainsErrors(a_r) ) {
        throw ngx::casper::broker::Exception("BROKER_MODULE_INITIALIZATION_ERROR",
                                             "Unable to initialize i18!"
        );
    }
      
    // ... mark as initialized ...
    s_initialized_ = true;
}

/**
 * @brief Dealloc previously allocated memory ( if any ).
 *
 * @param a_for_cleanup_only
 */
void ngx::casper::broker::Initializer::Shutdown (bool a_for_cleanup_only)
{
    // ... ev glue ...
    ngx::casper::ev::Glue::GetInstance().Shutdown(/* a_sig_no */ -1);
    // ... i18 ...
    ngx::casper::broker::I18N::GetInstance().Shutdown();
    // ... tracker ...
    ngx::casper::broker::Tracker::GetInstance().Shutdown();
           
    // ... casper-connectors // third party libraries cleanup ...
    ::cc::global::Initializer::GetInstance().Shutdown(a_for_cleanup_only);

    //
    // ... destructive?
    //
	if ( false == a_for_cleanup_only ) {
        // ... ev glue ...
		ngx::casper::ev::Glue::Destroy();
        // ... i18 ...
		ngx::casper::broker::I18N::Destroy();
        // ... tracker ...
        ngx::casper::broker::Tracker::Destroy();
        // ... casper-connectors // third party libraries ...
        ::cc::global::Initializer::Destroy();
        // ... casper-connectors // third party libraries cleanup ...
        ::cc::global::Initializer::Destroy();
	}
    
    // ... reset initialized flag ...
    s_initialized_ = false;
}

/**
 * @brief This method should be called prior to main event loop startup.
 *
 * @param a_config
 * @param a_cycle
 * @param a_master
 */
void ngx::casper::broker::Initializer::PreStartup (const ngx_core_conf_t* a_config, const ngx_cycle_t* a_cycle, const bool a_master)
{
    // ... reset 'forked' instances ...
    if ( false == a_master ) {
        Shutdown(/* a_for_cleanup_only */ true);
    }
    
    //
    // ... set process configuration dependent paths ...
    //..
    std::string alt_process_name;
    if ( nullptr != a_cycle && 0 != a_cycle->conf_file.len ) {
        
        const std::string conf_uri = std::string(reinterpret_cast<const char* const>(a_cycle->conf_file.data), a_cycle->conf_file.len);
        std::string conf_path;
        std::string conf_parent_path;
        
        ::cc::fs::File::Path(conf_uri, conf_path);
        ::cc::fs::Dir::Parent(conf_path, conf_parent_path);
        
        alt_process_name = std::string(conf_path.c_str() + conf_parent_path.length());
        alt_process_name = alt_process_name.substr(0, alt_process_name.length() - 1);
        
    }
    
    // ... other debug tokens ...
    CC_IF_DEBUG_DECLARE_VAR(std::set<std::string>, debug_tokens);
    CC_IF_DEBUG(
        //    debug_tokens.insert("ev_scheduler");
        //    debug_tokens.insert("ev_glue");
        //    debug_tokens.insert("ev_hub");
        //    debug_tokens.insert("ev_shared_glue");
        //    debug_tokens.insert("ev_hub_read_error");
        //    debug_tokens.insert("ev_hub_stats");
        //    debug_tokens.insert("ev_ngx_shared_handler");
        //    debug_tokens.insert("ev_subscriptions");
        //    debug_tokens.insert("PQsendQuery");
        //    debug_tokens.insert("ngx_casper_broker_module");

        //     debug_tokens.insert("oauth");
        //     debug_tokens.insert("auth");

            debug_tokens.insert("curl");
            debug_tokens.insert("exceptions");
    );
        //
    // ... initialize casper-connectors // third party libraries ...
    //
    ::cc::global::Initializer::GetInstance().WarmUp(
        /* a_process */
        {
            /* alt_name_  */ alt_process_name.c_str(),
            /* name_      */ NGX_NAME,
            /* version_   */ NGX_VERSION,
            /* info_      */ NGX_INFO,
            /* pid_       */ getpid(),
            /* is_master_ */ a_master
        },
        /* a_directories */
        nullptr, /* using defaults */
        /* a_logs */
        {
            // ... v1 ...
            // .. mandatory logs ...
            { /* token_ */ "libpq"                    , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ true, /* version */ 1 },
            { /* token_ */ "libpq-connections"        , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ true, /* version */ 2 },
            // ... v2 ...
            // .. mandatory logs ...
            { /* token_ */ "signals"                  , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ true, /* version */ 2 },
            { /* token_ */ "cc-modules"               , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ true, /* version */ 2 },
            // .. optional logs ...
            { /* token_ */ "gatekeeper"               , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ NRS_NGX_CASPER_BROKER_MODULE_CC_MODULES_LOGS_ENABLED, /* version */ 2 },
            // .. optional ( trace ) logs ...
            { /* token_ */ "redis_subscriptions_trace", /* uri_ */ "", /* conditional_ */ true, /* enabled_ */ true, /* version */ 2 },
            { /* token_ */ "redis_trace"              , /* uri_ */ "", /* conditional_ */ true, /* enabled_ */ true, /* version */ 2 }
        },
        /* a_v8 */
        {
            /* required_            */ false,
            /* runs_on_main_thread_ */ true
        },
        /* a_next_step */ {
            /* function_ */ std::bind(&ngx::casper::broker::Initializer::OnGlobalInitializationCompleted, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
            /* args_     */ (void*)(a_config)
        },
        /* a_present */ [] (std::string& o_title, std::map<std::string, std::string>& o_values) {
            o_title = "OWN MODULES";
            for ( ngx_uint_t idx = 0; ngx_modules[idx]; idx++ ) {
                if ( nullptr == strstr(ngx_modules[idx]->name, "casper")  ) {
                    if ( nullptr == strstr(ngx_modules[idx]->name, "ngx_http_named_imports_filter_module") ) {
                        continue;                        
                    }
                }
                o_values["ngx_modules[" + std::to_string(idx) + "]"] = ngx_modules[idx]->name;
            }
        },
       /* a_debug_tokens */
       CC_IF_DEBUG_ELSE(&debug_tokens, nullptr)
   );

}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Call this function to set nginx-broker process.
 *
 * @param a_process     Previously set process info.
 * @param a_directories Previously set directories info
 * @param a_arg         nginx core configuration data pointer.
 * @param o_logs        Post initialization additional logs to enable.
 */
void ngx::casper::broker::Initializer::OnGlobalInitializationCompleted (const ::cc::global::Process& a_process,
                                                                        const ::cc::global::Directories& /* a_directories */,
                                                                        const void* a_args,
                                                                        ::cc::global::Logs& /* o_logs */)
{
    //
    // ... initialize 'glue' ...
    //
    ngx::casper::ev::Glue::GetInstance().PreConfigure((const ngx_core_conf_t*)a_args, a_process.is_master_);
}

#ifdef __APPLE__
#pragma mark -
#endif

#ifdef __APPLE__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

#ifdef NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER

void ngx_http_casper_broker_sig_segv_handler (int a_sig_no)
{
    fprintf(stderr, "ngx_http_casper_broker_sig_segv_handler(a_sig_no= %d)\n", a_sig_no);
    void* callstack[128];
    int i, frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    for (i = 0; i < frames; ++i) {
        fprintf(stderr, "%s\n", strs[i]);
    }
    free(strs);
    exit(1);
}

#endif // NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER

extern "C" void ngx_http_casper_broker_signal_handler (int a_sig_no)
{
    (void)::ev::Signals::GetInstance().OnSignal(a_sig_no);
}

extern "C" void ngx_http_casper_broker_main_hook (const ngx_core_conf_t* a_config, const ngx_cycle_t* a_cycle)
{
#ifdef NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER
    signal(SIGSEGV, ngx_http_casper_broker_sig_segv_handler);
#endif // NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER
	ngx::casper::broker::Initializer::GetInstance().PreStartup(a_config, a_cycle, /* a_master*/ true);
}

extern "C" void ngx_http_casper_broker_worker_hook (const ngx_core_conf_t* a_config, const ngx_cycle_t* a_cycle)
{
    ngx::casper::broker::Initializer::GetInstance().PreStartup(a_config, a_cycle, /* a_master*/ false);
}
