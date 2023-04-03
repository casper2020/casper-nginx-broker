/**
 * @file glue.cc
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

#include "ngx/casper/ev/glue.h"

#include "ev/exception.h"
#include "ev/signals.h"

#include "ev/redis/device.h"
#include "ev/redis/request.h"

#include "ev/postgresql/device.h"
#include "ev/postgresql/object.h"
#include "ev/postgresql/request.h"

#include "ev/curl/device.h"

#include "ev/scheduler/unique_id_generator.h"

#include "ev/ngx/bridge.h"

#include "ngx/casper/broker/module.h"

#include "ev/redis/subscriptions/manager.h"

#include "ev/auth/route/gatekeeper.h"

#include "osal/utf8_string.h"

#include "ngx/version.h"

bool                        ngx::casper::ev::Glue::s_initialized_       = false;
const ::ev::Loggable::Data* ngx::casper::ev::Glue::s_loggable_data_ptr_ = nullptr;

/**
 * @brief One-shot initializer.
 *
 * @param a_loggable_data_ref
 * @param a_config
 * @param a_callbacks
 */
void ngx::casper::ev::Glue::Startup (const ::ev::Loggable::Data& a_loggable_data_ref, const std::map<std::string, std::string>& a_config,
                                     Callbacks& o_callbacks)
{
    OSALITE_DEBUG_TRACE("ev_glue", "~> Startup(...)");

    OSALITE_ASSERT(1 == PQisthreadsafe());

    // ... first, a sanity check ...
    if ( true == ngx::casper::ev::Glue::s_initialized_ ) {
        // ... insane call ...
        throw ::ev::Exception("casper::ev::Glue - already initialized!");
    }
    
    // ... track loggable data ...
    ngx::casper::ev::Glue::s_loggable_data_ptr_ = &a_loggable_data_ref;
    
    // ... gatekeeper ...
    auto gatekeeper_conf_file_uri_it = a_config.find(::ngx::casper::broker::Module::k_gatekeeper_config_file_uri_key_lc_);
    ::ev::auth::route::Gatekeeper::GetInstance().Startup(*::ngx::casper::ev::Glue::s_loggable_data_ptr_,
                                                         ( a_config.end() != gatekeeper_conf_file_uri_it ? gatekeeper_conf_file_uri_it->second : "" )
    );

    // ... PostgreSQL, REDIS cURL & Beanstalk
    SetupService(a_config,
                ::ngx::casper::broker::Module::k_service_id_key_lc_
    );
    
    SetupPostgreSQL(a_config,
                    ::ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_,
                    ::ngx::casper::broker::Module::k_postgresql_statement_timeout_lc_,
                    ::ngx::casper::broker::Module::k_postgresql_max_conn_per_worker_lc_,
                    ::ngx::casper::broker::Module::k_postgresql_min_queries_per_conn_lc_,
                    ::ngx::casper::broker::Module::k_postgresql_max_queries_per_conn_lc_,
                    ::ngx::casper::broker::Module::k_postgresql_post_connect_queries_lc_
    );
    
    SetupREDIS(a_config,
               ::ngx::casper::broker::Module::k_redis_ip_address_key_lc_,
               ::ngx::casper::broker::Module::k_redis_port_number_key_lc_,
               ::ngx::casper::broker::Module::k_redis_database_key_lc_,
               ::ngx::casper::broker::Module::k_redis_max_conn_per_worker_lc_
    );

    SetupCURL(a_config,
              ::ngx::casper::broker::Module::k_curl_max_conn_per_worker_lc_
    );
    
    SetupBeanstalkd(a_config,
                    ::ngx::casper::broker::Module::k_beanstalkd_host_key_lc_,
                    ::ngx::casper::broker::Module::k_beanstalkd_port_key_lc_,
                    ::ngx::casper::broker::Module::k_beanstalkd_timeout_key_lc_,
                    ::ngx::casper::broker::Module::k_beanstalkd_sessionless_tubes_key_lc_,
                    ::ngx::casper::broker::Module::k_beanstalkd_action_tubes_key_lc_,
                    s_beanstalkd_config_
    );

    // ... pick worker sockets ...
    PreWorkerStartup(scheduler_socket_fn_, shared_handler_socket_fn_);
    
    ::ev::scheduler::UniqueIDGenerator::GetInstance().Startup();

    // ... now initialize 'ev::SharedHandler' ...
    ::ev::ngx::Bridge::GetInstance().Startup(shared_handler_socket_fn_,
                                             std::bind(&ngx::casper::ev::Glue::OnFatalException, this, std::placeholders::_1)
    );
    
    // ... then initialize scheduler ...
    ::ev::scheduler::Scheduler::GetInstance().Start(NGX_ABBR,
                                                    scheduler_socket_fn_,
                                                    ::ev::ngx::Bridge::GetInstance(),
                                                    [this]() {
                                                        scheduler_cv_.Wake();
                                                    },
                                                    [this] (const ::ev::Object* a_object) -> ::ev::Device* {
                                                        
                                                        switch (a_object->target_) {
                                                            case ::ev::Object::Target::Redis:
                                                                return new ::ev::redis::Device(/* a_loggable_data */
                                                                                               *ngx::casper::ev::Glue::s_loggable_data_ptr_,
                                                                                               /* a_client_name */
                                                                                               NGX_NAME,
                                                                                               /* a_ip_address */
                                                                                               config_map_[::ngx::casper::broker::Module::k_redis_ip_address_key_lc_].c_str(),
                                                                                               /* a_port */
                                                                                               std::stoi(config_map_[::ngx::casper::broker::Module::k_redis_port_number_key_lc_]),
                                                                                               /* a_database_index */
                                                                                               std::stoi(config_map_[::ngx::casper::broker::Module::k_redis_database_key_lc_])
                                                                );
                                                            case ::ev::Object::Target::PostgreSQL:
                                                                return new ::ev::postgresql::Device(/* a_loggable_data */
                                                                                                    *ngx::casper::ev::Glue::s_loggable_data_ptr_,
                                                                                                    /* a_conn_str */
                                                                                                    config_map_[::ngx::casper::broker::Module::k_postgresql_conn_str_key_lc_].c_str(),
                                                                                                    /* a_statement_timeout */
                                                                                                    std::stoi(config_map_[::ngx::casper::broker::Module::k_postgresql_statement_timeout_lc_]),
                                                                                                    /* a_postgresql_post_connect_queries */
                                                                                                    postgresql_post_connect_queries_,
                                                                                                    /* a_max_queries_per_conn */
                                                                                                    device_limits_[::ev::Object::Target::PostgreSQL].rnd_queries_per_conn_()
                                                                );
                                                            case ::ev::Object::Target::CURL:
                                                                return new ::ev::curl::Device(/* a_loggable_data */
                                                                                              *ngx::casper::ev::Glue::s_loggable_data_ptr_
                                                                );
                                                            default:
                                                                return nullptr;
                                                        }
                                             
                                                    },
                                                    [this](::ev::Object::Target a_target) -> size_t {
                                                        
                                                        const auto it = device_limits_.find(a_target);
                                                        if ( device_limits_.end() != it ) {
                                                            return it->second.max_conn_per_worker_;
                                                        }
                                                        return 2;
                                                        
                                                    }
    );
    
    // ... wait until scheduler is ready ...
    scheduler_cv_.Wait();
    
    // ... finally, initialize 'redis' subscriptions ...
    ::ev::redis::subscriptions::Manager::GetInstance().Startup(/* a_loggable_data */
                                                               *ngx::casper::ev::Glue::s_loggable_data_ptr_,
                                                               /* a_bridge */
                                                               &::ev::ngx::Bridge::GetInstance(),
                                                               /* a_channels */
                                                               {},
                                                               /* a_patterns */
                                                               {},
                                                               /* a_timeout_config */
                                                               {
                                                                    /* callback_ */
                                                                   nullptr,
                                                                   /* sigabort_file_uri_ */
                                                                   "" /* "/tmp/cores/" NGX_NAME "-sigabort_on_redis_subscribe_timeout" */
                                                               }
    );
    
    // ... signals ...
    ::ev::Signals::GetInstance().Append({
        {
            /* signal_      */ SIGTTIN,
            /* description_ */ "Gatekeeper config reload",
            /* callback_    */ [] () -> std::string {
                if ( true == ::ev::auth::route::Gatekeeper::GetInstance().Reload(SIGTTIN) ) {
                    return "Gatekeeper config reloaded";
                } else {
                    return "Gatekeeper config reload skipped - is not configured";
                }
            }
        }
    });
    
    //
    o_callbacks.on_fatal_exception_  = std::bind(&ngx::casper::ev::Glue::OnFatalException, this, std::placeholders::_1);
    o_callbacks.call_on_main_thread_ = [] (std::function<void()> a_callback) {
        ::ev::ngx::Bridge::GetInstance().CallOnMainThread(a_callback);
    };

    // ... done ...
    ngx::casper::ev::Glue::s_initialized_ = true;
    
    OSALITE_DEBUG_TRACE("ev_glue", "<~ Startup(...)");
}

/**
 * @brief Call this to dealloc previously allocated memory.
 *
 * @param a_sig_no
 */
void ngx::casper::ev::Glue::Shutdown (int a_sig_no)
{
    OSALITE_DEBUG_TRACE("ev_glue", "~> Shutdown()");
    // ... first shutdown 'redis' subscriptions ...
    ::ev::redis::subscriptions::Manager::GetInstance().Shutdown();
    // ... then shutdown 'scheduler' ...
    ::ev::scheduler::Scheduler::GetInstance().Stop([]() {
#if 0
        scheduler_cv_.Wake();
#endif
    }, a_sig_no);
    // ... wait until scheduler is ready ...
#if 0
    scheduler_cv_.Wait();
#endif
    // ... finally, shutdown 'ev::SharedHandler' ...
    ::ev::ngx::Bridge::GetInstance().Shutdown();
    // ... forget configs ...
    config_map_.clear();
    
    ::ev::scheduler::UniqueIDGenerator::GetInstance().Shutdown();
    
    ::ev::Signals::GetInstance().Unregister();
    
    ngx::casper::ev::Glue::s_loggable_data_ptr_ = nullptr;
    ngx::casper::ev::Glue::s_initialized_       = false;
    OSALITE_DEBUG_TRACE("ev_glue", "<~ Shutdown()");
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Call to report a fatal exception and exit with error.
 *
 * @param a_ev_exception
 */
void ngx::casper::ev::Glue::OnFatalException (const ::ev::Exception& a_ev_exception)
{
    for ( auto output : { stdout, stderr } ) {
        fprintf(output, "\n@@@@@@@@@@@@\nFATAL ERROR:\n@@@@@@@@@@@@\n%s\n", a_ev_exception.what());
        fflush(output);
    }
    exit(EXIT_FAILURE);
}

