/**
 * @file types.cc
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

#include "ngx/casper/broker/cdn-common/types.h"

#include "cc/types.h"

/**
 * @brief Parse a X-CASPER HEADER KV.
 *
 * param a_header
 * param a_key
 * param o_value
 *
 * @return NGX_OK or NGX_ERROR
 */
template <typename T>
ngx_int_t ngx::casper::broker::cdn::Header<T>::ParseKVHeader (const std::string& a_header, const std::string& a_key, std::string& o_value)
{
    const std::string key = a_key + ":";
    const char* value_ptr = strcasestr(a_header.c_str(), key.c_str());
    if ( nullptr == value_ptr ) {
        return NGX_ERROR;
    }
    value_ptr += strlen(key.c_str());
    while ( ' ' == value_ptr[0] && ( '\0' != value_ptr[0]) ) {
        value_ptr++;
    }
    if ( '\0' == value_ptr[0] ) {
        return NGX_ERROR;
    }
    o_value = value_ptr;
    return NGX_OK;
}

/**
 * @brief Parse a X-CASPER HEADER KV.
 *
 * param a_header
 * param a_key
 * param o_value
 *
 * @return NGX_OK or NGX_ERROR
 */
template <typename T>
ngx_int_t ngx::casper::broker::cdn::Header<T>::ParseKVHeader (const std::string& a_header, const std::string& a_key, uint64_t& o_value)
{
    const std::string key = a_key + ":";
    const char* value_ptr = strcasestr(a_header.c_str(), key.c_str());
    if ( nullptr == value_ptr ) {
        return NGX_ERROR;
    }
    value_ptr += strlen(key.c_str());
    while ( ' ' == value_ptr[0] && ( '\0' != value_ptr[0]) ) {
        value_ptr++;
    }
    if ( '\0' == value_ptr[0] ) {
        return NGX_ERROR;
    }
    // ... hex number?
    if ( strlen(value_ptr) >= 2 && '0' == value_ptr[0] && ( 'x' == value_ptr[1] || 'X' == value_ptr[1] ) ) {
        int value = 0;
        if ( 'x' == value_ptr[1] && 1 != sscanf(value_ptr, "%x", &value) ) {
            return NGX_ERROR;
        } else if ( value < 0 ) {
            return NGX_ERROR;
        } else {
            o_value = static_cast<uint64_t>(value);
        }
    } else if ( 1 != sscanf(value_ptr, UINT64_FMT, &o_value) ) {
        return NGX_ERROR;
    }
    return NGX_OK;
}


#ifdef __APPLE__

/**
 * @brief Parse a X-CASPER HEADER KV.
 *
 * param a_header
 * param a_key
 * param o_value
 *
 * @return NGX_OK or NGX_ERROR
 */
template <typename T>
ngx_int_t ngx::casper::broker::cdn::Header<T>::ParseKVHeader (const std::string& a_header, const std::string& a_key, size_t& o_value)
{
    const std::string key = a_key + ":";
    const char* value_ptr = strcasestr(a_header.c_str(), key.c_str());
    if ( nullptr == value_ptr ) {
        return NGX_ERROR;
    }
    value_ptr += strlen(key.c_str());
    while ( ' ' == value_ptr[0] && ( '\0' != value_ptr[0]) ) {
        value_ptr++;
    }
    if ( '\0' == value_ptr[0] ) {
        return NGX_ERROR;
    }
    if ( 1 != sscanf(value_ptr, "%zd", &o_value) ) {
        return NGX_ERROR;
    }
    return NGX_OK;
}

#endif

/**
 * @brief Parse a X-CASPER HEADER KV.
 *
 * param a_header
 * param a_key
 * param o_value
 *
 * @return NGX_OK or NGX_ERROR
 */
template <typename T>
ngx_int_t ngx::casper::broker::cdn::Header<T>::ParseKVHeader (const std::string& a_header, const std::string& a_key, bool& o_value)
{
    const std::string key = a_key + ":";
    const char* value_ptr = strcasestr(a_header.c_str(), key.c_str());
    if ( nullptr == value_ptr ) {
        return NGX_ERROR;
    }
    value_ptr += strlen(key.c_str());
    while ( ' ' == value_ptr[0] && ( '\0' != value_ptr[0]) ) {
        value_ptr++;
    }
    if ( '\0' == value_ptr[0] ) {
        return NGX_ERROR;
    }
    o_value = ( 0 == strncasecmp(value_ptr, "true", 4 * sizeof(char)) );
    return NGX_OK;
}
