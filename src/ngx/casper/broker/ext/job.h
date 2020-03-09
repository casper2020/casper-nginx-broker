/**
 * @file job.h
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
#ifndef NRS_NGX_CASPER_BROKER_EXT_JOB_H_
#define NRS_NGX_CASPER_BROKER_EXT_JOB_H_

#include "ngx/casper/broker/ext/base.h"

#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include "json/json.h"

#include <map>    // std::map
#include <string> // std::string

#include "ev/loggable.h"
#include "ev/beanstalk/config.h"
#include "ev/redis/subscriptions/manager.h"

#include "ev/scheduler/scheduler.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace ext
            {
                
                class Job : public ext::Base, public ::ev::redis::subscriptions::Manager::Client, public ::ev::scheduler::Client
                {
                    
                protected: // Data Type(s)
                    
                    struct PrimitiveProtocolEntry {
                        const char* start_;
                        const char* end_;
                        unsigned    unsigned_value_;
                    };
                    
                protected: // data
                    
                    Json::Value             jwt_allowed_iss_;
                    std::string             jwt_rsa_public_key_uri_;
                    std::string             job_tube_;
                    std::string             job_id_key_;
                    std::string             job_key_;
                    std::string             job_channel_;
                    Json::Value             job_object_;
                    Json::Value             job_patch_object_;
                    std::set<std::string>   job_allowed_patch_members_set_;
                    Json::Value             job_status_;
                    size_t                  job_ttr_;
                    size_t                  job_expires_in_;
                    Json::Reader            json_reader_;
                    Json::FastWriter        json_writer_;
                    ::ev::beanstalk::Config beanstalk_config_;
                    
                    PrimitiveProtocolEntry  primitive_protocol_ [3];
                    
                public: // Constructor(s)

                    Job (broker::Module::CTX& a_ctx, Module* a_module,
                         const ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf);

                    Job (broker::Module::CTX& a_ctx, Module* a_module,
                         const broker::Module::Params& a_params, const ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf);
                    virtual ~Job();

                    
                public: // Method(s) / Function(s)

                    ngx_int_t Submit (const Json::Value& a_object);
                    ngx_int_t Submit (const std::string& a_jwt);

                    ngx_int_t Submit (const Json::Value& a_object,
                                      const std::function<void()> a_success_callback,
                                      const std::function<void(const ::ev::Exception&)> a_failure_callback);

                protected: // Method(s) / Function(s)
                    
                    ngx_int_t HandleRedirect (const Json::Value& a_object);
                    
                private:  // Method(s) / Function(s)
                    
                    ngx_int_t                                        ScheduleJob             (const std::string& a_name);
                    EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK JobSubscriptionCallback (const std::string& a_name, const ::ev::redis::subscriptions::Manager::Status& a_status);
                    EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK JobMessageCallback      (const std::string& a_name, const std::string & a_message);
                    
                protected: // from ::ev::redis::SubscriptionsManager::Client
                    
                    virtual void OnREDISConnectionLost ();
                    
                public: // Inline Method(s) / Function(s)
                    
                    size_t             ttr                    () const;
                    size_t             expires_in             () const;
                    const std::string& jwt_rsa_public_key_uri () const;
                    
                }; // end of class 'Job'
                
                /**
                 * @return TTR.
                 */
                inline size_t Job::ttr () const
                {
                    return job_ttr_;
                }
                
                /**
                 * @return Expires in.
                 */
                inline size_t Job::expires_in () const
                {
                    return job_expires_in_;
                }
            
                /**
                 * @return path to JWT RSA public key
                 */
                inline const std::string& Job::jwt_rsa_public_key_uri () const
                {
                    return jwt_rsa_public_key_uri_;
                }
            
            } // end of namespace 'ext'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_EXT_JOB_H_
