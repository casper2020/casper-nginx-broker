/**
 * @file types.h
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

#ifndef NRS_NGX_CASPER_BROKER_TYPES_H_
#define NRS_NGX_CASPER_BROKER_TYPES_H_

#include "unicode/char16ptr.h"
#include "unicode/utypes.h"
#include "unicode/msgfmt.h"

#include "json/json.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            typedef struct _ErrorsTracker {
                std::function<void(const char* const a_error_code, const uint16_t a_http_status_code,
                                   const char* const a_i18n_key)>                                       add_i18n_;
                
                std::function<void(const char* const a_error_code, const uint16_t a_http_status_code,
                                   const char* const a_i18n_key,
                                   U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count)>          add_i18n_v_;

                std::function<void(const char* const a_error_code, const uint16_t a_http_status_code,
                                   const char* const a_i18n_key,
                                   U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count,
                                   const char* const a_internal_error_msg)>                             add_i18n_vi_;
                
                std::function<void(const char* const a_error_code, const uint16_t a_http_status_code,
                                   const char* const a_i18n_key,
                                   const char* const a_internal_error_msg)>                             add_i18n_i_;
                
                std::function<void(Json::Value& o_object)>                                              i18n_jsonify_;
                std::function<void()>                                                                   i18n_reset_;
            } ErrorsTracker;
            
        } // end of namespace broker
        
    } // end of namespace casper
    
} // end of namespace 'ngx'


#endif // NRS_NGX_CASPER_BROKER_TYPES_H_
