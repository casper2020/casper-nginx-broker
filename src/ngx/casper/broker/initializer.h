/**
 * @file initializer.h
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
#ifndef NRS_NGX_CASPER_INITIALIZER_H_
#define NRS_NGX_CASPER_INITIALIZER_H_

#include "osal/osal_singleton.h"

#include <map>    // std::map
#include <string> // std::string

#include "ev/ngx/includes.h"

#include "cc/global/initializer.h"

namespace ngx
{

    namespace casper
    {

        namespace broker
        {

            // ---- //
            class Initializer;
            class OneShotInitializer final : public ::osal::Initializer<broker::Initializer>
            {
                
            public: // Constructor(s) / Destructor
                
                OneShotInitializer (broker::Initializer& a_instance)
                    : ::osal::Initializer<broker::Initializer>(a_instance)
                {
                    /* empty */
                }
                virtual ~OneShotInitializer ()
                {
                    /* empty */
                }
                
            }; // end of class 'BridgeInitializer'

            // ---- //
            class Initializer final : public osal::Singleton<Initializer, OneShotInitializer>
            {

            public: // Static Const Data

                static const char* const k_resources_dir_key_lc_;

            private: // Const Static Data
                
                static const char* const k_ngx_http_i18_file_missing_error_;

            private: // Static Data

                static bool             s_initialized_;

            public: // One-shot Call Method(s) / Function(s)

                void Startup  (ngx_http_request_t* a_r, const std::map<std::string, std::string>& a_configs);
                void Shutdown (bool a_for_cleanup_only);

                void PreStartup    (const ngx_core_conf_t* a_config, const ngx_cycle_t* a_cycle, const bool a_master,
                                    const bool a_standalone);

            private: // One-shot Call Method(s) / Function(s)
                
                void OnGlobalInitializationCompleted (const ::cc::global::Process& a_process,
                                                      const ::cc::global::Directories& a_directories,
                                                      const void* a_args,
                                                      ::cc::global::Logs& a_logs);
                
                void OnFatalException                 (const cc::Exception& a_exception);

            public: // Static Method(s) / Function(s)

                static bool IsInitialized ();

            }; // end of class Initializer

            /**
             * @return True if this singleton is initialized, false otherwise.
             */
            inline bool Initializer::IsInitialized ()
            {
                return s_initialized_;
            }

        } // end of namespace 'broker'

    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_INITIALIZER_H_
