/**
 * @file base.h
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
#ifndef NRS_NGX_CASPER_BROKER_EXT_BASE_H_
#define NRS_NGX_CASPER_BROKER_EXT_BASE_H_

#include "ngx/casper/broker/module.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace ext
            {
                
                class Base
                {
                    
                protected: // Refs
                    
                    Module::CTX& ctx_;
                    
                protected: // Ptrs
                    
                    Module* module_ptr_;
                    
                public: // Constructor(s) / Destructor
                    
                    Base (broker::Module::CTX& a_ctx, Module* a_module);                    
                    virtual ~Base();
                    
                }; // end of Base
                
            } // end of namespace 'ext'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_EXT_BASE_H_

