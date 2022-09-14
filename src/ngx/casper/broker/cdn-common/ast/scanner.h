/**
 * @file scanner.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_SCANNER_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_SCANNER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <inttypes.h> // uint64_t
#include <sys/types.h>  // size_t

#include <string>

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
                        
                        class Scanner final : public ::cc::NonCopyable, public ::cc::NonMovable
                        {
                            
                            friend class Parser;
                            
                        private: // Refs
                            
                            Tree& tree_;
                            
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
                            
                        private: // Data - temporary variables
                            
                            uint64_t number_;
                            
                        public: // Constructor(s) / Destructor
                            
                            Scanner () = delete;
                            Scanner (Tree& a_tree);
                            virtual ~Scanner();
                            
                        public: // Method(s) / Function(s)
                            
                            void               Set               (const char* a_data, size_t a_length);
                            void               ThrowParsingError (const std::string& a_title, const size_t a_column);
                            Parser::token_type Scan               (Parser::semantic_type* o_value, location* o_location);
                            
                        }; // end of class 'Scanner'
                        
                    } // end of namespace 'ast'
                    
                } // end of namespace 'common'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_SCANNER_H_
