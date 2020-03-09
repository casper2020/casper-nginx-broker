/**
 * @file context.cc
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
#ifndef NRS_NGX_CASPER_EV_GLUE_H_
#define NRS_NGX_CASPER_EV_GLUE_H_

#include "osal/osal_singleton.h"
#include "osal/condition_variable.h"

#include "ev/loggable.h"
#include "ev/object.h"
#include "ev/exception.h"

#include "ev/ngx/shared_glue.h"

#include <string>
#include <map>

namespace ngx
{

    namespace casper
    {
        
        namespace ev
        {
            
            class Glue final : public ::ev::ngx::SharedGlue, public osal::Singleton<Glue>
            {
                
            private: // Static Data
                
                static bool                        s_initialized_;
                
            private: // Static Const Ptrs
                
                static const ::ev::Loggable::Data* s_loggable_data_ptr_;
                
            private: // Data

                std::string scheduler_socket_fn_;
                std::string shared_handler_socket_fn_;

            public: // Method(s) / Function(s)
                
                void Startup      (const ::ev::Loggable::Data& a_loggable_data_ref, const std::map<std::string, std::string>& a_config,
                                   Callbacks& o_callbacks);
                void Shutdown     (int a_sig_no);
                
            public: // Inline Method(s) / Function(s)
                
                bool IsInitialized () const;
                
            private: // Method(s) / Function(s)
                
                void OnFatalException (const ::ev::Exception& a_ev_exception);
                
            }; // end of class 'Glue'
            
            inline bool Glue::IsInitialized () const
            {
                return s_initialized_;
            }
            
        } // end of namespace 'ev'
        
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_EV_GLUE_H_
