/**
 * @file billing.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_BILLING_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_BILLING_H_

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
                        
                        class Billing final : public ::cc::NonCopyable, public ::cc::NonMovable, public Object
                        {
                            
                        private: // Data
                            
                            bool include_details_;
                            
                        public: // Constructor(s) / Destructor
                            
                            Billing () = delete;
                            Billing (const ::ev::Loggable::Data& a_loggable_data_ref,
                                     const Settings& a_settings,
                                     const std::string& a_schema = "fs", const std::string& a_table = "billing") = delete;
                            Billing (const ::ev::Loggable::Data& a_loggable_data_ref,
                                     const std::string& a_schema = "fs", const std::string& a_table = "billing");
                            virtual ~Billing ();

                        public: // Pre Execute Config Method(s) / Function(s)
                                
                            void SetIncludeDetails (bool a_include);

                        public: // Method(s) / Function(s)

                            void Create (const uint64_t& a_id, const Json::Value& a_payload, JSONCallbacks a_callbacks);
                            void Update (const uint64_t& a_id, const Json::Value& a_payload, JSONCallbacks a_callbacks);
                            void Get    (const uint64_t& a_id, JSONCallbacks a_callbacks);
                            
                            void Get (const uint64_t& a_id, const std::string& a_type, JSONCallbacks a_callbacks);
                                                                                    
                        }; // end of class 'Billing'
                    
                        /**
                         * @brief Set if detail information should be included.
                         *
                         * @param a_include True if so, false otherwise.
                         */
                        inline void Billing::SetIncludeDetails (bool a_include)
                        {
                            include_details_ = a_include;
                        }
                    
                    } // end of namespace 'db'
                    
                } // end of namespace 'common'
            
            } // end of namespace 'cdn'
        
        } // end of namespace 'broker'
    
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_BILLING_H_
