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

%skeleton "lalr1.cc"
%require "3.0"

%defines
// %define api.pure full

%define api.namespace  { ngx::casper::broker::cdn::common::ast }
%define api.value.type { class Node* }
%define parser_class_name { Parser }

%code requires {
    
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
                            class Scanner;
                            
                        } // end of namespace 'ast'
                    } // end of namespace 'common'
                } // end of namespace 'cdn'
            } // end of namespace 'broker'
        } // end of namespace 'casper'
    } // end of namespace 'ngx'

    #include <cmath>
    #include <iostream>
    #include <stdio.h>
    #include <exception>
    
    #include "ngx/casper/broker/cdn-common/ast/tree.h"

}

%parse-param { Tree& tree_ } { ngx::casper::broker::cdn::common::ast::Scanner& scanner_ }
%code {
    #include "ngx/casper/broker/cdn-common/ast/scanner.h"
    #define yylex scanner_.Scan
}

%nonassoc BEGIN         "--- BEGIN ---"
%token END           0

%token TK_null       "null"

%token PERM          "Permission"
%token NUM           "Number"
%token VAR           "Variable"
%token HEX           "Hex"

%token NE            "!="
%token EQ            "=="
%token OR            "||"
%token AND           "&&"
%token BO_AND        "&"
%token BO_OR         "|"

%left  '='
%left OR
%left AND
%left BO_OR
%left BO_AND
%left NE
%left EQ
%left  '(' ')'

/*
* The grammar follows
*/
%%
input:
      END
    | expr_list END { /* fprintf(stdout, "End\n");  fflush(stdout); $$ = nullptr; */ }
;

expr:
    term ';' ;

expr_list:
      expr
    | expr_list expr
;


term:
    expressions
  | leaf_terminals
;

leaf_terminals:
      NUM            { $$ = tree_.Add($1->number_);  }
    | HEX            { $$ = tree_.Add($1->number_, /* a_hex */ true);  }
    | VAR            { $$ = tree_.Add($1->string_);  }
    | PERM           { $$ = tree_.Add($1->string_);  }
    | TK_null        { $$ = tree_.Add();             }
;

expressions:
'(' term ')'         { $2->SetWrap(); $$ = $2;                  }
| term '=' term      { tree_.Push($1->string(), $3);   }
| term BO_AND term   { $$ = tree_.AddExpression("&",$1,$3);     }
| term BO_OR term    { $$ = tree_.AddExpression("|",$1,$3);     }
| term  NE term      { $$ = tree_.AddExpression("!=",$1,$3);    }
| term  EQ term      { $$ = tree_.AddExpression("==",$1,$3);    }
| term  OR term      { $$ = tree_.AddExpression("||",$1,$3);    }
| term AND term      { $$ = tree_.AddExpression("&&",$1,$3);    }
;
%%

void ngx::casper::broker::cdn::common::ast::Parser::error (const location_type& a_location, const std::string& a_msg)
{
    scanner_.ThrowParsingError("Parsing error", a_location.begin.column);
}
