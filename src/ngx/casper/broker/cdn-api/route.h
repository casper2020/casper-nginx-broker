/**
 * @file route.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_API_ROUTE_H_
#define NRS_NGX_CASPER_BROKER_CDN_API_ROUTE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/loggable.h"

#include "json/json.h"

#include "ev/exception.h"

#include "core/ngx_config.h" // ngx_int_t, ngx_uint_t

#include <set>
#include <map>
#include <string>
#include <algorithm>

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
                    
                    class Route : public ::cc::NonCopyable, public ::cc::NonMovable
                    {
                        
                    protected: // Const Refs
                            
                        const ::ev::Loggable::Data& loggable_data_ref_;
       
                    public: // Constructor(s) / Destructor
                        
                        Route () = delete;                        
                        Route (const ::ev::Loggable::Data& a_loggable_data_ref);
                        virtual ~Route ();
                        
                    public: // Pure Virtual Method(s) / Function(s)
                        
                        virtual ngx_int_t Process (const uint64_t& a_id,
                                                   const ngx_uint_t& a_method, const std::map<std::string, std::string>& a_headers,
                                                   const std::map<std::string, std::set<std::string>>& a_params,
                                                   const char* const a_body) = 0;
                        
                    }; // end of class 'Route'

                } // end of namespace 'api'
            
            } // end of namespace 'cdn'
        
        } // end of namespace 'broker'
    
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_API_ROITE_H_
