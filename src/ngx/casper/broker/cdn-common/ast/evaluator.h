/**
 * @file evaluator.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <functional>
#include <stack>

#include "ngx/casper/broker/cdn-common/ast/tree.h"
#include "ngx/casper/broker/cdn-common/ast/parser.hh"

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
                        
                        class Evaluator final : public ::cc::NonCopyable, public ::cc::NonMovable
                        {
                            
                        public: // Enum
                            
                            enum class Operator : uint8_t {
                                NotSet,
                                LogicalOr,
                                LogicalAnd,
                                BitwiseOr,
                                BitwiseAnd,
                                RelationalEqual,
                                RelationalNotEqual
                            };
                            
                        private:
                            
                            typedef struct {
                                uint64_t  left_;
                                uint64_t  right_;
                                uint64_t* current_;
                            } Value;
                            
                            typedef struct {
                                const char* start_ptr_;
                                const char* end_ptr_;
                                Operator    operator_;
                                Value       value_;
                                bool        result_;
                            } Expression;
                            
                        private: // Const DATA
                            
                            char const* input_;
                            
                        private: // Data - interface to ragel scanner
                            
                            char const* p_;
                            char const* pe_;
                            char const* eof_;
                            int         cs_;
                            char const* ts_;
                            char const* te_;
                            int         act_;
                            int         top_;
                            int*        stack_;
                            int         stack_size_;

                        private: // Data
                            
                            const char* read_ptr_;
                            int         error_column_;
                            
                            Operator                 operator_;
                            const char*              operator_sp_;
                            const char*              operator_ep_;
                            Expression*              expression_;
                            std::stack<Expression*>  expressions_;
                            uint64_t                 result_;

                        public: // Constructor(s) / Destructor
                            
                            Evaluator();
                            virtual ~Evaluator();
                            
                        public: // Method(s) / Function(s)
                            
                            uint64_t Evaluate (const std::string& a_expression,                                               
                                               const std::function<uint64_t(const std::string& a_name)>& a_callback);
                            
                        }; // end of class 'Evaluator'
                        
                    } // end of namespace 'ast'
                    
                } // end of namespace 'common'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_H_

