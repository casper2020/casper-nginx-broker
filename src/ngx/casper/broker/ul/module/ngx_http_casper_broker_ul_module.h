/**
 * @file ngx_http_casper_broker_ul_module.h
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
#ifndef NRS_NGX_HTTP_CASPER_BROKER_UL_MODULE_H_
#define NRS_NGX_HTTP_CASPER_BROKER_UL_MODULE_H_

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <ngx_errno.h>
}

#include "json/json.h"

#include <string>
#include "ngx/ngx_utils.h"

#include "cc/fs/file.h"
#include "cc/hash/md5.h"

#include "ngx/casper/broker/ul/errors.h"

#ifdef __APPLE__
#pragma mark - module loc_conf_t
#endif

/**
 * @brief Module configuration structure, applicable to a location scope
 */
typedef struct {
    ngx_flag_t                enable;                //!< flag that enables the module
    ngx_str_t                 log_token;             //!<
    ngx_str_t                 allowed_origins;       //!<
    ngx_str_t                 output_dir;            //!<
    ngx_uint_t                expires_in;            //!<
    ngx_flag_t                accept_multipart_body; //!<
    ngx_uint_t                max_content_length;    //!<
    ngx_str_t                 authorization_url;     //!<
    ngx_uint_t                authorization_ct;      //!<
    ngx_uint_t                authorization_tt;      //!<
    ngx_array_t*              allowed_magic_types;   //!<
    ngx_array_t*              allowed_magic_desc;    //!<
} ngx_http_casper_broker_ul_module_loc_conf_t;

typedef struct {
    std::string           directory_;
    std::string           name_;
    std::string           uri_;
    cc::hash::MD5         md5_;
    cc::fs::file::Writer* writer_;
    size_t                bytes_written_;
    int64_t               expires_in_;
} ngx_http_casper_broker_ul_file_t;

typedef struct {
    ngx::utls::HeadersMap               in_headers_;
    std::string                         content_type_;
    size_t                              content_length_;
    std::string                         content_disposition_;
    ngx::utls::nrs_ngx_multipart_body_t multipart_body_;
    std::string                         field_name_;
    ngx_http_casper_broker_ul_file_t    file_;
    ngx::casper::broker::ul::Errors*    error_tracker_;
    bool                                read_callback_again_;
    unsigned int                        read_callback_count_;
} ngx_http_casper_broker_ul_module_context_t;

extern ngx_module_t ngx_http_casper_broker_ul_module;
extern ngx_int_t    ngx_http_casper_broker_ul_module_content_handler (ngx_http_request_t* a_r);

#ifdef __APPLE__
#pragma mark - C++ Classes
#endif

#endif // NRS_NGX_HTTP_CASPER_BROKER_UL_MODULE_H_
