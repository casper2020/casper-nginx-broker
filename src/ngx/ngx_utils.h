/**
 * @file ngx_utils.h
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
#ifndef NRS_NGX_UTILS_H_
#define NRS_NGX_UTILS_H_

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <ngx_errno.h>
    #include <ngx_palloc.h>
}

#include <map>
#include <string>

#include "json/json.h"

namespace ngx
{
    class utls
    {
        
    public: // Data Type(s)
        
        typedef std::map<std::string, std::string> HeadersMap;
        
        typedef struct {
            size_t       offset;
            ngx_str_t    boundary;
            ngx_array_t  ranges;
        } nrs_ngx_multipart_header_info_t;
        
        typedef struct {
            std::string value_;
            unsigned    set_:1;
            
        } nrs_ngx_multipart_string_piece_t;
        
        typedef struct {

            nrs_ngx_multipart_string_piece_t boundary_;
            nrs_ngx_multipart_string_piece_t content_disposition_;
            nrs_ngx_multipart_string_piece_t content_type_;
            nrs_ngx_multipart_string_piece_t terminator_;
            
            size_t                           offset_;
            unsigned                         done_:1;
            u_char                           last_;
            
            unsigned                         enabled_:1;

        } nrs_ngx_multipart_body_t;

    
    public: // Static Method(s) / Function(s)
        
        static void             nrs_ngx_unescape_uri                (ngx_str_t* a_value);
        
        static void             nrs_ngx_complex_value_to_string     (ngx_http_request_t* a_r,
                                                                     ngx_http_complex_value_t* a_cv, ngx_str_t* o_sv);
        
        static ngx_str_t*       nrs_ngx_copy_string                 (ngx_http_request_t* a_r,
                                                                     const std::string& a_value);
        
        static ngx_table_elt_t* nrs_ngx_add_response_header          (ngx_http_request_t* a_r,
                                                                      const ngx_str_t* a_name, const ngx_str_t* a_value);
        
        static ngx_int_t        nrs_ngx_send_only_header_response    (ngx_http_request_t* a_r,
                                                                      ngx_uint_t a_http_status_code);

        static void             nrs_ngx_read_in_headers              (ngx_http_request_t* a_r, HeadersMap& o_map);
        static void             nrs_ngx_read_out_headers             (ngx_http_request_t* a_r, HeadersMap& o_map);
        
        static ngx_int_t        nrs_ngx_parse_authorization          (const std::string& a_header, std::string& o_type, std::string& o_credentials);
        static ngx_int_t        nrs_ngx_parse_basic_authentication   (ngx_http_request_t* a_r,
                                                                      const std::string& a_value, std::string& o_username, std::string& o_password);
        static ngx_int_t        nrs_ngx_parse_content_type           (const std::string& a_header, std::string& o_type);
        static ngx_int_t        nrs_ngx_parse_content_type           (const std::string& a_header, std::string& o_type, std::string& o_charset);
        static ngx_int_t        nrs_ngx_parse_content_length         (const std::string& a_header, size_t& o_length);
        static ngx_int_t        nrs_ngx_parse_content_disposition    (const std::string& a_header,
                                                                      std::string& o_disposition, std::string& o_field_name, std::string& o_file_name);

        static ngx_int_t        nrs_ngx_parse_multipart_content_type (const std::string& a_content_type,
                                                                      ngx::utls::nrs_ngx_multipart_header_info_t& o_info);        

        static ngx_int_t        nrs_ngx_parse_multipart_body         (const u_char* a_data, size_t a_length,
                                                                      nrs_ngx_multipart_body_t& o_body,
                                                                      size_t& o_length);
        
        static ngx_int_t       nrs_ngx_parse_arg_as_uri              (ngx_http_request_t* a_r, const char* const a_name,
                                                                      std::string& o_value);
        
        static ngx_int_t       nrs_ngx_get_arg                       (ngx_http_request_t* a_r, ngx_str_t* a_args, const char* const a_name,
                                                                      std::string& o_value);
        static ngx_int_t       nrs_ngx_get_args                      (ngx_http_request_t* a_r, ngx_str_t* a_args,
                                                                      std::map<std::string, std::string>& o_map);

        static ngx_int_t       nrs_ngx_unescape_uri                  (ngx_http_request_t* a_r,
                                                                      const char* const a_name, std::string& o_uri);
        
        static void           nrs_ngx_get_headers                    (const ngx_list_part_t* a_list, Json::Value& o_array);
        static void           nrs_ngx_get_in_untouched_headers       (const ngx_http_request_t* a_r, Json::Value& o_array);
        static void           nrs_ngx_get_out_untouched_headers      (const ngx_http_request_t* a_r, Json::Value& o_array);
        
    };
    
}

#endif // NRS_NGX_UTILS_H_
