/**
 * @file abstract_token.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ABSTRACT_TOKEN_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ABSTRACT_TOKEN_H_

#include "ngx/casper/broker/oauth/server/object.h"

#include "json/json.h"

#include <string>
#include <map>

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
                    
                    class AbstractToken : public Object
                    {
                        
                    public: // Data Type(s)

                        typedef std::function<void()> PreloadCallback;

                        typedef std::function<void(const std::string& a_uri)>                                                                                     RedirectCallback;
                        typedef std::function<void(const uint16_t a_status_code, const std::map<std::string, std::string>& a_headers, const Json::Value& a_body)> JSONCallback;
                        typedef std::function<void(const std::string&)>                                                                                           LogCallback;
                        
                    protected: // Static Const Data
                        
                        static const std::string k_access_token_ttl_field_;
                        static const std::string k_refresh_token_ttl_field_;
                        
                    protected: // Const Data
                        
                        const std::string client_secret_;
                        const std::string token_type_;
                        
                    protected: // Data
                        
                        std::map<std::string, std::string> headers_;
                        Json::Value                        body_;
                        std::vector<std::string>           args_;
                        
                    public: // Constructor(s) / Destructor
                        
                        AbstractToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                       const std::string& a_service_id,
                                       const std::string& a_client_id, const std::string& a_client_secret,
                                       const std::string& a_redirect_uri,
                                       bool a_bypass_rfc);
                        virtual ~AbstractToken ();
                        
                    public: // Pure Virtual Method(s) / Function(s)
                        
                        virtual void AsyncRun (PreloadCallback a_preload_callback, RedirectCallback a_redirect_callback, JSONCallback a_json_callback, LogCallback a_log_callback) = 0;
                        
                    protected: // Method(s) / Function(s)
                        
                        void SetAndCallStandardJSONErrorResponse   (const std::string& a_error, const std::string a_error_description,
                                                                    JSONCallback a_callback, LogCallback a_log_callback,
                                                                    const ::cc::Exception& a_cc_exception);
                        void GenNewToken                           (const char* const a_name, const char* const a_error_code, std::string& o_value);
                        
                    }; // end of class 'AbstractToken'
                    
                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_ABSTRACT_TOKEN_H_
