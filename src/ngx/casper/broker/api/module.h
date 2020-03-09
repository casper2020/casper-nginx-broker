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
#ifndef NRS_NGX_CASPER_BROKER_API_MODULE_H_
#define NRS_NGX_CASPER_BROKER_API_MODULE_H_

#include "ngx/casper/broker/module.h"

#include "ngx/casper/broker/api/module/ngx_http_casper_broker_api_module.h"

#include "ngx/casper/broker/ext/session.h"
#include "ngx/casper/broker/ext/job.h"

#include "ev/postgresql/json_api.h"

#include "ev/auth/route/gatekeeper.h"

#include "json/json.h"

#include <map>    // std::map
#include <string> // std::string

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace api
            {

                class Module final : public ::ngx::casper::broker::Module
                {
                    
                public: // Static Const Data
                    
                    static const char* const k_json_api_content_type_;
                    static const char* const k_json_api_content_type_w_charset_;

                private: // Helpers

                    ::ev::postgresql::JSONAPI json_api_;
                    Json::Reader              json_reader_;
                    Json::FastWriter          json_writer_;
                    
                private: // Extensions
                    
                    ext::Session              session_;
                    ext::Job                  job_;
                    
                private: // Data
                    
                    std::string               uri_;
                    std::string               access_token_;
                    
                protected: // Constructor(s)
                    
                    Module (const broker::Module::Config& a_config, const broker::Module::Params& a_params,
                            ngx_http_casper_broker_module_loc_conf_t& a_ngx_broker_loc_conf, ngx_http_casper_broker_api_module_loc_conf_t& a_ngx_loc_conf);
                    
                public: // Constructor(s) / Destructor
                    
                    virtual ~Module();
                    
                public: // Inherited Method(s) / Function(s)
                    
                    virtual ngx_int_t Setup ();
                    virtual ngx_int_t Run   ();
                    
                private: // Method(s) / Function(s)
                    
                    void OnSessionFetchSucceeded    (const bool a_async_request, const ::ev::casper::Session& a_session);
                    void OnJSONAPIReply             (const char* a_uri, const char* a_json, const char* a_error, uint16_t a_status);
                    
                    ::ev::auth::route::Gatekeeper::DeflectorResult
                        OnOnGatekeeperDeflectToJob (const std::string& a_tube, ssize_t a_ttr, ssize_t a_valitity);
                    
                private: // Method(s) / Function(s)
                    
                    ngx_int_t PostJob (const ngx_int_t a_method,
                                       const std::string& a_urn, const std::string& a_body,
                                       const std::string& a_tube, const ssize_t a_ttr, const ssize_t a_validity,
                                       const std::string& a_access_token);
                    
                public: // Static Method(s) / Function(s)
                    
                    static ngx_int_t Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler);

                private: // Static Method(s) / Function(s)
                    
                    static void ReadBodyHandler (ngx_http_request_t* a_request);
                    static void CleanupHandler  (void*);

                }; // end of class 'Module'

            } // end of namespace 'api'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_API_MODULE_H_
