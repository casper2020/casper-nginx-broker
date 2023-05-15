/**
 * @file ngx_http_casper_broker_module.h
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
#ifndef NRS_NGX_HTTP_CASPER_BROKER_MODULE_H_
#define NRS_NGX_HTTP_CASPER_BROKER_MODULE_H_

#include "ngx/casper/broker/module/types.h"

#ifdef __APPLE__
#pragma mark -
#endif

typedef struct {
    ngx_str_t  protocol; //!<
    ngx_str_t  host;     //!<
    ngx_uint_t port;     //!<
    ngx_str_t  path;     //!<
    ngx_str_t  location; //!<
} ngx_http_casper_broker_internal_redirect_conf_t;

typedef struct {
    ngx_str_t  content_disposition; //!<
} ngx_http_casper_broker_response_conf_t;

typedef struct {
    ngx_str_t temporary_prefix;   //!<
    ngx_str_t archive_prefix;     //!<
} ngx_http_casper_broker_directory_conf_t;

/* CDN data types */

typedef struct {
    ngx_str_t                                       ast;         //!<
    ngx_str_t                                       h2e_map;     //!<
    ngx_http_casper_broker_internal_redirect_conf_t redirect;    //!<
    ngx_http_casper_broker_response_conf_t          response;    //!<
    ngx_http_casper_broker_directory_conf_t         directories; //!<
} ngx_http_casper_broker_cdn_conf_t;

/* 😒 or 🤬 */
typedef struct {
    ngx_flag_t ordered_errors_json_serialization_enabled;
} ngx_http_casper_broker_awful_conf_t;


/* logs */
typedef struct {
    ngx_flag_t set;
    ngx_flag_t write_body;
} ngx_http_casper_broker_cc_log_conf_t;

/* logs */
typedef struct {
    ngx_int_t  max;
    ngx_flag_t enforce;
} ngx_http_casper_broker_jobs_timeouts_conf_t;

/* job */
typedef struct {
    ngx_int_t  max;
    ngx_flag_t enforce;
} ngx_http_casper_broker_job_timeouts_conf_t;

typedef struct {
    ngx_flag_t payload;
} ngx_http_casper_broker_job_logs_conf_t;

typedef struct {
    ngx_http_casper_broker_job_timeouts_conf_t timeouts;
    ngx_http_casper_broker_job_logs_conf_t     log;
} ngx_http_casper_broker_jobs_conf_t;

/* SESSION data types */
typedef struct {
    // ... session cookie ...
    ngx_str_t  cookie_name;             //!<
    ngx_str_t  cookie_domain;           //!<
    ngx_str_t  cookie_path;             //!<
} ngx_http_casper_broker_session_conf_t;

#ifdef __APPLE__
#pragma mark - module loc_conf_t
#endif

typedef struct {
    // ...
    ngx_str_t  location;                        //!<
    ngx_str_t  supported_content_types;         //!< JSON array of strings with supported Content-Type prefix / values.

    // ... session cookie ...
    ngx_http_casper_broker_session_conf_t session;
    // ... JWT ...
    ngx_str_t  jwt_iss;                         //!<
    ngx_str_t  jwt_rsa_public_key_uri;          //!<
    ngx_str_t  jwt_allowed_patch_members_set;   //!<
    // ... redirect ...
    ngx_http_casper_broker_cdn_conf_t cdn;
    // ... awful code ...
    ngx_http_casper_broker_awful_conf_t awful_conf;
    // ... logs ...
    ngx_http_casper_broker_cc_log_conf_t     cc_log;
    // ... jobs ...
    ngx_http_casper_broker_jobs_conf_t       jobs;
} ngx_http_casper_broker_module_loc_conf_t;

extern ngx_module_t ngx_http_casper_broker_module;
extern ngx_int_t    ngx_http_casper_broker_module_content_handler (ngx_http_request_t* a_r);

#endif // NRS_NGX_HTTP_CASPER_BROKER_MODULE_H_
