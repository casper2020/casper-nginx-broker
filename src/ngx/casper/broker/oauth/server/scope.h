/**
 * @file scope.h
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
#ifndef NRS_NGX_CASPER_BROKER_OAUTH_SERVER_SCOPE_H_
#define NRS_NGX_CASPER_BROKER_OAUTH_SERVER_SCOPE_H_

#include "ev/redis/value.h"

#include <string>
#include <vector>

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
                    
                    class Scope
                    {
                        
                    public: // Const Data
                        
                        std::vector<std::string> allowed_;
                        
                    public: // Constructor(s) / Destructor
                        
                        Scope (const ev::redis::Value& a_allowed);
                        virtual ~Scope();
                        
                    public: // Method(s) / Function(s)
                        
                        std::string Grant (const std::string& a_scope);
                        
                    public: // Const Method(s) / Function(s)
                        
                        const std::vector<std::string>& Allowed () const;
                        const std::string               Default () const;
                        
                    private: // Method(s) / Function(s)
                        
                        void Parse (const std::string& a_scope, std::vector<std::string>& o_scope);
                        
                    }; // end of class 'Scope'
                    
                    /**
                     * @return The allowed list of scopes.
                     */
                    inline const std::vector<std::string>& Scope::Allowed () const
                    {
                        return allowed_;
                    }

                    /**
                     * @return The 'default' scope, empty string if none set.
                     */
                    inline const std::string Scope::Default () const
                    {
                        return allowed_.size() > 0 ? allowed_[0] : "";
                    }

                } // end of namespace 'server'
                
            } // end of namespace 'oauth'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_OAUTH_SERVER_SCOPE_H_

