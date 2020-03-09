/**
 * @file sideline.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_DB_COMMON_SIDELINE_H_
#define NRS_NGX_CASPER_BROKER_CDN_DB_COMMON_SIDELINE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ngx/casper/broker/cdn-common/db/object.h"

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
                        
                        class Sideline final : public ::cc::NonCopyable, public ::cc::NonMovable, public common::db::Object
                        {
                            
                        public: // Enum(s)
                            
                            enum class Activity : uint8_t {
                                NotSet = 0x00,
                                Copy
                            };
                            
                            enum class Status : uint8_t {
                                NotSet = 0x00,
                                Pending,
                                Scheduled,
                                InProgress,
                                Done
                            };
                            
                            friend std::ostream& operator << (std::ostream& o_stream, const Activity& a_activity);
                            friend std::ostream& operator << (std::ostream& o_stream, const Status& a_status);
                          
                        public: // Data Type(s)
                                                        
                            typedef struct {
                                uint64_t    id_;
                                std::string type_;
                            } Billing;
                                                        
                        public: // Constructor(s) / Destructor
                            
                            Sideline () = delete;
                            Sideline (const ::ev::Loggable::Data& a_loggable_data_ref,
                                      const std::string& a_schema = "fs", const std::string& a_table = "sideline") = delete;
                            Sideline (const ::ev::Loggable::Data& a_loggable_data_ref,
                                      const Settings& a_settings,
                                      const std::string& a_schema = "fs", const std::string& a_table = "sideline");
                            virtual ~Sideline ();

                        public: // Method(s) / Function(s)

                            void Register (const Activity& a_activity, const Billing& a_billing, const Json::Value& a_payload,
                                           JSONCallbacks a_callbacks);
                                                        
                        private: // Method(s) / Function(s)
                            
                            
                        }; // end of class 'Sideline'
                    
                       /**
                        * @brief Append a \link Sideline::Activity \link value ( string representation ) to a stream.
                        *
                        * @param a_activity One of \link Sideline::Activity \link.
                        */
                       inline std::ostream& operator << (std::ostream& o_stream, const Sideline::Activity& a_activity)
                       {
                           switch (a_activity) {
                               case Sideline::Activity::Copy : o_stream << "Copy"; break;
                               default                       : o_stream.setstate(std::ios_base::failbit); break;
                           }
                           return o_stream;
                       }
                       
                       /**
                        * @brief Append a \link Sideline::Status \link value ( string representation ) to a stream.
                        *
                        * @param a_operation One of \link Sideline::Status \link.
                        */
                       inline std::ostream& operator << (std::ostream& o_stream, const Sideline::Status& a_status)
                       {
                           switch (a_status) {
                               case Sideline::Status::Pending    : o_stream << "Pending"    ; break;
                               case Sideline::Status::Scheduled  : o_stream << "Scheduled"  ; break;
                               case Sideline::Status::InProgress : o_stream << "InProgress" ; break;
                               case Sideline::Status::Done       : o_stream << "Done"       ; break;
                               default                           : o_stream.setstate(std::ios_base::failbit); break;
                           }
                           return o_stream;
                       }
                    
                    } // end of namespace 'db'
                    
                } // end of namespace 'common'
            
            } // end of namespace 'cdn'
        
        } // end of namespace 'broker'
    
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_DB_COMMON_SIDELINE_H_
