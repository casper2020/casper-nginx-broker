/**
 * @file access_token.cc
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

#include "ngx/casper/broker/oauth/server/access_token.h"

#include "ev/redis/request.h"
#include "ev/redis/reply.h"

#include "cc/utc_time.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 * @param a_service_id
 * @param a_client_id
 * @param a_client_secret
 * @param a_redirect_uri
 * @param a_code
 * @param a_state
 * @param a_bypass_rfc
 */
ngx::casper::broker::oauth::server::AccessToken::AccessToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                              const std::string& a_service_id,
                                                              const std::string& a_client_id, const std::string& a_client_secret, const std::string& a_redirect_uri,
                                                              const std::string& a_code, const std::string& a_state,
                                                              bool a_bypass_rfc)
: ngx::casper::broker::oauth::server::AbstractToken(a_loggable_data_ref, a_service_id, a_client_id, a_client_secret, a_redirect_uri, a_bypass_rfc),
  code_(a_code), state_(a_state)
{
    ttl_                      = 86400; /* 86400s -> 24h */
    headers_["Cache-Control"] = "no-store";
    headers_["Pragma"]        = "no-cache";
    refresh_token_ttl_        = 5184000; /* 5184000s -> 2 months */
}

/**
 * @brief Destructor
 */
ngx::casper::broker::oauth::server::AccessToken::~AccessToken ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief 4.1.3.  Access Token Request - https://tools.ietf.org/html/rfc6749#page-29
 *
 * @param a_preload_callback
 * @param a_redirect_callback
 * @param a_json_callback
 */
void ngx::casper::broker::oauth::server::AccessToken::AsyncRun (ngx::casper::broker::oauth::server::AccessToken::PreloadCallback a_preload_callback,
                                                                ngx::casper::broker::oauth::server::AccessToken::RedirectCallback a_redirect_callback,
                                                                ngx::casper::broker::oauth::server::AccessToken::JSONCallback a_json_callback,
                                                                ngx::casper::broker::oauth::server::AccessToken::LogCallback a_log_callback)
{
    
    //
    // grant_type=authorization_code
    //
    
    /*
     * 4.1.3.  Access Token Request - https://tools.ietf.org/html/rfc6749#page-29
     *
     * The client makes a request to the token endpoint by sending the
     * following parameters using the "application/x-www-form-urlencoded"
     * format per Appendix B with a character encoding of UTF-8 in the HTTP
     * request entity-body:
     *
     * [REQUIRED]    - grant_type   - Value MUST be set to "authorization_code".
     * [REQUIRED]    - code         - The authorization code received from the authorization server.
     * [REQUIRED_IF] - redirect_uri - if the "redirect_uri" parameter was included in the
     *                                authorization request as described in Section 4.1.1, and their
     *                                values MUST be identical.
     *
     * [REQUIRED_IF] - client_id    - if the client is not authenticating with the
     *                                authorization server as described in Section 3.2.1.
     *
     * NOT IMP:   If the client type is confidential or the client was issued client
     *             credentials (or assigned other authentication requirements), the
     *             client MUST authenticate with the authorization server as described
     *             in Section 3.2.1.
     */
    
    //
    // The authorization server MUST:
    //
    // - require client authentication for confidential clients or for any
    //   client that was issued client credentials (or with other authentication requirements),
    //
    // - authenticate the client if client authentication is included,
    //
    // - ensure that the authorization code was issued to the authenticated
    //   confidential client, or if the client is public, ensure that the
    //   code was issued to "client_id" in the request,
    //
    // - verify that the authorization code is valid, and
    //
    // - ensure that the "redirect_uri" parameter is present if the
    //   "redirect_uri" parameter was included in the initial authorization
    //   request as described in Section 4.1.1, and if included ensure that
    //   their values are identical.
    
    // ... 'code', 'client_id' and 'client_secret' are REQUIRED ...
    if ( 0 == code_.length() || 0 == client_id_.length() || 0 == client_secret_.length()  ) {
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
        
        // ... 'code' exists?
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key    */ ( authorization_code_hash_prefix_ + ':' + code_ ),
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
            throw ev::Exception("Authorization code doesn't exist!");
        }

        //
        // Get ALL 'authorization code' properties
        //
        return new ::ev::redis::Request(loggable_data_ref_, "HGETALL", {
            /* key    */ ( authorization_code_hash_prefix_ + ':' + code_ )
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
        for ( int idx = 0 ; idx < static_cast<int>(value.Size()) ; idx += 2 ) {
            // ...'count' ?
            if ( 0 == value[idx].String().compare("count") ) {
                // ... check 'count' ...
                const ev::redis::Value& count_value = value[idx+1];
                if ( false == count_value.IsInteger() || 1 != count_value.Integer() ) {
                    if ( false == count_value.IsString() || 1 != std::atoll(count_value.String().c_str()) ) {
                        body_["error"] = "unauthorized_client";
                        throw ev::Exception("Authorization code already used!");
                    }
                }
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare(k_client_scope_field_) ) {
                // ... check 'scope' ...
                const ev::redis::Value& scope_value = value[idx+1];
                if ( false == scope_value.IsNil() ) {
                    if ( false == scope_value.IsString() ) {
                        // already set: body_["error"] = "server_error";
                        throw ev::Exception("Invalid scope type!");
                    } else {
                        scope_ = scope_value.String();
                    }
                }
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare(k_client_redirect_uri_field_) ) {
                // ... validate redirect url ...
                const std::string& acceptable_redirect_uri = value[idx+1].String();
                if ( redirect_uri_.length() > 0 ) {
                    if ( not (
                              redirect_uri_.length() >= acceptable_redirect_uri.length()
                              &&
                              0 == strncasecmp(acceptable_redirect_uri.c_str(), redirect_uri_.c_str(), acceptable_redirect_uri.length())
                              )
                        ) {
                        body_["error"] = "invalid_request";
                        throw ev::Exception("Redirect URI mismatch!");
                    }
                }
            } else if ( 0 == value[idx].String().compare("issuer") || 0 == value[idx].String().compare("created_at") ) {
                ignored_idxs.insert(idx);
            }
        }
        
        if ( 4 != ignored_idxs.size() ) {
            // already set: body_["error"] = "server_error";
            throw ev::Exception("Unable to confirm the following values: 'count', 'scope', 'issuer' and / or 'created_at'!");
        }
        
        /* copy 'template' values */
        for ( int idx = 0 ; idx < static_cast<int>(value.Size()) ; idx += 2 ) {
            if ( ignored_idxs.end() != ignored_idxs.find(idx) ) {
                continue;
            }
            args_.push_back(value[idx].String());
            args_.push_back(value[idx + 1].IsNil() ? "" : value[idx + 1].String());
        }
        args_.push_back(k_client_scope_field_); args_.push_back(scope_);
        args_.push_back("issuer")             ; args_.push_back("nginx-broker");
        args_.push_back("created_at")         ; args_.push_back(cc::UTCTime::NowISO8601WithTZ());
        
        //
        // GENERATE NEW ACCESS AND REFRESH TOKENS
        //
        GenNewToken(/* a_name*/ "access" , /* a_error_code*/ "unauthorized_client" , /* o_value */ access_token_);
        GenNewToken(/* a_name*/ "refresh", /* a_error_code*/ "unauthorized_client" , /* o_value */ refresh_token_);
        
        //
        // Get settings
        //
        return new ::ev::redis::Request(loggable_data_ref_, "HMGET", {
            /* key    */ client_key_,
            /* field  */ k_access_token_ttl_field_,
            /* field  */ k_refresh_token_ttl_field_,
            /* field  */ k_client_secret_field_,
        });
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
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
        const ev::redis::Value& refresh_token_ttl_value = value[1];
        if ( true == refresh_token_ttl_value.IsString() && refresh_token_ttl_value.String().length() > 0 ) {
            refresh_token_ttl_ = static_cast<int64_t>(std::atoll(refresh_token_ttl_value.String().c_str()));
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
            /* key */ ( access_token_hash_prefix_ + ':' + access_token_ )
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
        if ( 0 != value.Integer() ) {
            // already set: body_["error"] = "server_error";
            throw ev::Exception("Trying to insert a duplicated access token!");
        }
        
        //
        // Check if 'refresh' token already exists
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key */ ( refresh_token_hash_prefix_ + ':' + refresh_token_ )
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
        if ( 0 != value.Integer() ) {
            // already set: body_["error"] = "server_error";
            throw ev::Exception("Trying to insert a duplicated refresh token!");
        }
        
        //
        // Set 'access token' properties...
        //
        
        args_.insert(args_.begin(), ( access_token_hash_prefix_ + ':' + access_token_ ));
        args_.push_back("refresh_token") ; args_.push_back(refresh_token_);

        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", args_);
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
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
            /* key */ ( access_token_hash_prefix_ + ':' + access_token_ ),
            /* ttl */ std::to_string(ttl_)
        });
        
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
        
        //
        // SET 'refresh token' properties...
        //
        args_.insert(args_.begin(), ( refresh_token_hash_prefix_ + ':' + refresh_token_ ));

        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", args_);
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HMSET:
        //
        // - Status Reply is expected
        //
        //  - 'OK'
        //
        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");
        
        args_.erase(args_.begin());
        
        std::vector<std::string> ac_update_args;
        ac_update_args.insert(ac_update_args.begin(), ( authorization_code_hash_prefix_ + ':' + code_ ));
        ac_update_args.push_back("count"); ac_update_args.push_back(std::to_string(2));
        
        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", ac_update_args);
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HMSET:
        //
        // - Status Reply is expected
        //
        //  - 'OK'
        //
        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");

        //
        // EXPIRE code.
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXPIRE", {
            /* key */ ( refresh_token_hash_prefix_ + ":" + refresh_token_ ),
            /* ttl */ std::to_string(refresh_token_ttl_)
        });
        
    })->Finally([this, a_redirect_callback, a_json_callback] (::ev::Object* a_object) {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
        
        /*
         * 4.1.4.  Access Token Response
         *
         *  + If the access token request is valid and authorized, 
         *    the authorization server issues an access token
         *    and
         *    optional refresh token as described in Section 5.1.
         *
         * + If the request client authentication failed or is invalid,
         *   the authorization server returns error response as described in Section 5.2.
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
         *   "expires_in":<ttl>,
         *   "refresh_token":<refresh_token>
         * }
         */
        body_.removeMember("error");
        
        // ... not following rfc, send a JSON response ...
        body_["access_token"] = access_token_;
        body_["token_type"]   = token_type_;
        if ( 0 != state_.length() ) {
            body_["state"] = state_;
        }
        body_["expires_in"] = ttl_;
        if ( 0 != refresh_token_.length() ) {
            body_["refresh_token"] = refresh_token_;
        }
        
        a_json_callback(200 /* NGX_HTTP_OK */, headers_, body_);
        
    })->Catch([this, a_json_callback, a_log_callback] (const ::ev::Exception& a_ev_exception) {
        
        SetAndCallStandardJSONErrorResponse(body_["error"].asString(), /* a_error_description */ "", a_json_callback, a_log_callback, a_ev_exception);
        
    });
    
}
