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

#ifndef NRS_NGX_CASPER_BROKER_EXCEPTION_H_
#define NRS_NGX_CASPER_BROKER_EXCEPTION_H_

#include <exception> // std::exception
#include <string>    // std::string

#include <string>     // std::string
#include <cstdarg>    // va_start, va_end, std::va_list
#include <cstddef>    // std::size_t
#include <stdexcept>  // std::runtime_error
#include <vector>     // std::vector

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {

            class Exception final : public std::exception
            {

            protected: // Data

                std::string code_;
                std::string what_;

            public: // Constructor / Destructor
                
                /**
                 * @brief A constructor that provides the reason of the fault origin.
                 *
                 * @param a_code
                 * @param a_what
                 */
                Exception (const std::string a_code, const std::string& a_what)
                {
                    code_ = a_code;
                    what_ = a_what;
                }
                
                /**
                 * @brief A constructor that provides the reason of the fault origin.
                 *
                 * @param a_code
                 * @param a_format printf like format followed by a variable number of arguments.
                 * @param ...
                 */
                Exception (const std::string a_code, const char* const a_format, ...) __attribute__((format(printf, 3, 4)))
                {
                    auto temp   = std::vector<char> {};
                    auto length = std::size_t { 512 };
                    std::va_list args;
                    while ( temp.size() <= length ) {
                        temp.resize(length + 1);
                        va_start(args, a_format);
                        const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
                        va_end(args);
                        if ( status < 0 ) {
                            throw std::runtime_error {"string formatting error"};
                        }
                        length = static_cast<std::size_t>(status);
                    }
                    code_ = a_code;
                    what_ = std::string { temp.data(), length };
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~Exception ()
                {
                    /* empty */
                }
                
            public: // overrides
                
                /**
                 * @return An explanatory string.
                 */
                virtual const char* what() const throw()
                {
                    return what_.c_str();
                }
                
            public: // Method(s) / Function(s)
                
                /**
                 * @return This exceptio code.
                 */
                inline const char* const code() const
                {
                    return code_.c_str();
                }
                
            };

        } // end of namespace broker
        
    } // end of namespace casper
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_EXCEPTION_H_
