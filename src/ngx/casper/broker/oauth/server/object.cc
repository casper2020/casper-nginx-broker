/**
 * @file object.cc
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

#include "ngx/casper/broker/oauth/server/object.h"

#include <algorithm>
#include <random>
#include <ctime>
#include <openssl/rand.h>

#include "ngx/casper/broker/exception.h"

const std::string ngx::casper::broker::oauth::server::Object::k_client_secret_field_       = "secret";
const std::string ngx::casper::broker::oauth::server::Object::k_client_redirect_uri_field_ = "redirect_uri";
const std::string ngx::casper::broker::oauth::server::Object::k_client_scope_field_        = "scope";

/**
 * @brief Default constructor.
 */
ngx::casper::broker::oauth::server::Object::Object (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                    const std::string& a_service_id,
                                                    const std::string& a_client_id, const std::string& a_redirect_uri,
                                                    bool a_bypass_rfc)
    : loggable_data_ref_(a_loggable_data_ref),
      service_id_(a_service_id),
      client_id_(a_client_id), redirect_uri_(a_redirect_uri),
      bypass_rfc_(a_bypass_rfc),
//
// @REDIS:
//
// HMSET <service>:oauth:<client_id> secret <secret> redirect_uri <uri> authorization_code_ttl <ac_ttl> access_token_ttl <at_ttl> refresh_token_ttl <rt_ttl>
// [HASH] <service>:oauth:<client_id>
// <service>:oauth:<client_id>:secret                          <string>
// <service>:oauth:<client_id>:redirect_uri                    <uri>
// <service>:oauth:<client_id>:access_token_ttl                <seconds>
// <service>:oauth:<client_id>:refresh_token_ttl               <seconds>
// <service>:oauth:<client_id>:authorization_code_ttl          <seconds>
// <service>:oauth:<client_id>:scopes                          <string>
// [HASH]<service>:oauth:authorization_code:<code>             <string>
// <service>:oauth:authorization_code:<code>:<count>           <integer>
// <service>:oauth:authorization_code:<code>:<issuer>          <string>
// <service>:oauth:authorization_code:<code>:<created_at>      <string>
// [HASH]<service>:oauth:access_token:<token>                  <string>
// <service>:oauth:access_token:<token>:<issuer>               <string>
// <service>:oauth:access_token:<token>:<created_at>           <string>
// [HASH]<service>:oauth:refresh_token:<token>                 <string>
// <service>:oauth:refresh_token:<token>:<issuer>              <string>
// <service>:oauth:refresh_token:<token>:<created_at>          <string>
//
    client_key_                    ( service_id_ + ":oauth:" + client_id_           ),
    authorization_code_hash_prefix_( service_id_ + ":oauth:" + "authorization_code" ),
    access_token_hash_prefix_      ( service_id_ + ":oauth:" + "access_token"       ),
    refresh_token_hash_prefix_     ( service_id_ + ":oauth:" + "refresh_token"      )
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
    ttl_ = 0;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::oauth::server::Object::~Object ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

#ifdef __APPLE__
#pragma mark -
#endif


/**
 * @brief Generate a ramdom string.
 *
 * @param a_length
 */
std::string ngx::casper::broker::oauth::server::Object::RandomString (std::size_t a_length)
{

    int len = static_cast<int>(a_length) / 2;
    u_char* random_bytes = new u_char[a_length];
    if ( RAND_bytes(random_bytes, static_cast<int>(a_length)) != 1 ) {
        delete [] random_bytes;
        throw ngx::casper::broker::Exception("BROKER_INTERNAL_ERROR",
                                             "Unable to generate a random string!"
        );
    }
    
    static u_char  hex[] = "0123456789abcdef";
    
    std::string string;
    while (len--) {
        string += hex[*random_bytes >> 4];
        string += hex[*random_bytes++ & 0xf];
    }
    random_bytes -= (static_cast<int>(a_length) / 2);
    delete [] random_bytes;
    
    return string;
}

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ngx::casper::broker::oauth::server::Object::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}
