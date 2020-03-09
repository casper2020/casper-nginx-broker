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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_DB_OBJECT_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_DB_OBJECT_H_

#include "ev/scheduler/task.h"
#include "ev/scheduler/scheduler.h"

#include "ev/object.h"
#include "ev/postgresql/value.h"
#include "ev/postgresql/error.h"

#include <algorithm> // std::function

#include "ev/loggable.h" // ::ev::Loggable::Data

#include "json/json.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                namespace common
                {
                    
                    namespace db
                    {
                        
                        class Object : protected ::ev::scheduler::Client
                        {
                                                        
                        public: // Data Type(s)
                            
                            typedef struct {
                                std::function<void(const uint16_t, const ::ev::postgresql::Value&)> success_;
                                std::function<void(const uint16_t, const ::ev::postgresql::Error&)> error_;
                                std::function<void(const uint16_t, const ::ev::Exception&)>         failure_;
                            } Callbacks;
                            
                            typedef struct {
                                std::function<void(const uint16_t, const Json::Value&)>     success_;
                                std::function<void(const uint16_t, const ::ev::Exception&)> failure_;
                            } JSONCallbacks;
                            
                            typedef struct {
                                std::string   tube_;
                                uint64_t      ttr_;
                                uint64_t      validity_;
                                Json::Value   slaves_;
                            } Settings;

                        protected: // Const Refs
                                
                            const ::ev::Loggable::Data& loggable_data_ref_;

                        protected: // Const Ptrs
                                
                            const Settings* settings_;

                        protected: // Const Data
                            
                            const std::string schema_;
                            const std::string table_;

                        public: // Constructor(s) / Destructor
                            
                            Object () = delete;
                            Object (const ::ev::Loggable::Data& a_loggable_data_ref,
                                    const std::string& a_schema, const std::string& a_table);
                            Object (const ::ev::Loggable::Data& a_loggable_data_ref, const Settings* a_settings,
                                    const std::string& a_schema, const std::string& a_table);
                            virtual ~Object();
                            
                        protected: // Method(s) / Function(s)
                            
                            void AsyncQuery (const std::stringstream& a_ss, Callbacks a_callbacks);
                            void AsyncQueryAndSerializeToJSON (const std::stringstream& a_ss, JSONCallbacks a_callbacks);

                        protected: // Method(s) / Function(s) - ::ev::scheduler::Client
                            
                            ::ev::scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
                            
                        }; // end of class 'Object'
                        
                    } // end of namespace 'db'
                
                } // end of namespace 'common'
            
            } // end of namespace 'cdn'
        
        } // end of namespace 'broker'
    
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_DB_OBJECT_H_

