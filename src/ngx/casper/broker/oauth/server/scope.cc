/**
 * @file scope.cc
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

#include "ngx/casper/broker/oauth/server/scope.h"

#include <string.h>

/*
 * 3.3.  Access Token Scope - https://tools.ietf.org/html/rfc6749#section-3.3
 *
 * ...
 *
 * The value of the scope parameter is expressed as a list of space- delimited, case-sensitive strings.
 *
 * ...
 *
 * The strings are defined by the authorization server.
 *
 * ...
 */

/**
 * @brief Default constructor.
 *
 * @param a_allowed
 */
ngx::casper::broker::oauth::server::Scope::Scope (const ev::redis::Value& a_allowed)
{
    // ... parse 'allowed' scope ...
    if ( true == a_allowed.IsNil() || false == a_allowed.IsString() || 0 == a_allowed.String().length() ) {
        throw ev::Exception("Client scope list is empty or malformed - unable to grant scope(s)!");
    }
    Parse(a_allowed.String(), allowed_);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::oauth::server::Scope::~Scope ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Grant a scope or a set of scope.
 *
 * @param a_scope
 *
 * @return The granted scope(s).
 */
std::string ngx::casper::broker::oauth::server::Scope::Grant (const std::string& a_scope)
{
    std::vector<std::string> requested_scopes;
    std::vector<std::string> granted_scopes;

    // ... nothing to allow?
    if ( 0 == allowed_.size() ) {
        return "";
    }
    
    // ... parse scope 'list' ...
    Parse(a_scope, requested_scopes);
    
    // ... no scopes requested?
    if ( 0 == requested_scopes.size() || ( 1 == requested_scopes.size() && 0 == requested_scopes[0].length() )  ) {
        // .. grant default ...
        return Default();
    }
    
    // ...
    for ( auto requested : requested_scopes ) {
        for ( auto allowed : allowed_ ) {
            if ( 0 == requested.compare(allowed) ) {
                granted_scopes.push_back(requested);
                break;
            }
        }
    }
    
    // ... no scope granted?
    if ( 0 == granted_scopes.size() ) {
        return "";
    }
    
    // ... one or more scopes granted ...
    std::string rv = granted_scopes[0];
    for ( size_t idx = 1; idx  < granted_scopes.size() ; ++idx ) {
        rv += ' ' + granted_scopes[idx];
    }
    
    // ... we're done ...
    return rv;
}

#ifdef __APPLE__
#pragma mark -
#endif 

/**
 * @brief Parse scope string.
 *
 * @param a_scopes
 * @param o_scopes
 */
void ngx::casper::broker::oauth::server::Scope::Parse (const std::string& a_scopes, std::vector<std::string>& o_scopes)
{
    o_scopes.clear();
    // ... single or multiple scopes?
    if ( nullptr == strchr(a_scopes.c_str(), ' ') ) {
        // ... single ...
        o_scopes.push_back(a_scopes);
    } else {
        // ... multiple ...
        std::istringstream stream(a_scopes);
        std::string        scope;
        while ( std::getline(stream, scope, ' ') ) {
            if ( 0 == scope.length() ) {
                continue;
            }
            allowed_.push_back(scope);
        }
    }
}
