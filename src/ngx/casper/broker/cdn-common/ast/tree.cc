/**
 * @file tree.cc
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

#include "ngx/casper/broker/cdn-common/ast/tree.h"

#include "ngx/casper/broker/cdn-common/ast/scanner.h"

#ifdef __APPLE__
#pragma mark - Tree
#endif

/**
 * @brief Default constructor.
 */
ngx::casper::broker::cdn::common::ast::Tree::Tree ()
{
    tmp_[0] = '\0';
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::ast::Tree::~Tree()
{
    Clear();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Compile human readable expressions into a set of 'AST' expressions.
 *
 * param a_expressions
 * param a_callback
 */
void ngx::casper::broker::cdn::common::ast::Tree::Compile (const std::string& a_expressions,
                                                           const std::function<void(const std::string&, const std::string&)>& a_callback)
{

    Clear();

    ngx::casper::broker::cdn::common::ast::Tree&   tree = (*this);
    ngx::casper::broker::cdn::common::ast::Scanner scanner(tree);
    ngx::casper::broker::cdn::common::ast::Parser  parser(tree, scanner);
    
    scanner.Set(a_expressions.c_str(), a_expressions.length());
    
    parser.parse();
    
    std::stringstream ss;
    for ( auto it : roots_ ) {
        ss.str(it.first);
        Compile(it.second, ss);
        ss << ';';
        a_callback(it.first, ss.str());
    }
}

/**
 * @brief Reset and deallocate data.
 */
void ngx::casper::broker::cdn::common::ast::Tree::Clear ()
{
    for ( auto node : nodes_ ) {
        delete node;
    }
    nodes_.clear();
    roots_.clear();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Compile this 'tree' into a set of 'AST' expressions.
 *
 * param a_node
 * param o_ss
 */
void ngx::casper::broker::cdn::common::ast::Tree::Compile (const ngx::casper::broker::cdn::common::ast::Node* a_node, std::stringstream& o_ss) const
{
    if ( nullptr == a_node ) {
        return;
    }
    
    switch ( a_node->type() ) {
        case ngx::casper::broker::cdn::common::ast::Node::Type::Undefined:
            break;
        case ngx::casper::broker::cdn::common::ast::Node::Type::Expression:
            o_ss << a_node->string();
            o_ss << '(';
            Compile(a_node->left(), o_ss);
            o_ss << ',';
            Compile(a_node->right(), o_ss);
            o_ss << ')';
            break;
        case ngx::casper::broker::cdn::common::ast::Node::Type::Number:
            o_ss << a_node->number();
            break;
        case ngx::casper::broker::cdn::common::ast::Node::Type::Hex:
            o_ss << a_node->string();
            break;
        case ngx::casper::broker::cdn::common::ast::Node::Type::Variable:
            o_ss << a_node->string();
            break;
        default:
            throw std::runtime_error("Don't know how to handle with AST Node Type " + std::to_string(static_cast<uint8_t>(a_node->type())));;
    }
}
