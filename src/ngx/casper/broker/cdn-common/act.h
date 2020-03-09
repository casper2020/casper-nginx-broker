/**
 * @file act.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_ACT_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_ACT_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ngx/casper/broker/cdn-common/types.h" // H2EMap

#include <stdint.h> // uint8_t

#include "json/json.h"

#include "ngx/casper/broker/cdn-common/ast/tree.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                class ACT : public ::cc::NonCopyable, public ::cc::NonMovable
                {
           
                public: // Const Refs
                    
                    const Json::Value&                        config_;
                    const std::map<std::string, std::string>& headers_map_;
                    
                private: // Helpers
                    
                    common::ast::Tree                         tree_;
                    
                private: // Data
                    
                    std::map<std::string, Variable<uint64_t, XNumber>*> numeric_variables_;

                public: // Constructor(s) / Destructor
                    
                    ACT () = delete;
                    ACT (const Json::Value& a_config, const std::map<std::string, std::string>& a_headers_map);
                    virtual ~ACT ();
                    
                public: // Method(s) / Function(s)
                    
                    void     Setup    ();
                    void     Compile  (const std::string& a_value,
                                       const std::function<void(const std::string& ,const std::string&)>& a_callback);
                    uint64_t Evaluate (const std::string& a_expression) const;
                    
                }; // end of class 'ACT'
                
            } // end of namespace 'api'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_ACT_H_
