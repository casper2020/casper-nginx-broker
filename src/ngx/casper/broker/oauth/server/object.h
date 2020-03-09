/**
 * @file object.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_OBJECT_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_OBJECT_H_

#include <string>

#include "ev/loggable.h"

#include "ev/scheduler/scheduler.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace oauth
            {
                
                namespace server
                {
                    
                    class Object : public ::ev::scheduler::Client
                    {
                        
                    public: // DataTypes
                        
                        typedef std::function<void(const std::string& a_error)> FailureCallback;
                        
                    protected: // Static Const Data
                        
                        static const std::string k_client_secret_field_;
                        static const std::string k_client_redirect_uri_field_;
                        static const std::string k_client_scope_field_;

                    protected: // Const Data
                        
                        const ::ev::Loggable::Data& loggable_data_ref_;
                        const std::string           service_id_;
                        const std::string           client_id_;
                        const std::string           redirect_uri_;
                        const bool                  bypass_rfc_;
                        
                        const std::string client_key_;
                        const std::string authorization_code_hash_prefix_;
                        const std::string access_token_hash_prefix_;
                        const std::string refresh_token_hash_prefix_;
                        
                    protected: // Data
                        
                        int64_t ttl_;

                    public: // Constructor (s) / Destructor
                        
                        Object (const ::ev::Loggable::Data& a_loggable_data_ref,
                                const std::string& a_service_id,
                                const std::string& a_client_id, const std::string& a_redirect_uri,
                                bool a_bypass_rfc);
                        virtual ~Object ();
                        
                    protected: // Method(s) / Function(s)
                        
                        std::string            RandomString (const size_t a_length);
                        ::ev::scheduler::Task* NewTask      (const EV_TASK_PARAMS& a_callback);
                        
                    }; // end of class 'Object'
                    
                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_OBJECT_H_
