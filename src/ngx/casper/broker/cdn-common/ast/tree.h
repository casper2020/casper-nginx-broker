/**
 * @file tree.h
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

#pragma once
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_TREE_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_TREE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"
#include "cc/types.h"

#include "ngx/casper/broker/cdn-common/ast/node.h"

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <functional>

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                namespace common
                {
                    
                    namespace ast
                    {
                        
                        class Tree final : public ::cc::NonCopyable, public ::cc::NonMovable
                        {
                            
                            friend class Parser;
                            
                        private: // Data
                            
                            std::map<std::string, Node*> roots_;
                            std::vector<Node*>           nodes_;
                            char                         tmp_[21];
                            
                        public: // Constructor(s) / Destructor
                            
                            Tree ();
                            virtual ~Tree();
                            
                        public: // Inline Method(s) / Function(s)
                            
                            Node* Add ();
                            Node* Add (const uint64_t& a_value, bool a_hex = false);
                            Node* Add (const std::string& a_value);
                            Node* AddExpression (const std::string& a_operator, Node* a_left, Node* a_right);
                            
                            void Push (const std::string& a_id, Node* a_node);
                            
                        public: // Method(s) / Function(s)
                            
                            void Compile (const std::string& a_expressions,
                                          const std::function<void(const std::string&, const std::string&)>& a_callback);
                            void Clear ();
                            
                        private: // Method(s) / Function(s)
                            
                            void Compile (const Node* a_node, std::stringstream& o_ss) const;
                            
                        }; // end of class 'Tree'

                        inline Node* Tree::Add ()
                        {
                            Node* node = new Node();
                            nodes_.push_back(node);
                            return node;
                        }

                        inline Node* Tree::Add (const uint64_t& a_value, bool a_hex)
                        {
                            Node* node = new Node();
                            if ( true == a_hex ) {
                                tmp_[0] = '\0';
                                const int len = snprintf(tmp_, sizeof(tmp_) / sizeof(tmp_[0]), "0x" UINT64_HEX_FMT, a_value);
                                if ( len < 0 || len > 20 ) {
                                    throw std::runtime_error("snprintf: cloudn't write a uint64_t to tmp_ buffer!");
                                } else {
                                    (*node) = tmp_;
                                }
                            } else {
                                (*node) = a_value;
                            }
                            nodes_.push_back(node);
                            return node;
                        }
                        
                        inline Node* Tree::Add (const std::string& a_value)
                        {
                            Node* node = new Node(Node::Type::Variable);
                            (*node) = a_value;
                            nodes_.push_back(node);
                            return node;
                        }
                        
                        inline Node* Tree::AddExpression (const std::string& a_operator, Node* a_left, Node* a_right)
                        {
                            Node* node = new Node(Node::Type::Expression, a_left, a_right);
                            (*node) = a_operator;
                            nodes_.push_back(node);
                            return node;
                        }
                        
                        inline void Tree::Push (const std::string& a_id, Node* a_node)
                        {
                            roots_[a_id] = a_node;
                        }

                    } // end of namespace 'ast'
                    
                } // end of namespace 'common'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_TREE_H_
