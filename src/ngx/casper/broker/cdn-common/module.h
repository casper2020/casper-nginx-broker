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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_MODULE_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_MODULE_H_

#include "ngx/casper/broker/module.h"

#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include "ngx/casper/broker/cdn-common/types.h"
#include "ngx/casper/broker/cdn-common/archive.h"

#include <set>

#include "json/json.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                namespace common
                {
           
                    class Module : public ::ngx::casper::broker::Module
                    {
                        
                    protected: // Static Data
                        
                        static H2EMap      s_h2e_map_;
                        static bool        s_h2e_map_set_;
                        static Json::Value s_ast_config_;
                        static bool        s_ast_config_set_;

                    protected: // Data Type(s)
                        
                        typedef std::set<ngx_uint_t> ImplementedMethods;
                        typedef std::set<ngx_uint_t> ContentHeadersMethods;

                        typedef struct {
                            const ngx_str_t*  protocol_;
                            const ngx_str_t*  host_;
                            const ngx_uint_t* port_;
                            const ngx_str_t*  path_;
                            const ngx_str_t*  location_;
                        } Redirect;
                        
                        typedef struct {
                            const std::string default_value_;
                            const bool        allow_from_url_;
                        } ContentDispositionDefinitions;
                        
                        typedef struct {
                            ContentDispositionDefinitions disposition_;
                            ImplementedMethods            implemented_methods_;
                            ContentHeadersMethods         required_content_type_methods_;
                            ContentHeadersMethods         required_content_length_methods_;
                            Redirect                      redirect_;
                        } Settings;

                    private: // Data
                        
                        ContentType        content_type_;
                        ContentLength      content_length_;
                        ContentDisposition content_disposition_;

                    protected: // Data
                        
                        Settings           settings_;
                        Archive::RInfo     r_info_;

                    protected: // Helpers
                        
                        Json::Reader     json_reader_;
                        Json::FastWriter json_writer_;
                        Json::Value      json_value_;
                                                
                    protected: // Static Const Data
                        
                        static const std::string sk_empty_string_;
                        static const std::set<std::string> sk_preserved_upload_attrs_;
                        
                    protected: // Constructor(s)
                        
                        Module (const char* const a_name,
                                const broker::Module::Config& a_config, const broker::Module::Params& a_params, const Settings& a_settings);
                        
                    public: // Constructor(s) / Destructor
                        
                        virtual ~Module();
                        
                    public: // Inherited Method(s) / Function(s)
                        
                        virtual ngx_int_t Setup ();
                        
                    public: // Inline Method(s) / Function(s)
                        
                        const ContentType&        content_type () const;
                        const ContentLength&      content_length () const;
                        const ContentDisposition& content_disposition () const;
                        
                    }; // end of class 'Module'
                    
                    /**
                     * @return Read only access to \link ContentType \link.
                     */
                    inline const ContentType& Module::content_type () const
                    {
                        return content_type_;
                    }
                    
                    /**
                     * @return Read only access to \link ContentLength \link.
                     */
                    inline const ContentLength& Module::content_length () const
                    {
                        return content_length_;
                    }
                    
                    /**
                     * @return Read only access to \link ContentDisposition \link.
                     */
                    inline const ContentDisposition& Module::content_disposition () const
                    {
                        return content_disposition_;
                    }
                    
                } // end of namespace 'common'
                
            } // end of namespace 'api'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_MODULE_H_
