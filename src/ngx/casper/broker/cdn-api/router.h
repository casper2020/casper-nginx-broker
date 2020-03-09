/**
 * @file router.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_API_ROUTER_H_
#define NRS_NGX_CASPER_BROKER_CDN_API_ROUTER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ngx/casper/broker/cdn-api/route.h"

#include "core/ngx_config.h" // ngx_uint_t

#include <functional>

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
                    
                    class Router final : public ::cc::NonCopyable, public ::cc::NonMovable
                    {
                        
                    public: // Data Type(s)
                        
                        typedef std::function<Route*(const char* const, const ::ev::Loggable::Data&)> Factory;
                        
                    private: // Data Type(s)
                        
                        std::map<std::string, Route*> routes_;
                        
                    private: // Const Refs
                        
                        const ::ev::Loggable::Data& loggable_data_ref_;
                        
                    private: // Data
                        
                        Factory   factory_;

                    public: // Constructor(s) / Destructor
                        
                        Router () = delete;
                        Router (const ::ev::Loggable::Data& a_loggable_data_ref);
                        virtual ~Router();
                        
                    public: // Method(s) / Function(s)
                        
                        void Enable  (Factory a_factory);
                        void Process (const ngx_uint_t a_method, const std::string& a_uri, const char* const a_body, const std::map<std::string, std::string>& a_headers,
                                      std::string& o_type, std::string& o_id, std::map<std::string, std::set<std::string>>& o_params, bool& a_asynchronous);
                                                
                    private: // Inline Method(s) / Function(s)
                        
                        inline Route&  route  (const char* const a_name);
                        
                    }; // end of class 'Router'
                
                    /**
                     * @brief Enabble route by setting a factory.
                     */
                    inline void Router::Enable (Router::Factory a_factory)
                    {
                        factory_ = a_factory;
                    }
                
                    /**
                     * @brief Create a \link Route \link object to process the current api.
                     */
                    inline Route& Router::route (const char* const a_name)
                    {
                        const auto it = routes_.find(a_name);
                        if ( it != routes_.end() ) {
                            return *it->second;
                        }
                        routes_[a_name] = factory_(a_name, loggable_data_ref_);                        
                        return *routes_[a_name];
                    }

                } // end of namespace 'api'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_API_ROUTER_H_
