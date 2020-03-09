/**
 * @file authorization_code.cc
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

#include "ngx/casper/broker/oauth/server/authorization_code.h"

#include "ev/redis/request.h"
#include "ev/redis/reply.h"

#include "cc/utc_time.h"

#include "ngx/casper/broker/oauth/server/scope.h"

const std::string ngx::casper::broker::oauth::server::AuthorizationCode::k_authorization_code_ttl_field_ = "authorization_code_ttl";

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 * @param a_service_id
 * @param a_client_id
 * @param a_scope
 * @param a_state
 * @param a_redirect_uri
 */
ngx::casper::broker::oauth::server::AuthorizationCode::AuthorizationCode (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                                          const std::string& a_service_id,
                                                                          const std::string& a_client_id, const std::string& a_scope, const std::string& a_state,
                                                                          const std::string& a_redirect_uri, bool a_bypass_rfc)
    : ngx::casper::broker::oauth::server::Object(a_loggable_data_ref, a_service_id, a_client_id, a_redirect_uri, a_bypass_rfc),
      scope_(a_scope), state_(a_state)
{
    ttl_                      = 600; // 600sec -> 10min
    headers_["Cache-Control"] = "no-store";
    headers_["Pragma"]        = "no-cache";
    body_                     = Json::Value(Json::ValueType::objectValue);
}

/**
 * @brief Destructor
 */
ngx::casper::broker::oauth::server::AuthorizationCode::~AuthorizationCode ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief 4.1.1.  Authorization Request - https://tools.ietf.org/html/rfc6749#section-4.1.1
 *
 * @param a_preload_callback
 * @param a_redirect_callback
 * @param a_json_callback
 */
void ngx::casper::broker::oauth::server::AuthorizationCode::AsyncRun (ngx::casper::broker::oauth::server::AuthorizationCode::PreloadCallback a_preload_callback,
                                                                      ngx::casper::broker::oauth::server::AuthorizationCode::RedirectCallback a_redirect_callback,
                                                                      ngx::casper::broker::oauth::server::AuthorizationCode::JSONCallback a_json_callback)
{
    /*
     * 4.1.1.  Authorization Request
     *
     * The client constructs the request URI by adding the following
     * parameters to the query component of the authorization endpoint URI
     * using the "application/x-www-form-urlencoded" format, per Appendix B:
     *
     * [REQUIRED]    - response_type - Value MUST be set to "code".
     * [REQUIRED]    - client_id     - The client identifier as described in Section 2.2.
     * [OPTIONAL]    - redirect_uri  - described in Section 3.1.2.
     * [OPTIONAL]    - scope         - The scope of the access request as described by Section 3.3.
     * [RECOMMENDED] - state         - opaque value used by the client to maintain state between the request and callback.
     *                                 The authorization server includes this value when redirecting the user-agent back to the client.
     *                                 The parameter SHOULD be used for preventing cross-site request forgery as described in Section 10.12.
     *
     * The authorization server validates the request to ensure that all required parameters are present and valid.
     *
     * If the request is valid, the authorization server authenticates the resource owner and obtains an authorization decision
     * (by asking the resource owner or by establishing approval via other means).
     *
     * When a decision is established, the authorization server directs the user-agent to the provided client redirection URI using an HTTP
     * redirection response, or by other means available to it via the user-agent.
     */
    
    // ... 'client_id' and 'redirect_uri' are REQUIRED ...
    if ( 0 == client_id_.length() || 0 == redirect_uri_.length() ) {
        // ... report error we're done ...
        body_["error"] = "invalid_request";
        a_json_callback(400 /* NGX_HTTP_BAD_REQUEST */, headers_, body_);
        // ... and we're done ...
        return;
    }

    // ... assuming internal server error ...
    body_["error"] = "server_error";

    args_.clear();

    // ... notify that request will be run in background ...
    a_preload_callback();
    
    // ... generate code ...
    const std::string code = RandomString(64);
    
    // ... run REDIS tasks ...
    NewTask([this] () -> ::ev::Object* {
        
        // ... client exists?
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
             /* key */ client_key_
        });
        
    })->Then([this, code] (::ev::Object* a_object) -> ::ev::Object* {
        
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
        
        //
        // Get settings
        //
        return new ::ev::redis::Request(loggable_data_ref_, "HGETALL", {
            /* key    */ client_key_
        });
        
    })->Then([this, code] (::ev::Object* a_object) -> ::ev::Object* {
        
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
            if ( 0 == value[idx].String().compare(k_client_redirect_uri_field_) ) {
                // ... validate and 'redirect_uri' ...
                const std::string& acceptable_redirect_uri = value[idx+1].String();
                if ( not (
                          redirect_uri_.length() >= acceptable_redirect_uri.length()
                          &&
                          0 == strncasecmp(acceptable_redirect_uri.c_str(), redirect_uri_.c_str(), acceptable_redirect_uri.length())
                          )
                    ) {
                    body_["error"] = "invalid_request";
                    throw ev::Exception("Redirect URI mismatch!");
                }
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare(k_client_scope_field_) ) {
                // ... validate and 'grant' scope(s) ...
                ngx::casper::broker::oauth::server::Scope scope(value[idx+1]);
                granted_scope_ = scope.Grant(scope_);
                if ( 0 == granted_scope_.length() ) {
                    body_["error"] = "invalid_scope";
                    throw ev::Exception("Invalid scope(s): '%s'!", scope_.c_str());
                }
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare(k_authorization_code_ttl_field_) ) {
                // ... set ttl ...
                const ev::redis::Value& ttl_value = value[idx+1];
                if ( true == ttl_value.IsString() && ttl_value.String().length() > 0 ) {
                    ttl_ = static_cast<int64_t>(std::atoll(ttl_value.String().c_str()));
                }
                ignored_idxs.insert(idx);
            } else if (  0 == value[idx].String().compare("secret") || 0 == value[idx].String().compare("access_token_ttl") || 0 == value[idx].String().compare("refresh_token_ttl") ) {
                ignored_idxs.insert(idx);
            } else if ( 0 == value[idx].String().compare("issuer") || 0 == value[idx].String().compare("created_at") ) {
                ignored_idxs.insert(idx);
            }
        }
        
        if ( ignored_idxs.size() < 6 ) {
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
        
        //
        // Check if 'code' already exists
        //
        return new ::ev::redis::Request(loggable_data_ref_, "EXISTS", {
            /* key */ ( authorization_code_hash_prefix_ + ':' + code )
        });
        
    })->Then([this, code] (::ev::Object* a_object) -> ::ev::Object* {
        
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
            throw ev::Exception("Trying to insert a duplicated authorization code!");
        }
        
        //
        // Add new code
        //
        
        args_.insert(args_.begin(), ( authorization_code_hash_prefix_ + ':' + code ));
        args_.push_back(k_client_redirect_uri_field_) ; args_.push_back(redirect_uri_);
        args_.push_back(k_client_scope_field_)        ; args_.push_back(granted_scope_);
        args_.push_back("count")                      ; args_.push_back(std::to_string(1));
        args_.push_back("issuer")                     ; args_.push_back("nginx-broker");
        args_.push_back("created_at")                 ; args_.push_back(cc::UTCTime::NowISO8601WithTZ());
        
        return new ::ev::redis::Request(loggable_data_ref_, "HMSET", args_);
   
    })->Then([this, code] (::ev::Object* a_object) -> ::ev::Object* {
        
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
            /* key    */ ( authorization_code_hash_prefix_ + ':' + code ),
            /* value  */ std::to_string(ttl_)
        });
        
    
    })->Finally([this, code, a_redirect_callback, a_json_callback] (::ev::Object* a_object) {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
        
        /*
         *
         * 4.1.2.  Authorization Response
         *
         * If the resource owner grants the access request, the authorization server issues an authorization code and delivers it to the client by
         * adding the following parameters to the query component of the redirection URI using the "application/x-www-form-urlencoded" format, per Appendix B:
         *
         * [REQUIRED]    - code -  The authorization code generated by the authorization server.
         *
         *                         The authorization code MUST expire shortly after it is issued to mitigate the risk of leaks.
         *                         A maximum authorization code lifetime of 10 minutes is RECOMMENDED.
         *
         *                         The client MUST NOT use the authorization code more than once.
         *                         If an authorization code is used more than once, the authorization server MUST deny the request
         *                         and SHOULD revoke (when possible) all tokens previously issued based on that authorization code.
         *                         The authorization code is bound to  the client identifier and redirection URI.
         *
         *                         client_id, redirect_uri <-> code
         *
         * [REQUIRED_IF] - state - if the "state" parameter was present in the client authorization request.
         *                         The exact value received from the client.
         *
         *
         *   For example, the authorization server redirects the user-agent by
         *   sending the following HTTP response:
         *
         *   HTTP/1.1 302 Found
         *   Location: https://client.example.com/cb?code=SplxlOBeZQQYbYS6WxSbIA&state=xyz
         *
         *
         *   The client MUST ignore unrecognized response parameters.
         *   The authorization code string size is left undefined by this specification.
         *   The client should avoid making assumptions about code value sizes.
         *   The authorization server SHOULD document the size of any value it issues.
         *
         */

        body_.removeMember("error");

        // ... notify ...
        if ( false ==  bypass_rfc_ ) {
            // ... following rfc ...
            std::string uri = redirect_uri_;
            if ( nullptr != strchr(uri.c_str(), '?') ) {
                uri += "&";
            } else {
                uri += "?";
            }
            uri += "code=" + code;
            if ( 0 != state_.length() ) {
                uri += "&state=" + state_;
            }
            a_redirect_callback(uri);
        } else {
            // ... not following rfc, send a JSON response ...
            body_["code"] = code;
            if ( 0 != state_.length() ) {
                body_["state"] = state_;
            }
            a_json_callback(200 /* NGX_HTTP_OK */, headers_, body_);
        }
        
    })->Catch([this, a_json_callback, a_redirect_callback] (const ::ev::Exception& a_ev_exception) {
        
        OSAL_DEBUG_STD_EXCEPTION(a_ev_exception);
        
        /*
         * 4.1.2.1.  Error Response
         *
         * If the request fails due to a missing, invalid, or mismatching redirection URI,
         *   or
         * if the client identifier is missing or invalid, the authorization server SHOULD inform the resource owner of the error
         * and MUST NOT automatically redirect the user-agent to the invalid redirection URI.
         *
         *
         *  If the resource owner denies the access request or if the request fails for reasons other than a missing or invalid redirection URI,
         *  the authorization server informs the client by adding the following parameters to the query component of the redirection URI using the
         * "application/x-www-form-urlencoded" format, per Appendix B:
         *
         * [REQUIRED]    - error             - invalid_request | unauthorized_client | access_denied | unsupported_response_type | invalid_scope | server_error | temporarily_unavailable
         * [OPTIONAL]    - error_description -
         * [OPTIONAL]    - error_uri         -
         * [REQUIRED_IF] - state             -
         *
         * For example, the authorization server redirects the user-agent by sending the following HTTP response:
         *
         * HTTP/1.1 302 Found
         * Location: https://client.example.com/cb?error=access_denied&state=xyz
         *
         * invalid_request           - The request is missing a required parameter,
         *                             includes an invalid parameter value
         *                             includes a parameter more than once,
         *                             or is otherwise malformed.
         *
         * unauthorized_client       - The client is not authorized to request an authorization code using this method.
         *
         * access_denied             - The resource owner or authorization server denied the request.
         *
         * unsupported_response_type - The authorization server does not support obtaining an authorization code using this method.
         *
         * invalid_scope             - The requested scope is invalid, unknown, or malformed.
         *
         * server_error              - The authorization server encountered an unexpected condition that prevented it from fulfilling the request.
         *                             (This error code is needed because a 500 Internal Server Error HTTP status code cannot be returned to the client via an HTTP redirect.)
         * temporarily_unavailable   - The authorization server is currently unable to handle the request due to a temporary overloading or maintenance of the server.
         *                             (This error code is needed because a 503 Service Unavailable HTTP status code cannot be returned to the client via an HTTP redirect.)
         *
         */

        const std::string& error = body_["error"].asString();
        // ... notify ...
        if ( false ==  bypass_rfc_ ) {
            // ... following rfc ...
            if ( 0 == error.compare("invalid_request") || 0 == error.compare("invalid_scope") ||
                 0 == error.compare("unauthorized_client") || 0 == error.compare("access_denied") ) {
                std::string uri = redirect_uri_;
                if ( nullptr != strchr(uri.c_str(), '?') ) {
                    uri += "&";
                } else {
                    uri += "?";
                }
                uri += "error=" + error;
                if ( 0 != state_.length() ) {
                    uri += "&state=" + state_;
                }
                a_redirect_callback(uri);
            } else if ( 0 == error.compare("unsupported_response_type") ) {
                a_json_callback(501  /* NGX_HTTP_NOT_IMPLEMENTED */, headers_, body_);
            } else if ( 0 == error.compare("temporarily_unavailable") ) {
                a_json_callback(503  /* NGX_HTTP_SERVICE_UNAVAILABLE */, headers_, body_);
            } else { /* server_error */
                a_json_callback(500 /* NGX_HTTP_INTERNAL_SERVER_ERROR */, headers_, body_);
            }
        } else {
            // ... not following rfc, send a JSON response ...
            if ( 0 == error.compare("invalid_request") || 0 == error.compare("invalid_scope") ) {
                a_json_callback(400 /* NGX_HTTP_BAD_REQUEST */, headers_, body_);
            } else if ( 0 == error.compare("unauthorized_client") || 0 == error.compare("invalid_token") ) {
                a_json_callback(401 /* NGX_HTTP_UNAUTHORIZED */, headers_, body_);
            } else if ( 0 == error.compare("access_denied") ) {
                a_json_callback(403  /* NGX_HTTP_FORBIDDEN */, headers_, body_);
            } else if ( 0 == error.compare("unsupported_response_type") ) {
                a_json_callback(501  /* NGX_HTTP_NOT_IMPLEMENTED */, headers_, body_);
            } else if ( 0 == error.compare("temporarily_unavailable") ) {
                a_json_callback(503  /* NGX_HTTP_SERVICE_UNAVAILABLE */, headers_, body_);
            } else { /* server_error */
                a_json_callback(500 /* NGX_HTTP_INTERNAL_SERVER_ERROR */, headers_, body_);
            }
        }

    });
    
}
