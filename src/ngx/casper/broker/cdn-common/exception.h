/**
 * @file exception.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_EXCEPTION_H_
#define NRS_NGX_CASPER_BROKER_CDN_EXCEPTION_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/exception.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
            
                class Exception: public std::exception, public ::cc::NonMovable
                {
                    
                public: // Const Data
                    
                    const uint16_t    code_;
                    
                private: // Const Data
                    
                    const std::string fallback_;
                    
                private: // Data
                    
                    std::string what_;
                    
                public: // Constructor(s) / Destructor
                    
                    Exception () = delete;
                    
                    /**
                     * @brief A constructor that provides the reason of the fault origin and it's message.
                     *
                     * @param a_code     HTTP Static Code.
                     * @param a_fallback HTTP Status Message ( to be used when fault message is empty ).
                     * @param a_what     Fault message.
                     */
                    Exception (const uint16_t a_code, const char* const a_fallback, const char* const a_what)
                        : code_(a_code), fallback_(a_fallback), what_(nullptr != a_what ? a_what : "")
                    {
                        /* empty */
                    }
                                        
                    /**
                     * @brief A constructor that provides the reason of the fault origin and constructs it's message from variable arguments.
                     *
                     * @param a_code     HTTP Static Code.
                     * @param a_fallback HTTP Status Message ( to be used when fault message is empty ).
                     * @param a_what     Fault message.
                     */
                    Exception (const uint16_t a_code, const char* const a_format, ...) __attribute__((format(printf, 3, 4)))
                        : code_(a_code)
                    {
                        va_list  args;
                        va_start(args, a_format);
                        SetWhat(a_format, args);
                        va_end(args);
                    }

                    /**
                     * @brief Copy Constructor.
                     *
                     * @param a_exception Exception to be copied.
                     */
                    Exception (const Exception& a_exception)
                        : code_(a_exception.code_), fallback_(a_exception.fallback_), what_(a_exception.what_)
                    {
                        /* empty */
                    }

                    
                    virtual ~Exception ()
                    {
                        /* empty */
                    }
                    
                public:
                    
                    virtual const char* what() const throw()
                    {
                        return ( what_.length() > 0 ? what_.c_str() : fallback_.c_str() );
                    }
                    
                    
                protected: // Method(s) / Function(s)
                    
                    inline void SetWhat (const char* const a_format, va_list a_args)
                    {
                        auto temp   = std::vector<char> {};
                        auto length = std::size_t { 512 };
                        std::va_list args;
                        while ( temp.size() <= length ) {
                            temp.resize(length + 1);
                            va_copy(args, a_args);
                            const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
                            va_end(args);
                            if ( status < 0 ) {
                                throw std::runtime_error {"string formatting error"};
                            }
                            length = static_cast<std::size_t>(status);
                        }
                        what_ = std::string { temp.data(), length };
                    }
                    
                };
                
                //
                // 500
                //
                class InternalServerError final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    InternalServerError (const char* const a_what = nullptr)
                        : Exception(500, "500 - Internal Server Error", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~InternalServerError ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // 501
                //
                class NotImplemented final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    NotImplemented (const char* const a_what = nullptr)
                        : Exception(501, "501 - Not Implemented", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~NotImplemented ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // 400
                //
                class BadRequest final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    BadRequest (const char* const a_format, ...)  __attribute__((format(printf, 2, 3)))
                        : Exception(403, "403 - Bad Request", nullptr)
                    {
                        va_list  args;
                        va_start(args, a_format);
                        SetWhat(a_format, args);
                        va_end(args);
                    }
                    
                    virtual ~BadRequest ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // 404
                //
                class NotFound final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    NotFound (const char* const a_what = nullptr)
                        : Exception(404, "404 - Not Found", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~NotFound ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // 403
                //
                class Forbidden final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    Forbidden (const char* const a_what = nullptr)
                        : Exception(403, "403 - Forbidden", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~Forbidden ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // 405
                //
                class MethodNotAllowed final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    MethodNotAllowed (const char* const a_what = nullptr)
                        : Exception(405, "405 - Method Not Allowed", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~MethodNotAllowed ()
                    {
                        /* empty */
                    }
                    
                };
                
                
                //
                // 409
                //
                class Conflict final : public Exception
                {
                    
                public: // Constructor(s) / Destructor
                    
                    Conflict (const char* const a_what = nullptr)
                        : Exception(409, "409 - Conflict", a_what)
                    {
                        /* empty */
                    }
                    
                    virtual ~Conflict ()
                    {
                        /* empty */
                    }
                    
                };

            } // end of namespace 'api'
        
        } // end of namespace 'broker'
    
    } // end of namespace 'casper'

} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_EXCEPTION_H_
