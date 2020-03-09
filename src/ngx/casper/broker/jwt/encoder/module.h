/**
 * @file module.h
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
#ifndef NRS_NGX_CASPER_BROKER_JWT_ENCODER_MODULE_H_
#define NRS_NGX_CASPER_BROKER_JWT_ENCODER_MODULE_H_

#include "ngx/casper/broker/module.h"

#include "ngx/casper/broker/jwt/encoder/module/ngx_http_casper_broker_jwt_encoder_module.h"

#include "json/json.h"

#include <map>    // std::map
#include <string> // std::string

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace jwt
            {
                
                namespace encoder
                {
                    
                    class Module final : public ::ngx::casper::broker::Module
                    {
                        
                    public: // Static Const Data
                        
                        static const char* const k_json_jwt_encoder_content_type_;
                        static const char* const k_json_jwt_encoder_content_type_w_charset_;
                        
                    private: // Data
                        
                        std::string jwt_iss_;
                        std::string jwt_public_rsa_key_;
                        std::string jwt_private_rsa_key_;
                        uint64_t    jwt_duration_;
                        
                    protected: // Constructor(s)
                        
                        Module (const broker::Module::Config& a_config, const broker::Module::Params& a_params,
                                ngx_http_casper_broker_jwt_encoder_module_loc_conf_t& a_ngx_loc_conf);
                        
                    public: // Constructor(s) / Destructor
                        
                        virtual ~Module();
                        
                    public: // Inherited Method(s) / Function(s)
                        
                        virtual ngx_int_t Run ();
                        
                    public: // Static Method(s) / Function(s)
                        
                        static ngx_int_t Factory (ngx_http_request_t* a_r);
                        
                    private: // Static Method(s) / Function(s)
                        
                        static void ReadBodyHandler (ngx_http_request_t* a_request);
                        static void CleanupHandler  (void*);
                        
                    }; // end of class 'Module'
                    
                } // end of namespace 'encoder'
                
            } // end of namespace 'jwt'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_JWT_ENCODER_MODULE_H_
