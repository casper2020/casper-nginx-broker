/**
 * @file executor.cc
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

#include "ngx/casper/broker/executor.h"

#include "ngx/casper/broker/tracker.h"

/**
 * @brief Default constructor.
 *
 * @param a_errors_tracker
 */
ngx::casper::broker::Executor::Executor (ngx::casper::broker::ErrorsTracker& a_errors_tracker)
    : errors_tracker_(a_errors_tracker)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::Executor::~Executor ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}
