
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

#include "ngx/casper/broker/module/types.h"
#include "ngx/casper/broker/module/config.h"
#include "ngx/casper/broker/module.h"
#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include "ngx/casper/broker/i18.h"

#include "ngx/casper/broker/exception.h"
#include "ngx/casper/broker/tracker.h"

#include "ngx/casper/ev/glue.h"

#include "ev/signals.h"

#include "cc/os.h"

#include "cc/fs/file.h"
#include "cc/fs/dir.h"

#include "cc/debug/types.h"

#include "cc/logs/basic.h"

#include "ngx/casper/config.h"

#if defined(NGX_HAS_MODSECURITY_MODULE) && 1 == NGX_HAS_MODSECURITY_MODULE
    #include "cc/modsecurity/processor.h"
#endif

#if defined(NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE) && 1 == NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE
  #include "casper/hsm/about.h"
  #include "casper/hsm/singleton.h"
    #include "casper/hsm/fake/api.h"
    #if !defined(__APPLE__)
        #include "casper/hsm/safenet/api.h"
    #endif
  #include "ngx/casper/broker/hsm/module/ngx_http_casper_broker_hsm_module.h"
#endif

#ifdef CC_BROKER_INITIALIZER_LOG
    #undef CC_BROKER_INITIALIZER_LOG
#endif
#define CC_BROKER_INITIALIZER_LOG(a_token, a_format, ...) \
    if ( false == ::cc::global::Initializer::GetInstance().IsBeingDebugged() ) { \
        cc::logs::Basic::GetInstance().Log(a_token, a_format, __VA_ARGS__); \
    } else { \
        fprintf(stdout, a_format, __VA_ARGS__); \
        fflush(stdout); \
    }

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
 * param a_cycle
 */
void ngx::casper::broker::Initializer::Startup (const ngx_cycle_t* a_cycle)
{
    // ... mark as 'main' thread ...
    CC_DEBUG_SET_MAIN_THREAD_ID();

    //
    // READ NGINX CORE CONFIG
    //
    const ngx_core_conf_t* config = (ngx_core_conf_t *) ngx_get_conf(a_cycle->conf_ctx, ngx_core_module);
    {

        const bool standalone = ( 0 == config->master && 0 == config->daemon );
        const bool master     = false;

        // ... reset 'forked' instances ...
        if ( false == master ) {
            Shutdown(/* a_sig_no */ -1, /* a_for_cleanup_only */ true);
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
                /* name_       */ NGX_NAME,
                /* alt_name_   */ alt_process_name.c_str(),
                /* abbr_       */ NGX_NAME,
                /* version_    */ NGX_VERSION,
                /* rel_date_   */ NGX_REL_DATE,
                /* rel_branch_ */ NGX_REL_BRANCH,
                /* rel_hash_   */ NGX_REL_HASH,
                /* rel_target_ */ NGX_REL_TARGET,
                /* info_       */ NGX_INFO,
                /* banner_     */ NGX_BANNER,
                /* pid_        */ getpid(),
                /* standalone_ */ standalone,
                /* is_master_  */ master
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
                { /* token_ */ "cc-modsecurity"           , /* uri_ */ "", /* conditional_ */ false, /* enabled_ */ NRS_NGX_CASPER_BROKER_MODULE_CC_MODULES_LOGS_ENABLED, /* version */ 2 },
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
                /* args_     */ (void*)(config)
            },
            /* a_present */ [] (std::vector<::cc::global::Initializer::Present>& o_values) {
    #if defined(NGX_HAS_MODSECURITY_MODULE) && 1 == NGX_HAS_MODSECURITY_MODULE
                {
                    o_values.push_back({
                        /* title_  */ "MODSECURITY",
                        /* values_ */ {}
                    });
                    auto& skia = o_values.back();
                    skia.values_["VERSION"] = MODSECURITY_VERSION;
                }
    #endif
    #if defined(NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE) && 1 == NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE
                {
                    o_values.push_back({
                        /* title_  */ "CASPER-HSM",
                        /* values_ */ {}
                    });
                    auto& skia = o_values.back();
                    skia.values_["VERSION"       ] = ::casper::hsm::VERSION();
                    skia.values_["RELEASE NAME"  ] = ::casper::hsm::REL_NAME();
                    skia.values_["RELEASE DATE"  ] = ::casper::hsm::REL_DATE();
                    skia.values_["RELEASE BRANCH"] = ::casper::hsm::REL_BRANCH();
                    skia.values_["RELEASE HASH"  ] = ::casper::hsm::REL_HASH();
                    skia.values_["INFO"          ] = ::casper::hsm::INFO();
                }
    #endif
                o_values.push_back({
                    /* title_  */ "OWN MODULES",
                    /* values_ */ {}
                });
                auto& modules = o_values.back();
                for ( ngx_uint_t idx = 0; ngx_modules[idx]; idx++ ) {
                    if ( nullptr == strstr(ngx_modules[idx]->name, "casper")  ) {
                        if ( nullptr == strstr(ngx_modules[idx]->name, "ngx_http_named_imports_filter_module") ) {
                            continue;
                        }
                    }
                    modules.values_["ngx_modules[" + std::to_string(idx) + "]"] = ngx_modules[idx]->name;
                }
            },
           /* a_debug_tokens */
           CC_IF_DEBUG_ELSE(&debug_tokens, nullptr)
       );

    }
    
    //
    // READ NGINX BROKER SERVICE CONFIG
    //
    std::map<std::string, std::string> configs;
    
    const nginx_broker_service_conf_t* service_conf = (const nginx_broker_service_conf_t*)ngx_http_cycle_get_module_main_conf(a_cycle, ngx_http_casper_broker_module);
    {
        // ... resources dir ...
        if ( 0 == service_conf->resources_dir.len ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "Invalid or missing '%s' directive value!", "nginx_casper_broker_resources_dir!");
        }
        configs[ngx::casper::broker::Initializer::k_resources_dir_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->resources_dir.data), service_conf->resources_dir.len);
        
        //
        // ... SERVICE ...
        //
        configs[ngx::casper::broker::Module::k_service_id_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->service_id.data), service_conf->service_id.len);
        
        //
        // ... REDIS ....
        //
        
        // ... host ...
        if ( 0 == service_conf->redis.ip_address.len ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "Invalid or missing '%s' directive value!", "nginx_casper_broker_redis_ip_address");
        }
        configs[ngx::casper::broker::Module::k_redis_ip_address_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->redis.ip_address.data), service_conf->redis.ip_address.len);
        
        // ... port ...
        configs[ngx::casper::broker::Module::k_redis_port_number_key_lc_]
            = std::to_string(service_conf->redis.port_number);
        
        // ... database ...
        if ( -1 != service_conf->redis.database ) {
            configs[ngx::casper::broker::Module::k_redis_database_key_lc_]
                = std::to_string(service_conf->redis.database);
        }
        
        configs[ngx::casper::broker::Module::k_redis_max_conn_per_worker_lc_]
            = std::to_string(service_conf->redis.max_conn_per_worker);
        
        //
        // ... POSTGRESQL ...
        //
        if ( 0 == service_conf->postgresql.conn_str.len ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "Invalid or missing '%s' directive value!", "nginx_casper_broker_postgresql_connection_string");
        }
        configs[ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->postgresql.conn_str.data), service_conf->postgresql.conn_str.len);

        configs[ngx::casper::broker::Module::k_postgresql_statement_timeout_lc_]
            = std::to_string(service_conf->postgresql.statement_timeout);

        configs[ngx::casper::broker::Module::k_postgresql_max_conn_per_worker_lc_]
            = std::to_string(service_conf->postgresql.max_conn_per_worker);
        
        if ( service_conf->postgresql.post_connect_queries.len > 0 ) {
            configs[ngx::casper::broker::Module::k_postgresql_post_connect_queries_lc_]
                = std::string(reinterpret_cast<char const*>(service_conf->postgresql.post_connect_queries.data), service_conf->postgresql.post_connect_queries.len);
        }
        
        configs[ngx::casper::broker::Module::k_postgresql_min_queries_per_conn_lc_]
            = std::to_string(service_conf->postgresql.min_queries_per_conn);
        
        configs[ngx::casper::broker::Module::k_postgresql_max_queries_per_conn_lc_]
            = std::to_string(service_conf->postgresql.max_queries_per_conn);
        
        //
        // ... CURL ...
        //
        configs[ngx::casper::broker::Module::k_curl_max_conn_per_worker_lc_]
            = std::to_string(service_conf->curl.max_conn_per_worker);
        
        //
        // ... BEANSTALKD ...
        //
        configs[ngx::casper::broker::Module::k_beanstalkd_host_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->beanstalkd.host.data), service_conf->beanstalkd.host.len);
        configs[ngx::casper::broker::Module::k_beanstalkd_port_key_lc_]
           = std::to_string(service_conf->beanstalkd.port);
        configs[ngx::casper::broker::Module::k_beanstalkd_timeout_key_lc_]
           = std::to_string(service_conf->beanstalkd.timeout);
        const std::map<const char* const, const ngx_str_t*> beanstalk_conf_strings_map = {
            { ngx::casper::broker::Module::k_beanstalkd_sessionless_tubes_key_lc_, &service_conf->beanstalkd.tubes.sessionless },
            { ngx::casper::broker::Module::k_beanstalkd_action_tubes_key_lc_     , &service_conf->beanstalkd.tubes.action      }
        };
        for ( auto bcsm_it : beanstalk_conf_strings_map ) {
            if ( bcsm_it.second->len > 0 ) {
                configs[bcsm_it.first] = std::string(reinterpret_cast<char const*>(bcsm_it.second->data), bcsm_it.second->len);
            }
        }
        
        //
        // ... GATEKEEPER ...
        //
        configs[ngx::casper::broker::Module::k_gatekeeper_config_file_uri_key_lc_]
            = std::string(reinterpret_cast<char const*>(service_conf->gatekeeper.config_file_uri.data), service_conf->gatekeeper.config_file_uri.len);
    }    

    //
    // ... ev // cc ...
    //
    ngx::casper::ev::Glue::Callbacks glue_callbacks = { nullptr, nullptr };
    try {
        // ... all other stuff ...
        ngx::casper::ev::Glue::GetInstance().Startup(::cc::global::Initializer::GetInstance().loggable_data(), configs, glue_callbacks);
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
            const auto it = configs.find(key);
            if ( configs.end() != it ) {
                configs.erase(it);
            }
        }
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {
        CC_BROKER_INITIALIZER_LOG("cc-status", "\nBROKER_MODULE_INITIALIZATION_ERROR: %s\n", a_broker_exception.what());
        throw a_broker_exception;
    } catch (const ::ev::Exception& a_ev_exception) {
        CC_BROKER_INITIALIZER_LOG("cc-status", "\nBROKER_MODULE_INITIALIZATION_ERROR: %s\n", a_ev_exception.what());
        throw ngx::casper::broker::Exception("BROKER_MODULE_INITIALIZATION_ERROR",
                                           "Unable to initialize ev::Glue!"
        );
    }
    
    // ... modsecurity ...
#if defined(NGX_HAS_MODSECURITY_MODULE) && 1 == NGX_HAS_MODSECURITY_MODULE
    try {
        cc::modsecurity::Processor::GetInstance().Startup(
            ::cc::global::Initializer::GetInstance().loggable_data()
        );
    } catch (const ::cc::Exception& a_cc_exception) {
        // ... log ...
        CC_BROKER_INITIALIZER_LOG("cc-status", "\nBROKER_MODULE_INITIALIZATION_ERROR: %s\n", a_cc_exception.what());
        // ... notify ...
        throw ngx::casper::broker::Exception("BROKER_MODULE_INITIALIZATION_ERROR",
                                             "Unable to initialize modsecurity!"
        );
    }
#endif
    
    // ... casper-connectors // third party libraries ...
    ::cc::global::Initializer::GetInstance().Startup(
         /* a_signals */
         {
             /* register_ */ { SIGTTIN },
             /* callback_ */ [](const int a_sig_no) { // unhandled signals callback
                                 // ... is a 'shutdown' signal?
                                 switch(a_sig_no) {
                                     case SIGTTIN:
                                         return false;
                                     case SIGQUIT:
                                     case SIGTERM:
                                     {
                                         ngx::casper::broker::Initializer::GetInstance().Shutdown(a_sig_no, /* a_for_cleanup_only */ false);
                                     }
                                         return true;
                                     default:
                                         return false;
                                 }
                 }
         },
         /* a_callbacks */
         {
             /* call_on_main_thread_ */ std::move(glue_callbacks.call_on_main_thread_)
         }
     );
    
    // ... HSM signals ...
#if defined(NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE) && 1 == NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE
    ::ev::Signals::GetInstance().Append({
        {
            /* signal_      */ SIGTTIN,
            /* description_ */ "HSM reload",
            /* callback_    */ [] () -> std::string {
                try {
                    // ... recycle HSM ...
                    ::casper::hsm::Singleton::GetInstance().Recycle();
                    // ... done ...
                    return "HSM reloaded";
                } catch (...) {
                    try {
                        CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
                    } catch (const ::cc::Exception& a_cc_exception) {
                        return "HSM reload failed: " +  std::string(a_cc_exception.what()) + "!";
                    }
                    return "HSM reload failed!";
                }
            }
        }
    });
#endif
    
#if defined(NGX_HAS_MODSECURITY_MODULE) && 1 == NGX_HAS_MODSECURITY_MODULE
    ::ev::Signals::GetInstance().Append({
        {
            /* signal_      */ SIGTTIN,
            /* description_ */ "Modsecurity reload",
            /* callback_    */ [] () -> std::string {
                try {
                    // ... recycle Modsecurity ...
                    cc::modsecurity::Processor::GetInstance().Recycle();
                    // ... done ...
                    return "Modsecurity reloaded";
                } catch (...) {
                    try {
                        CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
                    } catch (const ::cc::Exception& a_cc_exception) {
                        return "Modsecurity reload failed: " +  std::string(a_cc_exception.what()) + "!";
                    }
                    return "Modsecurity reload failed!";
                }
            }
        }
    });
#endif

    
    // ... error(s) tracker ...
    ngx::casper::broker::Tracker::GetInstance().Startup();
    
    // ... i18n ...
    if ( false == ngx::casper::broker::I18N::GetInstance().IsInitialized() ) {
        std::string reason;
        ngx::casper::broker::I18N::GetInstance().Startup(configs.find(k_resources_dir_key_lc_)->second,
                                                        [&reason](const char* const /* a_i18_key */, const std::string& /* a_file */, const std::string& a_reason) {
                                                            reason = a_reason;
                                                        }
        );
        if ( 0 != reason.length() ) {
            throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                                 "%s", reason.c_str());
        }
    }
    
    //
    // ... HSM ...
    //
#if defined(NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE) && 1 == NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE
    const nginx_hsm_service_conf_t* hsm_conf = (const nginx_hsm_service_conf_t*)ngx_http_cycle_get_module_main_conf(a_cycle, ngx_http_casper_broker_hsm_module);
    if ( nullptr != hsm_conf && 1 == hsm_conf->enabled ) {
        ::casper::hsm::Singleton::GetInstance().Startup(
#ifdef __APPLE__
            "/usr/local/share/nginx-hsm/",
#else
            "/usr/share/nginx-hsm/",
#endif
        {
            [&hsm_conf] () -> ::casper::hsm::API* {
                const std::string application = std::string(NGX_NAME) + "(" + std::to_string(getpid()) + ")";
            #ifdef __APPLE__
                return new ::casper::hsm::fake::API(application, std::string(reinterpret_cast<const char* const>(hsm_conf->fake.config.data), hsm_conf->fake.config.len));
            #else
                if ( 0 == hsm_conf->pin.len || 0 != hsm_conf->fake.config.len ) {
                    return new ::casper::hsm::fake::API(application, std::string(reinterpret_cast<const char* const>(hsm_conf->fake.config.data), hsm_conf->fake.config.len));
                } else {
                    return new ::casper::hsm::safenet::API(application,
                                                           static_cast<::casper::hsm::SlotID>(hsm_conf->slot_id),
                                                           std::string(reinterpret_cast<const char* const>(hsm_conf->pin.data), hsm_conf->pin.len),
                                                           /* a_reuse_session */ true
                    );
                }
            #endif
            },
            [] (const ::casper::hsm::API* a_api) -> ::casper::hsm::API* {
            #ifdef __APPLE__
                return new ::casper::hsm::fake::API(*(dynamic_cast<const ::casper::hsm::fake::API*>(a_api)));
            #else
                if ( nullptr != dynamic_cast<const ::casper::hsm::fake::API*>(a_api) ) {
                    return new ::casper::hsm::fake::API(*(dynamic_cast<const ::casper::hsm::fake::API*>(a_api)));
                } else {
                    return new ::casper::hsm::safenet::API(*(dynamic_cast<const ::casper::hsm::safenet::API*>(a_api)));
                }
            #endif
            }
        });
    }
#endif
      
    // ... mark as initialized ...
    s_initialized_ = true;
}

/**
 * @brief Dealloc previously allocated memory ( if any ).
 *
 * @param a_for_cleanup_only
 */
void ngx::casper::broker::Initializer::Shutdown (const int a_sig_no, const bool a_for_cleanup_only)
{
    // ... ev glue ...
    ngx::casper::ev::Glue::GetInstance().Shutdown(/* a_sig_no */ -1);
    // ... i18 ...
    ngx::casper::broker::I18N::GetInstance().Shutdown();
    // ... tracker ...
    ngx::casper::broker::Tracker::GetInstance().Shutdown();
    // ... HSM ...
#if defined(NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE) && 1 == NGX_HAS_CASPER_NGINX_BROKER_HSM_MODULE
    ::casper::hsm::Singleton::GetInstance().Shutdown();
#endif
    // ... MODSECURITY ...
#if defined(NGX_HAS_MODSECURITY_MODULE) && 1 == NGX_HAS_MODSECURITY_MODULE
    cc::modsecurity::Processor::GetInstance().Shutdown();
#endif
    // ... casper-connectors // third party libraries cleanup ...
    ::cc::global::Initializer::GetInstance().Shutdown(a_sig_no, a_for_cleanup_only);
    // ... reset initialized flag ...
    s_initialized_ = false;
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

extern "C" void ngx_http_casper_broker_main_hook (const ngx_core_conf_t* /* a_config */, const ngx_cycle_t* /* a_cycle */)
{
#ifdef NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER
    signal(SIGSEGV, ngx_http_casper_broker_sig_segv_handler);
#endif // NGX_CASPER_BROKER_ENABLE_SIGV_HANDLER
}

extern "C" void ngx_http_casper_broker_worker_hook (const ngx_core_conf_t* /* a_config */, const ngx_cycle_t* /* a_cycle */)
{
    /* empty */
}
