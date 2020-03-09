/**
 * @file refresh_token.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_REFRESH_TOKEN_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_REFRESH_TOKEN_H_

#include "ngx/casper/broker/oauth/server/abstract_token.h"

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
                    
                    class RefreshToken final : public AbstractToken
                    {
                        
                    private: // Const Data
                        
                        const std::string refresh_token_;
                        const std::string scope_;
                        
                    private: // Data
                        
                        std::string granted_scope_;
                        bool        generate_new_refresh_token_;
                        std::string new_refresh_token_;
                        int64_t     new_refresh_token_ttl_;

                    public: // Constructor(s) / Destructor
                        
                        RefreshToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                      const std::string& a_service_id,
                                      const std::string& a_client_id, const std::string& a_client_secret,
                                      const std::string& a_refresh_token, const std::string& a_scope,
                                      bool a_bypass_rfc);
                        virtual ~RefreshToken();
                        
                    public: // Inherited Pure Virtual Method(s) / Function(s)
                        
                        virtual void AsyncRun (PreloadCallback a_preload_callback, RedirectCallback a_redirect_callback, JSONCallback a_json_callback);
                        
                    }; // end of class 'RefreshToken'
                    
                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_REFRESH_TOKEN_H_
