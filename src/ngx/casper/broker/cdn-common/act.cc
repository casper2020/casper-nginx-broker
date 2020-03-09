/**
 * @file act.cc
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

#include "ngx/casper/broker/cdn-common/act.h"

#include "ngx/casper/broker/cdn-common/ast/evaluator.h"

/**
 * @brief Default constructor.
 *
 * param a_config
 * param a_headers_map
 */
ngx::casper::broker::cdn::ACT::ACT (const Json::Value& a_config,
                                    const std::map<std::string, std::string>& a_headers_map)
    : config_(a_config), headers_map_(a_headers_map)
{
    /* empty */
}

/**
 * @brief Destructor
 */
ngx::casper::broker::cdn::ACT::~ACT ()
{
    for ( auto it : numeric_variables_ ) {
        delete it.second;
    }
    numeric_variables_.clear();
}

/**
 * @brief Setup ACT.
 */
void ngx::casper::broker::cdn::ACT::Setup ()
{
    for ( auto it : numeric_variables_ ) {
        delete it.second;
    }
    numeric_variables_.clear();

    const Json::Value& variables = config_["variables"];
    for ( auto type : variables.getMemberNames() ) {
        const Json::Value& var_array = variables[type];
        if ( 0 == strcasecmp(type.c_str(), "numeric") || 0 == strcasecmp(type.c_str(), "hex") ) {
            for ( Json::ArrayIndex idx = 0 ; idx < var_array.size() ; ++idx ) {
                const Json::Value& variable = var_array[idx];
                numeric_variables_[variable["name"].asString()] = new Variable<uint64_t, XNumber>(
                     variable["header"].asCString(), static_cast<uint64_t>(variable.get("default", 0).asUInt64()), false == variable["default"].isNull()
                );
            }
        } else {
            throw cc::Exception("Don't know how to handle ACT variable type '%s'!", type.c_str());
        }
    }
    
    for ( auto it : numeric_variables_ ) {
        if ( it.second->can_default_ ) {
            it.second->value().Set(headers_map_, &it.second->default_);
        } else {
            it.second->value().Set(headers_map_);
        }
    }
}

/**
 * @brief Compile human readable expressions into a set of 'AST' expressions.
 *
 * param a_expressions
 */
void ngx::casper::broker::cdn::ACT::Compile (const std::string& a_expressions,
                                             const std::function<void(const std::string& ,const std::string&)>& a_callback)
{
    tree_.Compile(a_expressions, a_callback);
}

/**
 * @brief Evaluate a previous compiled expression.
 *
 * @param a_expression The pre-compiled expression to evaluate.
 *
 * @return The epxression result.
 */
uint64_t ngx::casper::broker::cdn::ACT::Evaluate (const std::string& a_expression) const
{
    ngx::casper::broker::cdn::common::ast::Evaluator evaluator;
    return evaluator.Evaluate(a_expression, [this] (const std::string& a_name) -> uint64_t {
        const auto it = numeric_variables_.find(a_name);
        if ( numeric_variables_.end() == it ) {
            throw cc::Exception("ACT variable '%s' is not defined!", a_name.c_str());
        }
        return (const uint64_t&)(it->second->value());
    });
}
