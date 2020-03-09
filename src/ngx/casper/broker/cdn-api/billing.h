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
#ifndef NRS_NGX_CASPER_BROKER_CDN_API_BILLING_H_
#define NRS_NGX_CASPER_BROKER_CDN_API_BILLING_H_

#include "ngx/casper/broker/cdn-api/route.h"

#include "ngx/casper/broker/cdn-common/db/billing.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                namespace api
                {
                    
                    class Billing final : public Route
                    {

                    public: // Data Type(s)
                        
                        typedef common::db::Billing::JSONCallbacks Callbacks;
                        
                    private: // Data
                        
                        common::db::Billing db_;
                        Callbacks           owner_callbacks_;
                        Callbacks           wrapped_callbacks_;
                        Json::Value         tmp_value_;

                    public: // Constructor(s) / Destructor
                        
                        Billing () = delete;
                        Billing (const ::ev::Loggable::Data& a_loggable_data_ref, Callbacks a_callbacks);
                        virtual ~Billing();
                        
                    public: // Method(s) / Function(s)
                        
                        virtual ngx_int_t Process (const uint64_t& a_id,
                                                   const ngx_uint_t& a_method, const std::map<std::string, std::string>& a_headers,
                                                   const std::map<std::string, std::set<std::string>>& a_params,
                                                   const char* const a_body);
                        
                    private: // Method(s) / Function(s)
                        
                        const Json::Value& TranslateTableToJSONAPI (const ngx_uint_t& a_method, const uint64_t& a_id, const std::map<std::string, std::set<std::string>>& a_params,
                                                                    const Json::Value& a_value);
                        
                    }; // end of class 'Billing'
                    
                } // end of namespace 'api'
                
            } // end of namespace 'api'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_API_BILLING_H_
