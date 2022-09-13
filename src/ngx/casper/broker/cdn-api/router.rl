/**
 * @file router.h
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

#ifdef __APPLE__
#pragma mark - CDN API ROUTER
#endif

#include "ngx/casper/broker/cdn-api/router.h"

#include "ev/ngx/includes.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#ifdef __APPLE__
#pragma mark - RAGEL
#endif

%%{
    machine cdn_api;
    alphtype char;
    
    action parse_error
    {
       error_column = int(fpc - input);
       fbreak;
    }
        
    action insert_include
    {
        const size_t l = static_cast<size_t>( p - param_value_start_ptr );
        if ( l > 0 ) {
            o_params["include"].insert(std::string(p - l, l));
        }
    }
    
    action billing_create
    {
        o_type         = "billing";
        route(o_type.c_str()).Process(id, a_method, a_headers, o_params, a_body);
        a_asynchronous = true;
    }
    
    action billing_process
    {
         o_id           = std::to_string(id);
         o_type         = "billing";
         route(o_type.c_str()).Process(id, a_method, a_headers, o_params, a_body);
         a_asynchronous = true;
    }
    
    action sideline_process
    {
        o_type         = "sideline";
        route(o_type.c_str()).Process(std::numeric_limits<uint64_t>::max(), a_method, a_headers, o_params, a_body);
        a_asynchronous = true;
    }
    
    #
    
    res_type  = [a-zA-Z0-9] ((( [a-zA-Z0-9] | [\-_] | "%2D" | "%5F" )))* [a-zA-Z0-9] ;
    inc_path  = ( zlen res_type %insert_include ('.' zlen res_type %insert_include)* );
    inc_param = ( 'include' ( '=' | '%3D' ) %{ param_value_start_ptr = fpc; } inc_path ( ( ',' | '%2C' ) %{ param_value_start_ptr = fpc; } inc_path )* );
    
    uint64  = ( zlen >{ id = 0; } digit+ ${ id *= 10; id += (fc - '0'); } %{} );
    
    params = ( inc_param );
    
    #
    # supported resources / routes
    #
    # - billing -
    billing_new      = ( '/api/billing'         ('?' params )? ) %billing_create;
    billing_existing = ( '/api/billing/' uint64 ('?' params )? ) %billing_process;
    sideline         = ( '/api/sideline') %sideline_process;
    
    main := ( billing_existing | billing_new | sideline ) @err(parse_error);
}%%

%%write data nofinal;
    
#ifdef __APPLE__
#pragma mark - CONSTRUCTOR(S) / DESTRUCTOR
#endif

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref Loggable data reference.
 */
ngx::casper::broker::cdn::api::Router::Router (const ::ev::Loggable::Data& a_loggable_data_ref)
    : loggable_data_ref_(a_loggable_data_ref)
{
   /* empty */
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::api::Router::~Router ()
{
    for ( auto it : routes_ ) {
        delete it.second;
    }
    routes_.clear();
}

#ifdef __APPLE__
#pragma mark - PUBLIC METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Process a request.
 *
 * @param a_method       HTTP method ID.
 * @param a_uri          Request URI.
 * @param a_body         Requet body ( if any )
 * @param a_headers      HTTP headers.
 * @param o_type         Resource type.
 * @param o_id           Resource id.
 * @param o_params       Parsed parameters.
 * @param a_asynchronous True if request will be handled asynchronously, false otherwise.
 */
void ngx::casper::broker::cdn::api::Router::Process (const ngx_uint_t a_method, const std::string& a_uri, const char* const a_body, const std::map<std::string, std::string>& a_headers,
                                                     std::string& o_type, std::string& o_id, std::map<std::string, std::set<std::string>>& o_params, bool& a_asynchronous)
{
    int                   cs;
    const char*           input                 = a_uri.c_str();
    const char*           p                     = input;
    const char*           pe                    = input + a_uri.length();
    const char*           eof                   = pe;
    const char*           param_value_start_ptr = nullptr;
    int                   error_column          = -1;
    uint64_t              id                    = std::numeric_limits<uint64_t>::max();
    
    %% write init;
    %% write exec;
    
    if ( nullptr == factory_ ) {
        throw cdn::InternalServerError(("Unable to process route " + a_uri + " - factory not set!").c_str());
    }
    
    if ( -1 != error_column || p != pe ) {
        int   cnt;
        const char* tail;
        
        if ( p != pe ) {
            error_column = (int)(p - input);
        }
        if ( (pe - p) <= 0 ) {
            cnt  = 0;
            tail = "";
        } else {
            cnt = (int) (pe - p);
            tail = input + error_column;
        }
        throw cdn::BadRequest("Invalid route: %*.*s <~ ??? ~> %*.*s",
                              (int) error_column,
                              (int) error_column,
                              input,
                              cnt,
                              cnt,
                              tail);
    }
    
    (void)cdn_api_error;
    (void)cdn_api_en_main;
    
}
