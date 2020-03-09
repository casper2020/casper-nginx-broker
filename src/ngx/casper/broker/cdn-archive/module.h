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
#ifndef NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_MODULE_H_
#define NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_MODULE_H_

#include "ngx/casper/broker/cdn-common/module.h"

#include "ngx/casper/broker/cdn-common/types.h"
#include "ngx/casper/broker/cdn-common/archive.h"

#include "ngx/casper/broker/cdn-archive/module/ngx_http_casper_broker_cdn_archive_module.h"

#include "ngx/casper/broker/cdn-archive/db/synchronization.h"
#include "ngx/casper/broker/cdn-common/db/billing.h"

#include "ngx/casper/broker/ext/job.h"

#include "json/json.h"

#include <map>    // std::map
#include <string> // std::string

namespace ngx
{

    namespace casper
    {

        namespace broker
        {

            namespace cdn
            {

                namespace archive
                {

                    class Module final : public ::ngx::casper::broker::cdn::common::Module
                    {
                       
                    public: // Static Const Data

                        static const char* const sk_rx_content_type_;
                        static const char* const sk_tx_content_type_;

                    private: // Data
                        
                        Archive             archive_;

                        XNumericID          x_id_;
                        XStringID           x_moves_uri_;
                        XAccess             x_access_;
                        XFilename           x_filename_;
                        XArchivedBy         x_archived_by_;
                        XContentType        x_content_type_;
                        XContentDisposition x_content_disposition_;
                        XBillingID          x_billing_id_;
                        XBillingType        x_billing_type_;
                        XValidateIntegrity  x_validate_integrity_;
                        
                    private: // Static Data
                        
                        static db::Synchronization::Settings s_sync_registry_settings_;
                        static Archive::Settings             s_archive_settings_;
                        
                    private: // Helper(s)
                        
                        typedef struct {
                            db::Synchronization::Data sync_data_;
                            db::Synchronization*      synchronization_;
                            common::db::Billing*      billing_;
                        } DB;
                        
                        DB                            db_;
                        
                    private: // Extensions
                        
                        ext::Job                      job_;
                        
                    protected: // Constructor(s)

                        Module (const broker::Module::Config& a_config, const broker::Module::Params& a_params,
                                ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf,
                                ngx_http_casper_broker_cdn_archive_module_loc_conf_t& a_ngx_cdn_archive_loc_conf);

                    public: // Constructor(s) / Destructor

                        virtual ~Module();

                    public: // Inherited Method(s) / Function(s)

                        virtual ngx_int_t Setup ();
                        virtual ngx_int_t Run   ();
                        
                    private: // Method(s) / Function(s)
                        
                        ngx_int_t Perform ();
                        void      HEAD    ();
                        void      GET     ();
                        void      POST    ();
                        void      PUT     ();
                        void      PATCH   ();
                        void      DELETE  ();
                        
                    private: // Method(s) / Function(s) - Billing
                        
                        ngx_int_t FetchBillingInformation            ();
                        void      OnFetchBillingInformationSucceeded (const uint16_t a_status, const Json::Value& a_json);
                        void      OnFetchBillingInformationFailed    (const uint16_t a_status, const ::ev::Exception& a_ev_exception);

                    private: // Method(s) / Function(s) - Registry / Synchronization

                        ngx_int_t RegisterSyncOperation            ();
                        void      OnRegisterSyncOperationSucceeded (const db::Synchronization::Operation a_operation,
                                                                    const uint16_t a_status, const Json::Value& a_json);
                        void      OnRegisterSyncOperationFailed    (const db::Synchronization::Operation a_operation,
                                                                    const uint16_t a_status, const ::ev::Exception& a_ev_exception);

                    private: // Method(s) / Function(s)

                        void SetSuccessResponse     (const Archive::RInfo& a_info, const Json::Value* o_other_attrs = nullptr);
                        void SetNotModifiedResponse ();
                        void SetNoContentResponse   ();
                        void Commit                 ();
                        void TryRollback            ();
                        void TrySubmitJob           (const Json::Value& a_payload);
                        
                    public: // Static Method(s) / Function(s)

                        static ngx_int_t Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler);

                    private: // Static Method(s) / Function(s)

                        static void ReadBodyHandler (ngx_http_request_t* a_request);
                        static void CleanupHandler  (void*);

                    }; // end of class 'Module'

                } // end of namespace 'archive'

            } // end of namespace 'api'

        } // end of namespace 'broker'

    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_MODULE_H_
