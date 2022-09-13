/**
* @file evaluator.rl
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

#include "ngx/casper/broker/cdn-common/ast/evaluator.h"

#include "cc/exception.h"

#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_OPEN_EXPRESSION
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_CLOSE_EXPRESSION
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_VARIABLE
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_NUMBER
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_DECLARE_VARIABLE
#undef NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE

#if 0 // ( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_DECLARE_VARIABLE(a_type, a_name) \
    	a_type a_name
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(a_name, a_value) \
        a_name = a_value
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE(a_format, ...) \
        fprintf(stdout, "%*c" a_format, ( top_ + static_cast<int>(expressions_.size()) ), ' ', __VA_ARGS__); fflush(stdout);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_OPEN_EXPRESSION(a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ o ] EXPRESSION: " a_format, __VA_ARGS__);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_CLOSE_EXPRESSION(a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ c ] EXPRESSION: " a_format, __VA_ARGS__);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_VARIABLE(a_name, a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ → ] VARIABLE  : %s := " a_format, a_name, __VA_ARGS__);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_NUMBER(a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ → ] NUMBER    : " a_format, __VA_ARGS__);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR(a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ → ] OPERATOR  : " a_format, __VA_ARGS__);
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_RESULT(a_format, ...) \
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ r ] RESULT    : " a_format, __VA_ARGS__);
#else
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_DECLARE_VARIABLE(a_type, a_name)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(a_name, a_value) 
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE(a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_OPEN_EXPRESSION(a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_CLOSE_EXPRESSION(a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_VARIABLE(a_name, a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_NUMBER(a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR(a_format, ...)
    #define NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_RESULT(a_format, ...)
#endif

%%{
    machine evaluator;
    alphtype char;
    
    variable cs    cs_;
    variable p     p_;
    variable pe    pe_;
    variable eof   eof_;
    variable ts    ts_;
    variable te    te_;
    variable act   act_;
    variable top   top_;
    variable stack stack_;
    
    write data;
    
    action dgt
    {
        (*expression_->value_.current_) *= 10;
        (*expression_->value_.current_) += (fc - '0');
    }
    
    action parse_error
    {
        error_column_ = int(fpc - input_);
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE(
                 "[ %s ] %s\n"
                 "   %*.*s\n"
                 "   %*.*s^~~~~~\n",
                 "💣",
                 input_,
                 (int)(pe_ - input_), (int)(pe_ - input_), input_,
                 error_column_, error_column_, " "
        );
        fbreak;
    }
    
    action open_expression
    {
//        fprintf(stdout, "open_expression\n");
//        fprintf(stdout, "\t[p_       ] : %s\n", p_);
//        fprintf(stdout, "\t[read_ptr_] : %s\n", read_ptr_);
//        fprintf(stdout, "\t[cs_      ] : %d\n", cs_);
//        fprintf(stdout, "\t[ts_      ] : %s\n", ts_);
//        fprintf(stdout, "\t[te_      ] : %s\n", te_);
//        fprintf(stdout, "\t[pe_      ] : %s\n", pe_);
//        fprintf(stdout, "\t[fpc      ] : %s\n", fpc);
//        fprintf(stdout, "\t[eof_     ] : %s\n", eof_);

        expression_                  = new ngx::casper::broker::cdn::common::ast::Evaluator::Expression();
        expression_->start_ptr_      = operator_sp_;
        expression_->end_ptr_        = nullptr;
        expression_->value_.left_    = 0;
        expression_->value_.right_   = 0;
        expression_->value_.current_ = &expression_->value_.left_;
        expression_->operator_       = operator_;

        expressions_.push(expression_);

        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_OPEN_EXPRESSION("%s\n", expression_->start_ptr_);

        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("\t[operator_sp_] : %s\n", operator_sp_);
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("\t[operator_ep_] : %s\n", operator_ep_);
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("\t[operator_ep_] : %2d ~> %*.*s\n",
                                                                   int(operator_ep_ - operator_sp_), int(operator_ep_ - operator_sp_), int(operator_ep_ - operator_sp_), operator_sp_
        );
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("\t[read_ptr_   ] : %s\n", read_ptr_);
    }

    action close_expression
    {
        expression_->end_ptr_ = p_;
        
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_DECLARE_VARIABLE(const char*, op);
        switch(expression_->operator_)
        {
            case Operator::LogicalOr:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "||");
                expression_->result_ = ( expression_->value_.left_ || expression_->value_.right_ );
                break;
            case Operator::LogicalAnd:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "&&");
                expression_->result_ = ( expression_->value_.left_ && expression_->value_.right_ );
                break;
            case Operator::BitwiseOr:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "|");
                expression_->result_ = 0 != ( expression_->value_.left_ | expression_->value_.right_ );
                break;
            case Operator::BitwiseAnd:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "&");
                expression_->result_ = 0 != ( expression_->value_.left_ & expression_->value_.right_ );
                break;
            case Operator::RelationalEqual:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "==");
                expression_->result_ = ( expression_->value_.left_ == expression_->value_.right_ );
                break;
            case Operator::RelationalNotEqual:
                NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_SET_VARIABLE(op, "!=");
                expression_->result_ = ( expression_->value_.left_ != expression_->value_.right_ );
                break;
            default:
                error_column_ = int(fpc - input_);
                break;
        }
        
        if ( -1 != error_column_ ) {
            fbreak;
        }
        
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_CLOSE_EXPRESSION("%2d ~> %*.*s\n",
            int(expression_->end_ptr_ - expression_->start_ptr_), int(expression_->end_ptr_ - expression_->start_ptr_), int(expression_->end_ptr_ - expression_->start_ptr_), expression_->start_ptr_
        );
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_RESULT("%*.*s ~> %llu %s %llu ~> %s \n",
                int(expression_->end_ptr_ - expression_->start_ptr_), int(expression_->end_ptr_ - expression_->start_ptr_), expression_->start_ptr_,
                expression_->value_.left_, op, expression_->value_.right_,
                expression_->result_ ? "true" : "false"
        );
        
        const bool result = expression_->result_;
        
        delete expressions_.top();
        expressions_.pop();
        
        expression_ = expressions_.size() > 0 ? expressions_.top() : nullptr;
        
        if ( nullptr != expression_ ) {
            (*expression_->value_.current_) = ( result ? 1 : 0 );
            if ( expression_->value_.current_ == &expression_->value_.left_ ) {
                expression_->value_.current_ = &expression_->value_.right_;
            } else {
                expression_->value_.current_ = &expression_->value_.left_;
            }
        } else {
            result_ = result;
        }
    }
    
    action save_variable
    {
        const auto        rwnd     = static_cast<size_t>( p_ - read_ptr_ );
        const std::string variable = std::string(read_ptr_, rwnd);
        
        (*expression_->value_.current_) = a_callback(variable);
        
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_VARIABLE(variable.c_str(),
                                                                                "%llu\n", (*expression_->value_.current_)
        );
        
        if ( expression_->value_.current_ == &expression_->value_.left_ ) {
            expression_->value_.current_ = &expression_->value_.right_;
        } else {
            expression_->value_.current_ = &expression_->value_.left_;
        }
    }

    action save_number
    {
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_NUMBER("%llu\n", (*expression_->value_.current_));
        if ( expression_->value_.current_ == &expression_->value_.left_ ) {
            expression_->value_.current_ = &expression_->value_.right_;
        } else {
            expression_->value_.current_ = &expression_->value_.left_;
        }
    }
    
    action save_hex_number
    {
        (*expression_->value_.current_) = 0;
        uint64_t base  = 1;
        for ( ssize_t idx = static_cast<ssize_t>(p_ - read_ptr_) - 1 ;  idx >= 2 /* 0x */ ; idx --) {
            if ( read_ptr_[idx] >= '0' && read_ptr_[idx] <= '9') {
                (*expression_->value_.current_) += ( read_ptr_[idx] - 48 ) * base;
            } else if ( read_ptr_[idx] >= 'A' && read_ptr_[idx] <= 'F') {
                (*expression_->value_.current_) += ( read_ptr_[idx] - 55 ) * base;
            } else if ( read_ptr_[idx] >= 'a' && read_ptr_[idx] <= 'f') {
                (*expression_->value_.current_) += ( read_ptr_[idx] - 87 ) * base;
            } else {
                error_column_ = int(fpc - input_);
                fbreak;
            }
            base  *= 16;
        }
        
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_NUMBER("0x%llx\n", (*expression_->value_.current_));

        if ( expression_->value_.current_ == &expression_->value_.left_ ) {
            expression_->value_.current_ = &expression_->value_.right_;
        } else {
            expression_->value_.current_ = &expression_->value_.left_;
        }
    }

    ws = [ \t\n];
    
    __logical_or_           = ( "||" >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "||"); operator_ = Operator::LogicalOr;          operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    __logical_and_          = ( "&&" >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "&&"); operator_ = Operator::LogicalAnd;         operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    __bitwise_or_           = ( "|"  >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "|" ); operator_ = Operator::BitwiseOr;          operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    __bitwise_and_          = ( "&"  >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "&" ); operator_ = Operator::BitwiseAnd;         operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    __relational_equal      = ( "==" >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "=="); operator_ = Operator::RelationalEqual;    operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    __relational_not_equal  = ( "!=" >{ operator_sp_ = p_;} %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE_SET_OPERATOR("%s\n", "!="); operator_ = Operator::RelationalNotEqual; operator_ep_ = p_; read_ptr_ = p_ + sizeof(char); } );
    
    __operator              = ( __logical_or_ | __logical_and_ | __bitwise_or_ | __bitwise_and_ | __relational_equal | __relational_not_equal );
    
    __start_expression      = ( __operator '(' >open_expression );
    
    __number                = ( zlen >{ (*expression_->value_.current_) = 0; } [0-9]+ $dgt %save_number);
    
    __variable              = ( ( [a-zA-Z_]* ) %save_variable);
    __hex_number            = ( ( '0x'[0-9A-Fa-f]+ ) %save_hex_number );

    __item                  = ( __variable | __number | __hex_number | ( __start_expression @{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ ↳ ] CALL      : cs_: %3d p_: %s\n", cs_, p_); fcall __expression; } %close_expression ) );
    __expression            := __item ',' %{ read_ptr_ = p_; } __item ')'                   @{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ ↵ ] RETURN    : cs_: %3d p_: %s\n", cs_, p_); fret;               };
  
    main                    := ( __item ';' %{ NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ %s ] %llu\n", "🍾", result_); } ) @err(parse_error);
        
    prepush {
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ ↥ ] STACK PUSH: top_: %d, stack_size_: %d\n", top_, stack_size_);
        if ( stack_size_ == 0 ) {
            stack_size_ = 20;
            stack_ = (int*) malloc(sizeof(int) * stack_size_);
        }
        if ( top_ == stack_size_ -1 ) {
            stack_size_ *= 2;
            stack_ = (int*) realloc(stack_, sizeof(int) * stack_size_);
        }
    }
    
    postpop {
        NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ ↧ ] STACK POP : top_: %d, stack_size_: %d\n", top_, stack_size_);
    }
    
}%%

/**
* @brief Default constructor.
*/
ngx::casper::broker::cdn::common::ast::Evaluator::Evaluator ()
    :  input_(nullptr), p_(nullptr), pe_(nullptr), eof_(nullptr), cs_(0), ts_(nullptr), te_(nullptr), act_(0), top_(0), stack_(nullptr), stack_size_(0)
{
    read_ptr_     = nullptr;
    error_column_ = -1;
    operator_     = Operator::NotSet;
    operator_sp_  = nullptr;
    operator_ep_  = nullptr;
    expression_   = nullptr;
    result_       = 0;
    (void)ts_;
    (void)te_;
    (void)act_;
}

/**
* @brief Destructor.
*/
ngx::casper::broker::cdn::common::ast::Evaluator::~Evaluator ()
{
    while ( expressions_.size() > 0 ) {
        delete expressions_.top();
        expressions_.pop();
    }
    if ( nullptr != stack_ ) {
        free(stack_);
    }
}

/**
 * @brief Set input data.
 *
 * param a_expression
 * param a_callback
 */
uint64_t ngx::casper::broker::cdn::common::ast::Evaluator::Evaluate (const std::string& a_expression,
                                                                     const std::function<uint64_t(const std::string& a_name)>& a_callback)
{
    read_ptr_     = a_expression.c_str();
    error_column_ = -1;
    
    while ( expressions_.size() > 0 ) {
        delete expressions_.top();
        expressions_.pop();
    }
    expression_ = nullptr;
    
    // ... operator reset ...
    operator_    = Operator::NotSet;
    operator_sp_ = nullptr;
    operator_ep_ = nullptr;

    // ... setup ragel variables ..
    input_ = a_expression.c_str();
    p_     = a_expression.c_str();
    pe_    = a_expression.c_str() + a_expression.length();
    eof_   = pe_;
    
    // ... stack variables ...
    top_ = 0;
    if ( nullptr != stack_ ) {
        free(stack_);
    }
    stack_      = nullptr;
    stack_size_ = 0;
    
    NRS_NGX_CASPER_BROKER_CDN_COMMON_AST_EVALUATOR_DEBUG_TRACE("[ E ] %s\n", input_);
    
    %% write init;
    
    // ... run ...
    %% write exec;
    
    (void)(evaluator_first_final);
    (void)(evaluator_error);
    (void)(evaluator_en_main);
    (void)(evaluator_en___expression);
    
    // ... error set?
    if ( -1 != error_column_ ) {
        throw ::cc::Exception("Parse error near column %3d: %s", error_column_, input_);
    }

    // ... done ...
    return result_;
}
