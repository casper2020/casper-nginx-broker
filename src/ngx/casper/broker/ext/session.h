/**
 * @file session.h
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
#ifndef NRS_NGX_CASPER_BROKER_EXT_SESSION_H_
#define NRS_NGX_CASPER_BROKER_EXT_SESSION_H_

#include "ngx/casper/broker/ext/base.h"

#include "ev/casper/session.h"

#include <functional>

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace ext
            {
                
                class Session : public ext::Base
                {
                    
                protected: // Data
                    
                    ::ev::casper::Session session_;
                    
                private: // Ptr Func
                    
                    std::function<void(const bool a_async_request, const ::ev::casper::Session& a_session)> success_callback_;
                    std::function<void()>                                                                   failure_callback_;
                    
                public: // Constructor(s) / Destructor
                    
                    Session (broker::Module::CTX& a_ctx, Module* a_module, const broker::Module::Params& a_params);
                    virtual ~Session();                    
                    
                public: // Inherited Method(s) / Function(s)
                    
                    virtual ngx_int_t Fetch (std::function<void(const bool a_async_request, const ::ev::casper::Session& a_session)> a_success,
                                             std::function<void()> a_failure = nullptr);
                    
                }; // end of Job
                
            } // end of namespace 'ext'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_EXT_SESSION_H_

