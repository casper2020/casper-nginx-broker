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
#ifndef NRS_NGX_CASPER_BROKER_CDN_API_MODULE_H_
#define NRS_NGX_CASPER_BROKER_CDN_API_MODULE_H_

#include "ngx/casper/broker/module.h"

#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include "ngx/casper/broker/cdn-api/module/ngx_http_casper_broker_cdn_api_module.h"

#include "ngx/casper/broker/cdn-api/router.h"

#include "ngx/casper/broker/cdn-api/billing.h"
#include "ngx/casper/broker/cdn-api/sideline.h"

#include "ngx/casper/broker/ext/job.h"

#include "json/json.h"

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

                    class Module final : public ::ngx::casper::broker::Module
                    {
                       
                    public: // Static Const Data

                        static const char* const sk_rx_content_type_;
                        static const char* const sk_tx_content_type_;

                    private: // Static Data
                            
                        static Sideline::Settings s_sideline_settings_;

                    private: // Data
                        
                        std::string                                  type_;
                        std::string                                  id_;
                        std::map<std::string, std::set<std::string>> params_;
                        
                    private: // Helpers

                        Router                                       router_;
                        ext::Job                                     job_;
                        Json::FastWriter                             json_writer_;

                    protected: // Constructor(s)

                        Module (const broker::Module::Config& a_config, const broker::Module::Params& a_params,
                                ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf,
                                ngx_http_casper_broker_cdn_api_module_loc_conf_t& a_ngx_cdn_api_loc_conf);

                    public: // Constructor(s) / Destructor

                        virtual ~Module();

                    public: // Inherited Method(s) / Function(s)

                        virtual ngx_int_t Setup ();
                        virtual ngx_int_t Run   ();
                        
                    private: // Method(s) / Function(s)
                        
                        void OnRouterRequestSucceeded (const uint16_t a_status, const Json::Value& a_value);
                        void OnRouterRequestFailed    (const uint16_t a_status, const ::ev::Exception& a_exception);
                        
                    private: // Factory Method(s) / Function(s)
                        
                        Route*    RouteFactory    (const char* const a_name, const ::ev::Loggable::Data& a_loggable_data_ref);
                        Billing*  BillingFactory  (const ::ev::Loggable::Data& a_loggable_data_ref);
                        Sideline* SidelineFactory (const ::ev::Loggable::Data& a_loggable_data_ref);
                        
                    private: // Follow-up Job Method(s) / Function(s)
                        
                        void TrySubmitJob (const Json::Value& a_payload,
                                           std::function<void(bool a_submitted)> a_callback);
                   
                    public: // Static Method(s) / Function(s)

                        static ngx_int_t Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler);

                    private: // Static Method(s) / Function(s)

                        static void ReadBodyHandler (ngx_http_request_t* a_request);
                        static void CleanupHandler  (void*);

                    }; // end of class 'Module'

                } // end of namespace 'api'

            } // end of namespace 'api'

        } // end of namespace 'broker'

    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_API_MODULE_H_
