/**
 * @file abstract_token.cc
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

#include "ngx/casper/broker/oauth/server/abstract_token.h"

const std::string ngx::casper::broker::oauth::server::AbstractToken::k_access_token_ttl_field_  = "access_token_ttl";
const std::string ngx::casper::broker::oauth::server::AbstractToken::k_refresh_token_ttl_field_ = "refresh_token_ttl";


/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 * @param a_service_id
 * @param a_client_id
 * @param a_client_secret
 * @param a_redirect_uri
 */
ngx::casper::broker::oauth::server::AbstractToken::AbstractToken (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                                  const std::string& a_service_id,
                                                                  const std::string& a_client_id, const std::string& a_client_secret,
                                                                  const std::string& a_redirect_uri,
                                                                  bool a_bypass_rfc)
    : ngx::casper::broker::oauth::server::Object(a_loggable_data_ref, a_service_id, a_client_id, a_redirect_uri, a_bypass_rfc),
      client_secret_(a_client_secret), token_type_("Bearer")
{
    body_ = Json::Value(Json::ValueType::objectValue);
}

/**
 * @brief Destructor
 */
ngx::casper::broker::oauth::server::AbstractToken::~AbstractToken ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/*
 * @brief 5.2.  Error Response - https://tools.ietf.org/html/rfc6749#section-5.2
 *
 * @param a_error
 * @param a_error_description
 * @param a_callback
 * @param a_log_callback
 */
void ngx::casper::broker::oauth::server::AbstractToken::SetAndCallStandardJSONErrorResponse (const std::string& a_error, const std::string a_error_description,
                                                                                             ngx::casper::broker::oauth::server::AbstractToken::JSONCallback a_callback,
                                                                                             ngx::casper::broker::oauth::server::AbstractToken::LogCallback a_log_callback,
                                                                                             const cc::Exception& a_cc_exception)
{    
    /*
     * 5.2.  Error Response - https://tools.ietf.org/html/rfc6749#section-5.2
     *
     *  The authorization server responds with an HTTP 400 (Bad Request)
     * status code (unless specified otherwise) and includes the following
     * parameters with the response:
     *
     * [REQUIRED] - error             -
     * [OPTIONAL] - error_description -
     * [OPTIONAL] - error_uri         -
     *
     * !!!
     *
     * ...
     *
     * !!!
     *
     * Example:
     *
     * HTTP/1.1 400 Bad Request
     * Content-Type: application/json;charset=UTF-8
     * Cache-Control: no-store
     * Pragma: no-cache
     * {
     *   "error":"invalid_request"
     * }
     *
     */
    
    body_["error"] = a_error;
    if ( 0 != a_error_description.length() ) {
        body_["error_description"] = a_error;
    }

    // ... not following rfc, send a JSON response ...
    if ( 0 == a_error.compare("invalid_request") || 0 == a_error.compare("invalid_scope") ) {
        a_callback(400 /* NGX_HTTP_BAD_REQUEST */, headers_, body_);
    } else if ( 0 == a_error.compare("unauthorized_client") || 0 == a_error.compare("invalid_token" )) {
        a_callback(401 /* NGX_HTTP_UNAUTHORIZED */, headers_, body_);
    } else if ( 0 == a_error.compare("access_denied") ) {
        a_callback(403  /* NGX_HTTP_FORBIDDEN */, headers_, body_);
    } else if ( 0 == a_error.compare("unsupported_response_type") ) {
        a_callback(501  /* NGX_HTTP_NOT_IMPLEMENTED */, headers_, body_);
    } else if ( 0 == a_error.compare("temporarily_unavailable") ) {
        a_callback(503  /* NGX_HTTP_SERVICE_UNAVAILABLE */, headers_, body_);
    } else { /* server_error */
        a_callback(500 /* NGX_HTTP_INTERNAL_SERVER_ERROR */, headers_, body_);
    }
    
    // ... log exception ...
    a_log_callback(a_cc_exception.what());
}

/**
 * @brief Generate a new token.
 *
 * @param a_type       Token type: access or refresh.
 * @param a_error_code On error set thos code.
 * @param o_value      New token value.
 */
void ngx::casper::broker::oauth::server::AbstractToken::GenNewToken (const char* const /* a_name */, const char* const a_error_code, std::string& o_value)
{
    //
    // ENSURE WE HAVE 'NEW' MANDATORY TEMPLATE VALUES
    //
    std::map<std::string, std::string> tmp;
    for ( auto key : { "cluster", "entity_id", "user_id" } ) {
        for ( size_t idx = 0 ; idx < args_.size() ; idx += 2 ) {
            if ( 0 == args_[idx].compare(key) ) {
                tmp[key] = args_[idx + 1];
                break;
            }
        }
    }
    // ... if mandatory parameters are not present ...
    if ( 3 != tmp.size() ) {
        // ... set error code ...
        body_["error"] = a_error_code;
        // ... notify ...
        throw ev::Exception("Unable to confirm the following values: 'cluster', 'entity_id' and / or 'user_id'!");
    }
    // ... generate new token ...
    o_value  = tmp["cluster"] + '-' + tmp["entity_id"] + '-' + tmp["user_id"] + '-' + RandomString(64);
}
