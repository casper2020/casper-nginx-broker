/**
 * @file types.h
 *
 * Copyright (c) 2017-2023 Cloudware S.A. All rights reserved.
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
#ifndef NRS_NGX_HTTP_CASPER_BROKER_MODULE_TYPES_H_
#define NRS_NGX_HTTP_CASPER_BROKER_MODULE_TYPES_H_

#include "ngx/casper/module/config/types.h"

/* SERVICE data types */
typedef struct {
    // ...
    ngx_str_t                    service_id;
    ngx_str_t                    resources_dir;
    ngx_int_t                    connection_validity;
    // ... redis ...
    ngx_casper_redis_conf_t      redis;
    // ... postgresql ...
    ngx_casper_postgresql_conf_t postgresql;
    // ... beanstalk ...
    ngx_casper_beanstalk_conf_t  beanstalkd;
    // ... curl ....
    ngx_casper_curl_conf_t       curl;
    // ... gatekeeper ...
    ngx_casper_gatekeeper_conf_t gatekeeper;
} nginx_broker_service_conf_t;

#endif // NRS_NGX_HTTP_CASPER_BROKER_MODULE_TYPES_H_
