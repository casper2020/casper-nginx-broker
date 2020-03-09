/**
 * @file node.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_NODE_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_NODE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <stdint.h>    // uint8_t, etc
#include <sys/types.h> // uint64_t
#include <string>

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
                        
                        class Node final : public ::cc::NonCopyable, public ::cc::NonMovable
                        {
                            
                            friend class Parser;
                            
                        public: // Enum(s)
                            
                            enum class Type : uint8_t
                            {
                                Undefined,
                                Expression,
                                Hex,
                                Number,
                                Variable
                            };
                            
                        private: // Data
                            
                            Type        type_;
                            Node*       right_ptr_;
                            Node*       left_ptr_;
                            uint64_t    number_;
                            std::string string_;
                            bool        wrap_;

                        public: // Constructor(s) / Destructor
                            
                            Node (const Type& a_type = Type::Undefined, Node* a_left = nullptr, Node* a_right = nullptr)
                            {
                                type_      = a_type;
                                left_ptr_  = a_left;
                                right_ptr_ = a_right;
                                number_    = 0;
                                wrap_      = false;
                            }
                            
                            virtual ~Node()
                            {
                                /* empty */
                            }
                            
                        public: // Operator(s)
                            
                            inline void operator = (const Type& a_type)
                            {
                                type_ = a_type;
                            }
                            
                            inline void operator = (const uint64_t& a_value)
                            {
                                type_   = Type::Number;
                                number_ = a_value;
                            }

                            inline void operator = (const std::string& a_value)
                            {
                                if ( Type::Undefined == type_ ) {
                                    type_ = Type::Variable;
                                }
                                string_ = a_value;
                            }

                            inline void operator << (Node* a_node)
                            {
                                left_ptr_ = a_node;
                            }

                            inline void operator >> (Node* a_node)
                            {
                                right_ptr_ = a_node;
                            }
                            
                        public:
                            
                            inline void SetWrap ()
                            {
                                wrap_ = true;
                            }
                            
                            inline bool wrap () const
                            {
                                return wrap_;
                            }

                            inline const Node* left() const
                            {
                                return left_ptr_;
                            }

                            inline Node* right() const
                            {
                                return right_ptr_;
                            }
                            
                            inline const std::string& string() const
                            {
                                return string_;
                            }

                            inline const uint64_t& number() const
                            {
                                return number_;
                            }

                            inline Type type() const
                            {
                                return type_;
                            }
                            
                        }; // end of class 'Node'
                        
                    } // end of namespace 'ast'
                    
                } // end of namespace 'common'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_NODE_H_
