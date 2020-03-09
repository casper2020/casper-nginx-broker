/**
 * @file base.cc
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

#include "ngx/casper/broker/ext/base.h"


/**
 * @brief Default constructor.
 *
 * @param a_ctx
 * @param a_module
 */
ngx::casper::broker::ext::Base::Base (ngx::casper::broker::Module::CTX& a_ctx, ngx::casper::broker::Module* a_module)
    : ctx_(a_ctx), module_ptr_(a_module)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::ext::Base::~Base ()
{
    /* empty */
}

