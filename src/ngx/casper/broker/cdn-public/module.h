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
#ifndef NRS_NGX_CASPER_BROKER_CDN_PUBLIC_MODULE_H_
#define NRS_NGX_CASPER_BROKER_CDN_PUBLIC_MODULE_H_

#include "ngx/casper/broker/cdn-common/module.h"

#include "ngx/casper/broker/cdn-public/module/ngx_http_casper_broker_cdn_public_module.h"

#include "ngx/casper/broker/cdn-common/archive.h"
#include "ngx/casper/broker/cdn-common/types.h"

namespace ngx
{

    namespace casper
    {

        namespace broker
        {

            namespace cdn
            {

                namespace pub
                {

                    class Module final : public ::ngx::casper::broker::cdn::common::Module
                    {

                    private: // Data Type(s)

                        typedef struct {
                            Json::Value allowed_iss_;
                            std::string rsa_public_key_uri_;
                            bool        set_;
                        } JWTConfig;
                        
                        typedef struct {
                            Json::Value value_;
                            bool        set_;
                        } ASTConfig;

                    public: // Static Const Data

                        static const char* const sk_rx_content_type_;
                        static const char* const sk_tx_content_type_;
                        
                    private: // Static Data
                        
                        static Archive::Settings s_public_archive_settings_;
                        static ASTConfig         s_public_ast_config_;
                        static JWTConfig         s_jwt_config_;
                        
                    private: // Data
                        
                        std::map<std::string, std::string> fake_headers_;
                        
                    private: // Helper(s)
                        
                        Archive             archive_;
                        
                    private: // Data
                        
                        XContentType        x_content_type_;
                        XContentDisposition x_content_disposition_;
                        XFilename           x_filename_;
                        std::string         uri_;

                    protected: // Constructor(s)

                        Module (const broker::Module::Config& a_config, const broker::Module::Params& a_params,
                                ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf,
                                ngx_http_casper_broker_cdn_public_module_loc_conf_t& a_ngx_cdn_public_loc_conf);

                    public: // Constructor(s) / Destructor

                        virtual ~Module();

                    public: // Inherited Method(s) / Function(s)

                        virtual ngx_int_t Setup ();
                        virtual ngx_int_t Run   ();
                        
                    private: // Method(s) / Function(s)
                        
                        void HEAD ();
                        void GET  ();
                        
                    public: // Static Method(s) / Function(s)

                        static ngx_int_t Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler);

                    private: // Static Method(s) / Function(s)

                        static void ReadBodyHandler (ngx_http_request_t* a_request);
                        static void CleanupHandler  (void*);

                    }; // end of class 'Module'

                } // end of namespace 'pub'

            } // end of namespace 'api'

        } // end of namespace 'broker'

    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_PUBLIC_MODULE_H_
