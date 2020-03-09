/**
 * @file executor.h
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
#ifndef NRS_NGX_CASPER_BROKER_EXECUTOR_H_
#define NRS_NGX_CASPER_BROKER_EXECUTOR_H_

#include <map>        // std::map
#include <string>     // std::string
#include <map>        // std::map
#include <functional> // std::function

#include "ev/scheduler/scheduler.h"

#include "ngx/casper/broker/types.h"
#include "ngx/casper/broker/errors.h"
#include "ngx/casper/broker/exception.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            class Executor : public ::ev::scheduler::Client
            {
                
            protected: // Refs
                
                ErrorsTracker& errors_tracker_;

            public: // Constructor(s) / Destructor

                Executor (ErrorsTracker& a_errors_tracker);
                virtual ~Executor();

            }; // end of class 'Executor'
            
        } // end of namespace 'broker'

    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_EXECUTOR_H_
