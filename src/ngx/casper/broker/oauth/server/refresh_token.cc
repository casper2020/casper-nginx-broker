/**
 * @file refresh_token.cc
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

#include "ngx/casper/broker/oauth/server/refresh_token.h"

#include "ev/redis/request.h"
#include "ev/redis/reply.h"

#include "cc/utc_time.h"

#include "ev/logger.h"


#include "ngx/casper/broker/oauth/server/scope.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 * @param a_service_id
 * @param a_client_id
 * @param a_client_secret
 * @param a_refresh_token
 * @param a_scope
 */
ngx::casper::broker::oauth::server::RefreshToken::RefreshToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                                const std::string& a_service_id,
                                                                const std::string& a_client_id, const std::string& a_client_secret,
                                                                const std::string& a_refresh_token, const std::string& a_scope,
                                                                bool a_bypass_rfc)
: ngx::casper::broker::oauth::server::AbstractToken(a_loggable_data_ref, a_service_id, a_client_id, a_client_secret, /* a_redirect_uri */ "", a_bypass_rfc),
  refresh_token_(a_refresh_token), scope_(a_scope)
{
    ttl_                        = 5184000; /* 5184000s -> 2 months */
    headers_["Cache-Control"]   = "no-store";
    headers_["Pragma"]          = "no-cache";
    new_refresh_token_ttl_      = 5184000; /* 5184000s -> 2 months */
    generate_new_refresh_token_ = false;
}

/**
 * @brief Destructor
 */
ngx::casper::broker::oauth::server::RefreshToken::~RefreshToken ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief 6.  Refreshing an Access Token - https://tools.ietf.org/html/rfc6749#section-6
 *
 * @param a_preload_callback
 * @param a_redirect_callback
 * @param a_json_callback
 */
void ngx::casper::broker::oauth::server::RefreshToken::AsyncRun (ngx::casper::broker::oauth::server::RefreshToken::PreloadCallback a_preload_callback,
                                                                 ngx::casper::broker::oauth::server::RefreshToken::RedirectCallback a_redirect_callback,
                                                                 ngx::casper::broker::oauth::server::RefreshToken::JSONCallback a_json_callback)
{
    
    /**
     * 6.  Refreshing an Access Token
     *
     *
     * If the authorization server issued a refresh token to the client, the
     * client makes a refresh request to the token endpoint by adding the
     *  following parameters using the "application/x-www-form-urlencoded"
     * format per Appendix B with a character encoding of UTF-8 in the HTTP
     * request entity-body:
     *
     * [REQUIRED] - grant_type    - Value MUST be set to "refresh_token".
     * [REQUIRED] - refresh_token - refresh token issued to the client.
     * [OPTIONAL] - scope         -  The scope of the access request as described by Section 3.3.
     *                               The requested scope MUST NOT include any scope not originally granted by the resource owner, 
     *                               and if omitted is treated as equal to the scope originally granted by the resource owner.
     *
     *
     * Because refresh tokens are typically long-lasting credentials used to request additional access tokens, 
     * the refresh token is bound to the client to which it was issued.
     * If the client type is confidential or the client was issued client credentials (or assigned other authentication requirements), 
     * the client MUST authenticate with the authorization server as described in Section 3.2.1.
     *
     * Example:
     *
     * POST /token HTTP/1.1
     * Host: server.example.com
     * Authorization: Basic <b64>
     * Content-Type: application/x-www-form-urlencoded
     *
     * grant_type=refresh_token&refresh_token=<refresh_token>
     * ...
     *
     */
    
    // ... 'refresh_token' is REQUIRED ...
    if ( 0 == refresh_token_.length() ) {
        // ... report error we're done ...
        body_["error"] = "invalid_request";
        a_json_callback(400 /* NGX_HTTP_BAD_REQUEST */, headers_, body_);
        // ... and we're done ...
        return;
    }
    
    // ... assuming internal server error ...
    body_["error"] = "server_error";
    
    // ... notify that request will be run in background ...
    a_preload_callback();
    
    // ... generate code ...
    const std::string access_token = RandomString(64);
    
    // ... run REDIS tasks ...
    NewTask([this] () -> ::ev::Object* {
        
        // ... client exists?
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            client_key_,
        });
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - n key(s) exist
        //  - 0 the key(s) do(es) not exist
        //
        
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 1 != value.Integer() ) {
            body_["error"] = "unauthorized_client";
            throw ev::Exception("Client doesn't exist!");
        }
        
        // ... 'refresh_token' exists?
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key    */ ( refresh_token_hash_prefix_ + ':' + refresh_token_ )
        });
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {

        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - n key(s) exist
        //  - 0 the key(s) do(es) not exist
        //
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 1 != value.Integer() ) {
            body_["error"] = "unauthorized_client";
            throw ev::Exception("Refresh token doesn't exist!");
        }
        
        //
        // Get 'refresh code' properties
        //
        return new ::ev::redis::Request(loggable_data_ref_, "HGETALL", {
            /* key    */ ( refresh_token_hash_prefix_ + ':' + refresh_token_ )
        });
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {

        //
        // HGETALL:
        //
        // - Array reply is expected:
        //
        //  - list of fields and their values stored in the hash, or an empty list when key does not exist.
        //
        const ev::redis::Value& value = ev::redis::Reply::EnsureArrayReply(a_object);
        
        // ... search for 'count' and 'scope' values ...
        std::set<int> ignored_idxs;
        for ( Json::ArrayIndex idx = 0 ; idx < value.Size() ; idx += 2 ) {
             if ( 0 == value[idx].String().compare(k_client_scope_field_) ) {
                 // ... validate and 'grant' scope(s) ...
                 ngx::casper::broker::oauth::server::Scope scope(value[idx+1]);
                 // The requested scope MUST NOT include any scope not originally granted by the resource owner...
                 if ( scope_.length() > 0 ) {
                     // .. grant the requested scope ...
                     granted_scope_ = scope.Grant(scope_);
                     if ( 0 == granted_scope_.length() ) {
                         body_["error"] = "invalid_scope";
                         throw ev::Exception("Invalid scope(s): '%s'!", scope_.c_str());
                     }
                 } else {
                     // ... and if omitted is treated as equal to the scope originally granted by the resource owner.
                     granted_scope_ = scope.Default();
                     if ( 0 == granted_scope_.length() ) {
                         body_["error"] = "server_error";
                         throw ev::Exception("Server has no scope(s) set for this client: '%s'!", client_id_.c_str());
                     }
                 }
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare("issuer") || 0 == value[idx].String().compare("created_at") ) {
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare("on_refresh_issue_new_pair") ) {
                generate_new_refresh_token_ = ( 0 == value[idx+1].String().compare("true") );
            }
        }
        
        if ( 3 != ignored_idxs.size() ) {
            // alread set: body_["error"] = "server_error";
            throw ev::Exception("Unable to confirm the follwing values: 'count', 'scope', 'issuer' and / or 'created_at'!");
        }
        
        /* copy 'template' values */
        for ( Json::ArrayIndex idx = 0 ; idx < value.Size() ; idx += 2 ) {
            if ( ignored_idxs.end() != ignored_idxs.find(idx) ) {
                continue;
            }
            args_.push_back(value[idx].String());
            args_.push_back(value[idx + 1].IsNil() ? "" : value[idx + 1].String());
        }
        args_.push_back(k_client_scope_field_); args_.push_back(granted_scope_);
        args_.push_back("issuer")             ; args_.push_back("nginx-broker");
        args_.push_back("created_at")         ; args_.push_back(cc::UTCTime::NowISO8601WithTZ());
        
        //
        // Get settings
        //
        return new ::ev::redis::Request(loggable_data_ref_, "HMGET", {
            /* key    */ client_key_,
            /* field  */ k_access_token_ttl_field_,
            /* field  */ k_refresh_token_ttl_field_,
            /* field  */ k_client_secret_field_
        });
        
    })->Then([this, access_token] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HMGET:
        //
        // - Array reply is expected:
        //
        //  - list of values associated with the given fields, in the same order as they are requested.
        //
        const ev::redis::Value& value = ev::redis::Reply::EnsureArrayReply(a_object, 3);
        
        // ... set 'access' token ttl ...
        const ev::redis::Value& access_token_ttl_value = value[0];
        if ( true == access_token_ttl_value.IsString() && access_token_ttl_value.String().length() > 0 ) {
            ttl_ = static_cast<int64_t>(std::atoll(access_token_ttl_value.String().c_str()));
        }
        
        // ... set 'refresh' token ttl ...
        const ev::redis::Value& new_refresh_token_ttl_value = value[1];
        if ( true == new_refresh_token_ttl_value.IsString() && new_refresh_token_ttl_value.String().length() > 0 ) {
            new_refresh_token_ttl_ = static_cast<int64_t>(std::atoll(new_refresh_token_ttl_value.String().c_str()));
        }
        
        // ... compare 'client_secret' ...
        const ev::redis::Value& client_secret_value = value[2];
        if ( false == client_secret_value.IsString() || 0 == client_secret_value.String().length() ) {
            // already set: body_["error"] = "server_error";
            throw ev::Exception("Invalid client configuration - missing client secret field!");
        }
        if ( 0 != client_secret_value.String().compare(client_secret_) ) {
            body_["error"] = "access_denied";
            throw ev::Exception("Invalid credentials!");
        }
        
        //
        // Check if 'access' token already exists
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key */ ( access_token_hash_prefix_ + ':' + access_token )
        });
        
    })->Then([this, access_token] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - n key(s) exist
        //  - 0 the key(s) do(es) not exist
        //
        
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 0 != value.Integer() ) {
            // alread set: body_["error"] = "server_error";
            throw ev::Exception("Trying to insert a duplicated access token!");
        }
        
        //
        // Set 'access token' properties....
        //
        args_.insert(args_.begin(), ( access_token_hash_prefix_ + ':' + access_token ));
        if ( true == generate_new_refresh_token_ ) {
            // ... generate new refresh token ...
            new_refresh_token_ = RandomString(64);
            args_.push_back("refresh_token") ; args_.push_back(new_refresh_token_);
        } else {
            args_.push_back("refresh_token") ; args_.push_back(refresh_token_);
        }
        
        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", args_);
        
    })->Then([this, access_token] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HMSET:
        //
        // - Status Reply is expected
        //
        //  - 'OK'
        //
        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");
        
        args_.erase(args_.begin());
        args_.erase(args_.end()-1);
        args_.erase(args_.end()-1);
        
        //
        // EXPIRE code.
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXPIRE", {
            /* key */ ( access_token_hash_prefix_ + ':' + access_token ),
            /* ttl */ std::to_string(ttl_)
        });
        
    })->Then([this, access_token] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
        
        // ... if a new refresh is not required ...
        if ( false == generate_new_refresh_token_ ) {
            // ... nothing else to do ...
            return nullptr;
        }
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);

        // ... 'new_refresh_token_' exists?
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key    */ ( refresh_token_hash_prefix_ + ':' + new_refresh_token_ )
        });
        
    })->Then([this, access_token] (::ev::Object* a_object) -> ::ev::Object* {
        
        // ... if a new refresh is not required ...
        if ( false == generate_new_refresh_token_ ) {
            // ... nothing else to do ...
            return nullptr;
        }
        
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - n key(s) exist
        //  - 0 the key(s) do(es) not exist
        //
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 0 != value.Integer() ) {
            // alread set: body_["error"] = "server_error";
            throw ev::Exception("Trying to insert a duplicated refresh token!");
        }
        
        //
        // SET new 'refresh token' properties...
        //
        args_.insert(args_.begin(), ( refresh_token_hash_prefix_ + ':' + new_refresh_token_ ));
        
        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", args_);
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        // ... if a new refresh is not required ...
        if ( false == generate_new_refresh_token_ ) {
            // ... nothing else to do ...
            return nullptr;
        }
        
        //
        // HMSET:
        //
        // - Status Reply is expected
        //
        //  - 'OK'
        //
        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");
        
        args_.erase(args_.begin());

        //
        // EXPIRE code.
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXPIRE", {
            /* key */ ( refresh_token_hash_prefix_ + ":" + new_refresh_token_ ),
            /* ttl */ std::to_string(new_refresh_token_ttl_)
        });
        
    })->Finally([this, access_token, a_redirect_callback, a_json_callback] (::ev::Object* /* a_object */) {
        
        /*
         * 5.1.  Successful Response
         *
         * ...
         *
         * Example:
         *
         * HTTP/1.1 200 OK
         * Content-Type: application/json;charset=UTF-8
         * Cache-Control: no-store
         * Pragma: no-cache
         *
         * {
         *   "access_token":<access_token>,
         *   "token_type":<token_type>,
         *   "expires_in":<ttl>
         * }
         */
        body_.removeMember("error");
        
        // ... not following rfc, send a JSON response ...
        body_["access_token"] = access_token;
        if ( true == generate_new_refresh_token_ ) {
            body_["refresh_token"] = new_refresh_token_;
        }
        body_["token_type"] = token_type_;
        body_["expires_in"] = ttl_;
                
        a_json_callback(200 /* NGX_HTTP_OK */, headers_, body_);
        
    })->Catch([this, a_json_callback] (const ::ev::Exception& /* a_ev_exception */) {
        
        SetAndCallStandardJSONErrorResponse(body_["error"].asString(), a_json_callback);
        
    });
}
