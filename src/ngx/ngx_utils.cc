/**
 * @file ngx_utils.cc
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

#include "ngx/ngx_utils.h"

#include <cstdlib> // std::atoll
#include <algorithm> // std::transform

/**
 * @brief URL unescape.
 * 
 * @parama_value
 */
void ngx::utls::nrs_ngx_unescape_uri (ngx_str_t* a_value)
{
    if ( 0 == a_value->len || NULL == a_value->data ) {
        return;
    }
 
    u_char* dst = a_value->data;
    u_char *src = a_value->data;
    ngx_unescape_uri(&dst, &src, a_value->len, NGX_UNESCAPE_URI);
    if ( dst < src ) {
        *dst = '\0';
        a_value->len = dst - a_value->data;
    }
}

/**
 * @brief Convert a complex value to a string.
 *
 * @param a_r
 * @param a_cv
 * @param o_sv
 */
void ngx::utls::nrs_ngx_complex_value_to_string (ngx_http_request_t* a_r,
                                                 ngx_http_complex_value_t* a_cv, ngx_str_t* o_sv)
{
    ngx_http_complex_value(a_r, a_cv, o_sv);
    nrs_ngx_unescape_uri(o_sv);
}

/**
 * @brief Copy a std::string in to a ngx_str_t.
 *
 * @param a_r
 * @param a_value
 *
 * @return
 */
ngx_str_t* ngx::utls::nrs_ngx_copy_string (ngx_http_request_t* a_r,
                                           const std::string& a_value)
{
    ngx_str_t* str = (ngx_str_t*)ngx_pcalloc(a_r->pool, sizeof(ngx_str_t));
    if ( NULL == str ) {
        return NULL;
    }
    
    str->len  = a_value.length();
    str->data = (u_char*)ngx_palloc(a_r->pool, str->len);
    if ( NULL == str->data ) {
        return NULL;
    }
    ngx_memcpy(str->data, a_value.data(), str->len);

    return str;
}

/**
 * @brief Add a response header to a request.
 *
 * @param a_r
 * @param a_value
 */
ngx_table_elt_t* ngx::utls::nrs_ngx_add_response_header (ngx_http_request_t* a_r,
                                                         const ngx_str_t* a_name, const ngx_str_t* a_value)
{
    ngx_table_elt_t* h = (ngx_table_elt_t*)ngx_list_push(&a_r->headers_out.headers);
    
    if ( NULL == h ) {
        return NULL;
    }
    
    h->hash       = 1;
    h->key.len    = a_name->len;
    h->key.data   = a_name->data;
    h->value.len  = a_value->len;
    h->value.data = a_value->data;
    
    return h;
}

/**
 * @brief Send a header as a response to a request.
 *
 * @param a_r
 * @param a_status HTTP status code.
 */
ngx_int_t ngx::utls::nrs_ngx_send_only_header_response (ngx_http_request_t* a_r, ngx_int_t a_status)
{
    ngx_int_t rc;
    
    a_r->header_only                  = 1;
    a_r->headers_out.content_length_n = 0;
    a_r->headers_out.status           = a_status;
    
    rc = ngx_http_send_header(a_r);
    
    if ( rc > NGX_HTTP_SPECIAL_RESPONSE ) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    return rc;
}

/**
 * @brief Load a request in-headers to a map.
 *
 * @param a_r
 * @param o_map
 */
void ngx::utls::nrs_ngx_read_in_headers (ngx_http_request_t* a_r, ngx::utls::HeadersMap& o_map)
{
    o_map.clear();
    
    ngx_list_part_t* list_part = &a_r->headers_in.headers.part;
    while ( NULL != list_part ) {
        // ... for each element ...
        ngx_table_elt_t* header = (ngx_table_elt_t*)list_part->elts;
        for ( ngx_uint_t index = 0 ; list_part->nelts > index ; ++index ) {
            // ... if is set ...
            if ( 0 < header[index].key.len && 0 < header[index].value.len ) {
                // ... keep track of it ...
                std::string key   = std::string(reinterpret_cast<char const*>(header[index].key.data), header[index].key.len);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                
                o_map[key] = std::string(reinterpret_cast<char const*>(header[index].value.data), header[index].value.len);
            }
        }
        // ... next...
        list_part = list_part->next;
    }
}

/**
 * @brief Load a request out-headers to a map.
 *
 * @param a_r
 * @param o_map
 */
void ngx::utls::nrs_ngx_read_out_headers (ngx_http_request_t* a_r, ngx::utls::HeadersMap& o_map)
{
    o_map.clear();
    
    ngx_list_part_t* list_part = &a_r->headers_out.headers.part;
    while ( NULL != list_part ) {
        // ... for each element ...
        ngx_table_elt_t* header = (ngx_table_elt_t*)list_part->elts;
        for ( ngx_uint_t index = 0 ; list_part->nelts > index ; ++index ) {
            // ... if is set ...
            if ( 0 < header[index].key.len && 0 < header[index].value.len ) {
                // ... keep track of it ...
                std::string key   = std::string(reinterpret_cast<char const*>(header[index].key.data), header[index].key.len);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                
                o_map[key] = std::string(reinterpret_cast<char const*>(header[index].value.data), header[index].value.len);
            }
        }
        // ... next...
        list_part = list_part->next;
    }
}

/**
 * @brief Parse 'Authorization' header.
 *
 * @param a_header
 * @param o_type
 * @param o_credentials
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_authorization (const std::string& a_header, std::string& o_type, std::string& o_credentials)
{
    const char* type_ptr = strcasestr(a_header.c_str(), "Authorization:");
    if ( nullptr == type_ptr ) {
        return NGX_ERROR;
    }
    type_ptr += strlen("Authorization:");
    while ( ' ' == type_ptr[0] ) {
        type_ptr++;
    }
    
    if ( '\0' == type_ptr[0] ) {
        return NGX_ERROR;
    }
    
    while ( not ( '\0' == type_ptr[0] || ' ' == type_ptr[0] ) ) {
        o_type += type_ptr[0];
        type_ptr++;
    }
    
    while ( ' ' == type_ptr[0] ) {
        type_ptr++;
    }
    
    if ( '\0' == type_ptr[0] ) {
        return NGX_ERROR;
    }
    
    while ( not ( '\0' == type_ptr[0] || ' ' == type_ptr[0] ) ) {
        o_credentials += type_ptr[0];
        type_ptr++;
    }
    
    return NGX_OK;
}

/**
 * @brief Parse 'Authorization' header.
 *
 * @param a_r
 * @param o_value
 * @param o_username
 * @param o_password
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_basic_authentication (ngx_http_request_t* a_r,
                                                         const std::string& a_value, std::string& o_username, std::string& o_password)
{
    ngx_str_t encoded;
    encoded.data = (u_char*)a_value.c_str();
    encoded.len  = a_value.length();
    
    while ( encoded.len && ' ' == encoded.data[0] ) {
        encoded.len--;
        encoded.data++;
    }
    
    if ( encoded.len == 0 ) {
        return NGX_DECLINED;
    }
    
    ngx_str_t auth;
    auth.len  = ngx_base64_decoded_length(encoded.len);
    auth.data = (u_char*)ngx_pnalloc(a_r->pool, auth.len + 1);
    if ( NULL == auth.data ) {
        return NGX_ERROR;
    }
    
    if ( NGX_OK != ngx_decode_base64(&auth, &encoded) ) {
        return NGX_DECLINED;
    }
    
    auth.data[auth.len] = '\0';
    
    ngx_uint_t len;
    for (len = 0; len < auth.len; len++) {
        if (auth.data[len] == ':') {
            break;
        }
    }
    
    if ( 0 == len || len == auth.len) {
        return NGX_DECLINED;
    }
    
    o_username = std::string(reinterpret_cast<const char*>(auth.data), len);
    o_password = std::string(reinterpret_cast<const char*>(&auth.data[len + 1]), auth.len - len - 1);
    
    return NGX_OK;
}

/**
 * @brief Parse 'Content-Type' header.
 *
 * @param a_header
 * @param o_type
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_content_type (const std::string& a_header, std::string& o_type)
{
    const char* type_ptr = strcasestr(a_header.c_str(), "Content-Type:");
    if ( nullptr == type_ptr ) {
        return NGX_ERROR;
    }
    type_ptr += strlen("Content-Type:");
    while ( ' ' == type_ptr[0] && not ( '\0' == type_ptr[0] || ';' == type_ptr[0] ) ) {
        type_ptr++;
    }
    
    if ( '\0' == type_ptr[0] ) {
        return NGX_ERROR;
    }
    
    const char* sep = strchr(type_ptr, ';');
    if ( nullptr != sep ) {
        o_type = std::string(type_ptr, sep - type_ptr);
    } else {
        o_type = type_ptr;
    }
    
    return NGX_OK;
}


/**
 * @brief Parse 'Content-Type' header.
 *
 * @param a_header
 * @param o_type
 * @param o_charset
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_content_type (const std::string& a_header, std::string& o_type, std::string& o_charset)
{
    const char* type_ptr = strcasestr(a_header.c_str(), "Content-Type:");
    if ( nullptr == type_ptr ) {
        return NGX_ERROR;
    }
    type_ptr += strlen("Content-Type:");
    while ( ' ' == type_ptr[0] && not ( '\0' == type_ptr[0] || ';' == type_ptr[0] ) ) {
        type_ptr++;
    }
    
    if ( '\0' == type_ptr[0] ) {
        return NGX_ERROR;
    }
    
    const char* sep = strchr(type_ptr, ';');
    if ( nullptr != sep ) {
        o_type = std::string(type_ptr, sep - type_ptr);
        const char* charset_ptr = strcasestr(sep + sizeof(char), "charset=");
        if ( nullptr != charset_ptr ) {
            charset_ptr += ( sizeof(char) * 8 );
            while ( ' ' == charset_ptr[0] && not ( '\0' == charset_ptr[0]  ) ) {
                charset_ptr++;
            }
            o_charset = charset_ptr;
        }
    } else {
        o_type    = type_ptr;
        o_charset = "";
    }
    
    return NGX_OK;
}

/**
 * @brief Parse 'Content-Length' header.
 *
 * @param a_header
 * @param o_length
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_content_length (const std::string& a_header, size_t& o_length)
{
    const char* length_ptr = strcasestr(a_header.c_str(), "Content-Length:");
    if ( nullptr == length_ptr ) {
        return NGX_ERROR;
    }
    length_ptr += strlen("Content-Length:");
    while ( ' ' == length_ptr[0] && ( '\0' != length_ptr[0]) ) {
        length_ptr++;
    }
    if ( '\0' == length_ptr[0] ) {
        return NGX_ERROR;
    }
    
    o_length = std::atoll(length_ptr);
    
    return NGX_OK;
}

/**
 * @brief Parse 'Content-Disposition' header.
 *
 * @param a_header       One of:
 *                        - inline
 *                        - attachment
 *                        - form-data
 * @param o_disposition  The original name of the file transmitted ( optional ).
 * @param o_field_name   The name of the HTML field in the form that the content of this subpart refer to.
 *                       A name with a value of '_charset_' indicates that the part is not an HTML field, 
 *                       but the default charset to use for parts without explicit charset information.
 * @param o_file_name
 *
 * @return NGX_OK or NGX_ERROR.
 */
ngx_int_t ngx::utls::nrs_ngx_parse_content_disposition (const std::string& a_header,
                                                        std::string& o_disposition, std::string& o_field_name, std::string& o_file_name)
{
    
    o_disposition = "";
    o_field_name  = "";
    o_file_name   = "";
    
    const char* disposition_ptr = strcasestr(a_header.c_str(), "Content-Disposition:");
    if ( nullptr == disposition_ptr ) {
        return NGX_ERROR;
    }
    disposition_ptr += strlen("Content-Disposition:");
    while ( ' ' == disposition_ptr[0] && not ( '\0' == disposition_ptr[0] || ';' == disposition_ptr[0] ) ) {
        disposition_ptr++;
    }
    
    if ( '\0' == disposition_ptr[0] ) {
        return NGX_ERROR;
    }
    
    const char* sep = strchr(disposition_ptr, ';');
    if ( nullptr != sep ) {
        o_disposition = std::string(disposition_ptr, sep - disposition_ptr);
        disposition_ptr += ( sep - disposition_ptr );
        // ... has name ?
        typedef struct {
            const char* const name_;
            std::string&      value_;
        } Entry;
        
        const Entry entries[2] = {
            { "name"    , o_field_name     },
            { "filename", o_file_name }
        };
        for ( auto entry : entries ) {
            
            sep = strchr(disposition_ptr, ';');
            if ( nullptr != sep ) {
                
                sep++;
                while ( ' ' == sep[0] && not ( '\0' == sep[0] ) ) {
                    sep++;
                }
                
                const std::string search_prefix = std::string(entry.name_) + "=\"";
                
                if ( 0 == strncasecmp(search_prefix.c_str(), sep, search_prefix.length()) ) {
                    disposition_ptr = sep + search_prefix.length();
                    sep = strchr(disposition_ptr, '"');
                    if ( nullptr != sep ) {
                        entry.value_ = std::string(disposition_ptr, sep - disposition_ptr);
                        sep++;
                    } else {
                        return NGX_ERROR;
                    }
                }
                
            }
        }
        
    } else {
        o_disposition = disposition_ptr;
    }
    
    // ... _charset_ exception ...
    if ( 0 != strncasecmp(o_field_name.c_str(), "_charset_", strlen("_charset_")) ) {
        o_field_name = "";
    }
    
    return NGX_OK;
}

/**
 * @brief Parse 'Content-Type' header, expecting media-type 'multipart/form-data' and 'boundary'.
 *
 * @param a_content_length
 * @param o_content_type
 *
 * @return
 */
ngx_int_t ngx::utls::nrs_ngx_parse_multipart_content_type (const std::string& a_content_type,
                                                           ngx::utls::nrs_ngx_multipart_header_info_t& o_info)
{
    
    //
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
    //
    // supported:
    //
    // Content-Type: multipart/form-data; boundary=something
    //
    // media-type - multipart/form-data
    // boundary   - For multipart entities the boundary directive is required!
    
    // ... media-type ...
    const char* content_type_str = strcasestr(a_content_type.c_str(), "Content-Type:");
    if ( nullptr == content_type_str ) {
        return NGX_ERROR;
    }
    content_type_str += strlen("Content-Type:");
    while ( ' ' == content_type_str[0] && ( '\0' != content_type_str[0] && ';' != content_type_str[0] ) ) {
        content_type_str++;
    }
    if ( '\0' == content_type_str[0] ) {
        return NGX_ERROR;
    }
    
    const size_t min_length   = strlen("multipart/form-data");
    const size_t value_length = strlen(content_type_str);
    
    if ( value_length < min_length || 0 != strncasecmp(content_type_str, "multipart/form-data", min_length) ) {
        return NGX_ERROR;
    }
    
    const char* content_type_end = strchr(content_type_str, ';');
    if ( nullptr == content_type_end ) {
        return NGX_ERROR;
    }
    content_type_end += sizeof(char);
    
    // ... boundary ...
    const char* boundary_start = strcasestr(content_type_end, "boundary=");
    if ( nullptr == boundary_start ) {
        return NGX_ERROR;
    }
    boundary_start += strlen("boundary=");
    
    o_info.offset = strlen(boundary_start);
    
    return NGX_OK;
}

/**
 * @brief Parse a 'multipart/form-data' w/ 'boundary' body.
 *
 * @param a_data
 * @param a_length
 * @param o_length Number of u_char consumed
 *
 * @return
 */
ngx_int_t ngx::utls::nrs_ngx_parse_multipart_body (const u_char* a_data, size_t a_length,
                                                   ngx::utls::nrs_ngx_multipart_body_t& o_body,
                                                   size_t& o_length)
{
    
    o_length = 0;
    
    if ( 1 == o_body.done_ ) {
        return NGX_DONE;
    }
    
    //
    // expected:
    //
    // <boundary>\r\n
    // Content-Disposition:...\r\n
    // Content-Type:...\r\n\r\n
    //
     
    const char* chr = reinterpret_cast<const char*>(a_data);
    
    auto pieces = { &o_body.boundary_, &o_body.content_disposition_, &o_body.content_type_, &o_body.terminator_ };
    for ( auto piece : pieces ) {
        
        if ( 1 == piece->set_ ) {
            continue;
        }
        
        while ( o_length < a_length && ( not ( '\r' == chr[0] || '\n' == chr[0] ) ) ) {
            piece->value_ += chr[0];
            o_body.last_   = (u_char)chr[0];
            o_body.offset_++;
            o_length++;
            chr++;
        }

        if ( &o_body.terminator_ == piece ) {
            while ( o_length < a_length && ( '\r' == chr[0] || '\n' == chr[0] ) ) {
                piece->value_ += chr[0];
                o_body.last_   = (u_char)chr[0];
                o_body.offset_++;
                o_length++;
                chr++;
            }
        } else {
            size_t ctn = 0;
            while ( o_length < a_length && ( '\r' == chr[0] || '\n' == chr[0] ) && ctn < 2 ) {
                o_body.last_   = (u_char)chr[0];
                o_body.offset_++;
                o_length++;
                chr++;
                ctn++;
            }
        }
        
        piece->set_ = ( '\n' == o_body.last_ );
        
        if ( o_length >= a_length ) {
            break;
        }
        
    }
    
    // ... we're done?
    if ( 1 == o_body.boundary_.set_ && 1 == o_body.content_disposition_.set_ && 1 == o_body.content_type_.set_ && 1 == o_body.terminator_.set_ ) {        
        o_body.done_ = 1;        
    }
    
    return ( 1 == o_body.done_ ? NGX_DONE : NGX_AGAIN );
}


/**
 * @brief Parse an argument as an uri.
 *
 * @param a_r
 * @param a_name
 * @param o_value
 *
 * @return NGX_OK, NGX_ERROR or NGX_DECLINED
 */
ngx_int_t ngx::utls::nrs_ngx_parse_arg_as_uri (ngx_http_request_t* a_r, const char* const a_name,
                                               std::string& o_value)
{
    ngx_int_t rv = NGX_ERROR;
    
    ngx_str_t src;
    src.data = (u_char*)ngx_pnalloc(a_r->pool, a_r->uri.len);
    if ( NULL == src.data ) {
        return NGX_ERROR;
    }
    
    src.len = a_r->uri.len;
    if ( NGX_OK != ( rv = ngx_http_arg(a_r, (u_char*)a_name, strlen(a_name), &src) ) ) {
        return rv;
    }
    
    ngx_str_t dst;
    dst.data = (u_char*)ngx_pnalloc(a_r->pool, src.len);
    if ( NULL == dst.data ) {
        return NGX_ERROR;
    }
    dst.len = src.len;
    memset(dst.data, 0, dst.len);
    
    u_char* dst_sp = dst.data;    
    ngx_unescape_uri(&dst.data, &src.data, src.len, NGX_UNESCAPE_URI);
    dst.len   = dst.data - dst_sp;
    dst.data -= dst.len;
    src.data -= src.len;

    o_value = std::string(reinterpret_cast<const char*>(dst.data), dst.len);
    
    return NGX_OK;
}

/**
 * @brief Parse an argument.
 *
 * @param a_r
 * @param a_name
 * @param o_value
 *
 * @return NGX_OK, NGX_ERROR or NGX_DECLINED
 */
ngx_int_t ngx::utls::nrs_ngx_get_arg (ngx_http_request_t* a_r, ngx_str_t* a_args, const char* const a_name,
                                      std::string& o_value)
{
    u_char  *p, *last;
    
    if ( 0 == a_args->len ) {
        return NGX_DECLINED;
    }
    
    p = a_args->data;
    last = p + a_args->len;
    
    u_char* name = (u_char*)a_name;
    size_t  len  = strlen(a_name);
    
    for ( /* void */ ; p < last; p++) {
        
        /* we need '=' after name, so drop one char from last */
        
        p = ngx_strlcasestrn(p, last - 1, name, len - 1);
        
        if ( NULL == p ) {
            return NGX_DECLINED;
        }
        
        if ( (p == a_args->data || *(p - 1) == '&') && *(p + len) == '=' ) {
            
            ngx_str_t value;
            
            value.data = p + len + 1;
            
            p = ngx_strlchr(p, last, '&');
            
            if (p == NULL) {
                p = a_args->data + a_args->len;
            }
            
            value.len = p - value.data;
            
            o_value = std::string(reinterpret_cast<const char*>(value.data), value.len);
            
            return NGX_OK;
        }
    }
    
    return NGX_DECLINED;
}

/**
 * @brief Parse an argument.
 *
 * @param a_r
 * @param a_args
 * @param o_map
 *
 * @return NGX_OK, NGX_ERROR or NGX_DECLINED
 */
ngx_int_t ngx::utls::nrs_ngx_get_args (ngx_http_request_t* a_r, ngx_str_t* a_args,
                                       std::map<std::string, std::string>& o_map)
{
    if ( 0 == a_args->len ) {
        return NGX_DECLINED;
    }
    
    u_char* p    = a_args->data;
    u_char* last = p + a_args->len;

    ngx_str_t name;
    for ( /* void */ ; p < last; p++) {
        
        /* we need '=' after name, so drop one char from last */
        
        u_char* e = ngx_strlchr(p, last, '=');
        if ( NULL == e ) {
            return NGX_DECLINED;
        }
        
        name.data = p;
        name.len = (e - p);
        
        if ( (p == a_args->data || *(p - 1) == '&') && *(p + name.len) == '=' ) {
            
            ngx_str_t value;
            
            value.data = p + name.len + 1;
            
            p = ngx_strlchr(p, last, '&');
            
            if (p == NULL) {
                p = a_args->data + a_args->len;
            }
            
            value.len = p - value.data;
            
            o_map[std::string(reinterpret_cast<const char*>(name.data), name.len)] = std::string(reinterpret_cast<const char*>(value.data), value.len);
            
        } else {
            p = a_args->data + a_args->len;
        }

    }
    
    return ( p >= last ) ? NGX_OK : NGX_DECLINED;
}

/**
 * @brief Parse an argument.
 *
 * @param a_r
 * @param a_name
 * @param o_uri
 *
 * @return NGX_OK, NGX_ERROR or NGX_DECLINED
 */
ngx_int_t ngx::utls::nrs_ngx_unescape_uri (ngx_http_request_t* a_r, const char* const a_name, std::string& o_uri)
{    
    if ( NULL == a_name || '\0' == a_name[0] ) {
        return NGX_DECLINED;
    }
    
    ngx_str_t src;
    src.data = (u_char*)a_name;
    src.len  = strlen(a_name);
    
    ngx_str_t dst;
    dst.data = (u_char*)ngx_pnalloc(a_r->pool, src.len);
    if ( NULL == dst.data ) {
        return NGX_ERROR;
    }
    dst.len = src.len;
    memset(dst.data, 0, dst.len);
    
    u_char* dst_sp = dst.data;
    ngx_unescape_uri(&dst.data, &src.data, src.len, NGX_UNESCAPE_URI);
    dst.len   = dst.data - dst_sp;
    dst.data -= dst.len;
    src.data -= src.len;
    
    o_uri = std::string(reinterpret_cast<const char*>(dst.data), dst.len);
    
    return NGX_OK;
}

/**
 * @brief Return untouched headers.
 *
 * @param a_list
 * @param o_array Untouched headers as JSON array of strings.
 */
void ngx::utls::nrs_ngx_get_headers (const ngx_list_part_t* a_list, Json::Value& o_array)
{
    o_array = Json::Value(Json::ValueType::arrayValue);
    const ngx_list_part_t* list_part = a_list;
    while ( NULL != list_part ) {
        // ... for each element ...
        ngx_table_elt_t* header = (ngx_table_elt_t*)list_part->elts;
        for ( ngx_uint_t index = 0 ; list_part->nelts > index ; ++index ) {
            // ... if is set ...
            if ( 0 < header[index].key.len && 0 < header[index].value.len ) {
                // ... append to array ...
                o_array.append(std::string(reinterpret_cast<char const*>(header[index].key.data), header[index].key.len) + ": " + std::string(reinterpret_cast<char const*>(header[index].value.data), header[index].value.len));
            }
        }
        // ... next...
        list_part = list_part->next;
    }
}

/**
 * @brief Return incomming untouched headers.
 *
 * @param a_r
 * @param o_array Untouched headers as JSON array of strings.
 */
void ngx::utls::nrs_ngx_get_in_untouched_headers (const ngx_http_request_t* a_r, Json::Value& o_array)
{
    nrs_ngx_get_headers(&a_r->headers_in.headers.part, o_array);
}

/**
 * @brief Return untouched headers.
 *
 * @param a_r
 * @param o_array Untouched headers as JSON array of strings.
 */
void ngx::utls::nrs_ngx_get_out_untouched_headers (const ngx_http_request_t* a_r, Json::Value& o_array)
{
    nrs_ngx_get_headers(&a_r->headers_out.headers.part, o_array);
}
