/**
 * @file ngx_http_casper_broker_cdn_module.h
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
#ifndef NRS_NGX_HTTP_CASPER_BROKER_CDN_MODULE_H_
#define NRS_NGX_HTTP_CASPER_BROKER_CDN_MODULE_H_

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <ngx_errno.h>
}

#ifdef __APPLE__
#pragma mark - module loc_conf_t
#endif

/**
 * @brief Module configuration structure, applicable to a location scope
 */
typedef struct {
    ngx_flag_t            enable;                           //!< flag that enables the module
    ngx_str_t             log_token;                        //!<
    ngx_str_t             jwt_prefix;                       //!<
} ngx_http_casper_broker_cdn_module_loc_conf_t;

extern ngx_module_t ngx_http_casper_broker_cdn_module;

#endif // NRS_NGX_HTTP_CASPER_BROKER_CDN_MODULE_H_
