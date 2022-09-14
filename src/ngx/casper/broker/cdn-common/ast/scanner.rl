/**
* @file scanner.rl
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

#include "ngx/casper/broker/cdn-common/ast/scanner.h"

#include "cc/types.h"

#include "osal/utils/pow10.h"

 %%{
     machine scanner;
     alphtype char;
     
     variable cs    cs_;
     variable p     p_;
     variable pe    pe_;
     variable eof   eof_;
     variable ts    ts_;
     variable te    te_;
     variable act   act_;

     write data;

     ws = [ \t\n];
     
     __number     = ( [0-9]* );
     __variable   = ( [a-zA-Z_]* );
     __permission = ( [rwdRWD]* );
     __hex_number = ( '0x'[0-9A-Fa-f]+ );

    main := |*
     
        '('   => { rv = (ngx::casper::broker::cdn::common::ast::Parser::token_type) '(' ; fbreak; };
        ')'   => { rv = (ngx::casper::broker::cdn::common::ast::Parser::token_type) ')' ; fbreak; };

        '&'   => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::BO_AND    ; fbreak; };
        '|'   => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::BO_OR     ; fbreak; };

        '!='  => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::NE        ; fbreak; };
        '=='  => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::EQ        ; fbreak; };
        '&&'  => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::AND       ; fbreak; };
        '||'  => { rv = ngx::casper::broker::cdn::common::ast::Parser::token::OR        ; fbreak; };

        '='   => { rv = (ngx::casper::broker::cdn::common::ast::Parser::token_type) '=' ; fbreak; };
        ';'   => { rv = (ngx::casper::broker::cdn::common::ast::Parser::token_type) ';' ; fbreak; };
     
        __permission => {
            // fprintf(stdout, "__permission: %s\n", std::string(ts_, te_ - ts_).c_str());fflush(stdout);
            rv = ngx::casper::broker::cdn::common::ast::Parser::token::PERM;
            (*value) = std::string(ts_, static_cast<size_t>(te_ - ts_));
            fbreak;
        };

        __variable    => {
            // fprintf(stdout, "__variable: %s\n", std::string(ts_, static_cast<size_t>(te_ - ts_)).c_str());fflush(stdout);
            rv = ngx::casper::broker::cdn::common::ast::Parser::token::VAR;
            (*value) = std::string(ts_, static_cast<size_t>(te_ - ts_));
            fbreak;
        };
     
        __number => {
            // fprintf(stdout, "__number: " UINT64_FMT "\n", number_);fflush(stdout);
            if ( 1 == sscanf(std::string(ts_, static_cast<size_t>(te_ - ts_)).c_str(), UINT64_FMT, &number_) ) {
                (*value) = number_;
                rv = ngx::casper::broker::cdn::common::ast::Parser::token::NUM;
            } else {
                rv = ngx::casper::broker::cdn::common::ast::Parser::token::TK_null;
            }
            fbreak;
        };

        __hex_number => {
            // fprintf(stdout, "hex number: %s\n", std::string(ts_, static_cast<size_t>(te_ - ts_)).c_str());fflush(stdout);
            if ( 1 == sscanf(std::string(ts_, static_cast<size_t>(te_ - ts_)).c_str(), UINT64_HEX_FMT, &number_) ) {
                (*value) = number_;
                rv = ngx::casper::broker::cdn::common::ast::Parser::token::HEX;
            } else if ( 1 == sscanf(std::string(ts_, static_cast<size_t>(te_ - ts_)).c_str(), UINT64_hex_FMT, &number_) ) {
                (*value) = number_;
                rv = ngx::casper::broker::cdn::common::ast::Parser::token::HEX;
            } else {
                rv = ngx::casper::broker::cdn::common::ast::Parser::token::TK_null;
            }
            fbreak;
        };
     
        ws*;

    *|;
}%%

/**
 * @brief Default constructor.
 *
 * @param a_tree
 */
ngx::casper::broker::cdn::common::ast::Scanner::Scanner (ngx::casper::broker::cdn::common::ast::Tree& a_tree)
: tree_(a_tree), input_(nullptr), p_(nullptr), pe_(nullptr), eof_(nullptr), cs_(0), ts_(nullptr), te_(nullptr), act_(0)
{
    number_ = 0;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::ast::Scanner::~Scanner ()
{
    /* empty */
}

/**
 * @brief Set input data.
 *
 * @param a_data
 * @param a_length
 */
void ngx::casper::broker::cdn::common::ast::Scanner::Set (const char* a_data, size_t a_length)
{
    input_ = a_data;
    p_     = a_data;
    pe_    = a_data + a_length;
    eof_   = pe_;
    %% write init;
    (void)(scanner_first_final);
    (void)(scanner_error);
    (void)(scanner_en_main);
}

/**
 * @brief Parse previously set expressions set.
 *
 * param o_value
 * param o_location
 */
ngx::casper::broker::cdn::common::ast::Parser::token_type ngx::casper::broker::cdn::common::ast::Scanner::Scan (ngx::casper::broker::cdn::common::ast::Parser::semantic_type* o_value, ngx::casper::broker::cdn::common::ast::location* o_location)
{
    ngx::casper::broker::cdn::common::ast::Parser::token_type rv = ngx::casper::broker::cdn::common::ast::Parser::token::END;

    if ( nullptr == (*o_value) ) {
        (*o_value) = tree_.Add();
    }

    ngx::casper::broker::cdn::common::ast::Parser::semantic_type value = (*o_value);

    %% write exec;
    
    o_location->begin.line   = 1;
    o_location->begin.column = (int)(ts_ - input_);
    o_location->end.line     = 1;
    o_location->end.column   = (int)(te_ - input_ - 1);

    if ( ngx::casper::broker::cdn::common::ast::Parser::token_type::TK_null == rv
            &&
        ngx::casper::broker::cdn::common::ast::Node::Type::Undefined == (*o_value)->type()
    ) {
        ngx::casper::broker::cdn::common::ast::Scanner::ThrowParsingError("Scanner error", static_cast<size_t>(o_location->begin.column));
    }
    
    return rv;
}

/**
 * @brief Throw a \link std::runtime_error \link with input message and error location.
 *
 * @param a_title
 * @param a_column
 */
void ngx::casper::broker::cdn::common::ast::Scanner::ThrowParsingError (const std::string& a_title, const size_t a_column)
{
    const size_t l1 = a_title.length() + ( static_cast<size_t>( pe_ - input_ ) + ( a_column + 20 ) ) * sizeof(char);
    char* b1        = new char[l1]; b1[0] = '\0';

    snprintf(b1, l1 - sizeof(char),
             "%s:\n"
             "   %*.*s\n"
             "   %*.*s^~~~~~\n",
             a_title.c_str(),
             (int)(pe_ - input_), (int)(pe_ - input_), input_,
             (int)a_column, (int)a_column, " "
    );

    const std::string msg = std::string(b1);
    
    fprintf(stdout, "\n\n%s\n\n", msg.c_str());
    fflush(stdout);

    delete [] b1;

    throw std::runtime_error(msg);
}
