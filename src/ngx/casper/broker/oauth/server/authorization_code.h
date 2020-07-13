/**
 * @file authorization_code.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_AUTHORIZATION_CODE_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_AUTHORIZATION_CODE_H_

#include "ngx/casper/broker/oauth/server/object.h"

#include "json/json.h"

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
                    
                    class AuthorizationCode final : public Object
                    {
                        
                    public: // Data Type(s)
                        
                        typedef std::function<void()>                                                                                                             PreloadCallback;
                        typedef std::function<void(const std::string& a_uri)>                                                                                     RedirectCallback;
                        typedef std::function<void(const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body)> JSONCallback;
                        typedef std::function<void(const std::string&)>                                                                                           LogCallback;
                                                
                    private: // Static Const Data
                        
                        static const std::string k_authorization_code_ttl_field_;

                    private: // Const Data
                        
                        const std::string scope_;
                        const std::string state_;
                        
                    private: // Data
                        
                        std::map<std::string, std::string> headers_;
                        Json::Value                        body_;
                        std::string                        granted_scope_;
                        std::vector<std::string>           args_;
                        
                    public: // Constructor (s) / Destructor
                        
                        AuthorizationCode (const ::ev::Loggable::Data& a_loggable_data_ref,
                                           const std::string& a_service_id,
                                           const std::string& a_client_id, const std::string& a_scope, const std::string& a_state,
                                           const std::string& a_redirect_uri,
                                           bool a_bypass_rfc);
                        virtual ~AuthorizationCode ();
                        
                    public: // Method(s) / Function(s)
                        
                        void AsyncRun (PreloadCallback a_preload_callback, RedirectCallback a_redirect_callback, JSONCallback a_json_callback, LogCallback a_log_callback);
                        
                    }; // end of class 'AuthorizationCode'
                    
                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_AUTHORIZATION_CODE_H_
