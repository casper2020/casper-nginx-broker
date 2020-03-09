/**
 * @file access_token.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ACCESS_TOKEN_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ACCESS_TOKEN_H_

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
                    
                    class AccessToken final : public AbstractToken
                    {

                    private: // Const Data
                        
                        const std::string code_;
                        const std::string state_;
                        
                    private: // Data
                        
                        int64_t     refresh_token_ttl_;
                        std::string scope_;
                        
                    public: // Constructor(s) / Destructor
                        
                        AccessToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                     const std::string& a_service_id,
                                     const std::string& a_client_id, const std::string& a_client_secret, const std::string& a_redirect_uri,
                                     const std::string& a_code, const std::string& a_state,
                                     bool a_bypass_rfc);
                        virtual ~AccessToken();
                        
                    public: // Inherited Pure Virtual Method(s) / Function(s)
                        
                        virtual void AsyncRun (PreloadCallback a_preload_callback, RedirectCallback a_redirect_callback, JSONCallback a_json_callback);
                        
                    }; // end of class 'AccessToken'
                    
                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ACCESS_TOKEN_H_
