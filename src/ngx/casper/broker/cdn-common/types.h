/**
 * @file types.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_COMMON_TYPES_H_
#define NRS_NGX_CASPER_BROKER_CDN_COMMON_TYPES_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <map>
#include <string>
#include <algorithm> // std::find_if

#include "ngx/ngx_utils.h"

#include "cc/exception.h"
#include "cc/fs/file.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {

                //
                // XHeader
                //
                template <typename T> class Header : public ::cc::NonMovable
                {
                    
                public: // Data Type(s)
                    
                    typedef struct _KeyComparator {
                        
                        const std::string& value_;
                        _KeyComparator (const std::string& a_value)
                        : value_(a_value)
                        {}
                        bool operator() (const std::pair<std::string, std::string>& a_value) const
                        {
                            return ( 0 == strcasecmp(value_.c_str(), a_value.first.c_str()) );
                        }
                    } KeyComparator;
                    
                private: // Const Data
                    
                    const std::string name_;
                    
                public: // Const Data
                    
                    const T           default_;
                    
                protected: // Data
                    
                    std::string alt_name_;
                    T           value_;
                    std::string tmp_;
                    bool        set_;
                    
                protected: //
                    
                    std::function<ngx_int_t(const std::string&)> parse_function_;
                    
                public: // Constructor(s) / Destructor
                    
                    Header (const Header<T>& a_header)
                    : name_(a_header.name_), default_(a_header.default_),
                    alt_name_(a_header.alt_name_), value_(a_header.default_), tmp_(a_header.tmp_), set_(a_header.set_)
                    {
                        parse_function_ = nullptr;
                    }
                    
                    Header (const char* const a_name, const T& a_default)
                    : name_(a_name), default_(a_default), value_(a_default), set_(false)
                    {
                        /* empty */
                    }
                    
                    virtual ~Header ()
                    {
                        /* empty */
                    }
                    
                public: // Inline Method(s) / Function(s)
                    
                    bool               IsSet () const;
                    const std::string& Name  () const;
                    
                    void Set (const std::map<std::string,std::string>& a_headers, const T* a_default = nullptr);
                    void Set (const std::vector<std::string>& a_allowed, const std::map<std::string,std::string>& a_headers,
                              const T* a_default = nullptr, bool a_first_of = false);
                    
                public: // Operator(s)
                    
                    inline Header<T>& operator=(const Header<T>& a_header)
                    {
                        alt_name_ = a_header.alt_name_,
                        value_    = a_header.default_;
                        tmp_      = a_header.tmp_;
                        set_      = a_header.set_;
                        return *this;
                    }
                    
                    inline void operator=(const T& a_value)
                    {
                        value_ = a_value;
                        set_   = true;
                        tmp_   = "";
                    }
                    
                    inline operator const T& () const
                    {
                        return value_;
                    }
                    
                    inline operator T& ()
                    {
                        return value_;
                    }
                    
                    inline operator std::string () const
                    {
                        std::string tmp = std::to_string(value_);
                        return tmp;
                    }
                    
                protected:
                    
                    
                    static ngx_int_t       ParseKVHeader      (const std::string& a_header, const std::string& a_key, std::string& o_value);
                    static ngx_int_t       ParseKVHeader      (const std::string& a_header, const std::string& a_key, uint64_t& o_value);
#ifdef __APPLE__
                    static ngx_int_t       ParseKVHeader      (const std::string& a_header, const std::string& a_key, size_t& o_value);
#endif
                    static ngx_int_t       ParseKVHeader      (const std::string& a_header, const std::string& a_key, bool& o_value);
                    
                };
                
                template <typename T> bool Header<T>::IsSet () const
                {
                    return set_;
                }
                
                template <typename T> const std::string& Header<T>::Name () const
                {
                    return ( alt_name_.length() > 0 ? alt_name_ : name_ );
                }
                
                template <typename T> void Header<T>::Set (const std::map<std::string,std::string>& a_headers, const T* a_default)
                {
                    const auto it = std::find_if(a_headers.begin(), a_headers.end(), KeyComparator(name_));
                    if ( a_headers.end() == it ) {
                        if ( nullptr == a_default ) {
                            throw ::cc::Exception("Can't set header '%s' value - not found in provided map!", Name().c_str());
                        } else {
                            value_ = (*a_default);
                        }
                    } else {
                        const std::string tmp = (it->first + ": " + it->second);
                        if ( nullptr != parse_function_ ) {
                            if ( NGX_OK != parse_function_(tmp) ) {
                                if ( nullptr == a_default ) {
                                    throw ::cc::Exception("Can't set header '%s' value - invalid value!", Name().c_str());
                                } else {
                                    value_ = (*a_default);
                                }
                            }
                        } else if ( NGX_OK != ParseKVHeader(tmp, it->first, value_) ) {
                            if ( nullptr == a_default ) {
                                throw ::cc::Exception("Can't set header '%s' value - invalid value!", Name().c_str());
                            } else {
                                value_ = (*a_default);
                            }
                        }
                    }
                    set_ = true;
                }
                
                template <typename T> void Header<T>::Set (const std::vector<std::string>& a_allowed, const std::map<std::string,std::string>& a_headers,
                                                           const T* a_default, bool a_first_of)
                {
                    alt_name_ = "";
                    if ( 0 == a_allowed.size() ) {
                        throw ::cc::Exception("Can't set header '%s' value - zero 'allowed' headers provided!", Name().c_str());
                    }
                    if ( 0 == a_headers.size() ) {
                        if ( nullptr == a_default ) {
                            throw ::cc::Exception("Can't set header '%s' value - zero 'headers' key / value provided!", Name().c_str());
                        } else {
                            value_ = (*a_default);
                            set_   = true;
                        }
                        return;
                    }
                    size_t cnt = 0;
                    for ( auto name : a_allowed ) {
                        const auto it = std::find_if(a_headers.begin(), a_headers.end(), KeyComparator(name));
                        if ( a_headers.end() == it ) {
                            continue;
                        }
                        const std::string tmp = (it->first + ": " + it->second);
                        if ( 0 == cnt ) {
                            if ( nullptr != parse_function_ ) {
                                if (  NGX_OK != parse_function_(tmp) ) {
                                    throw ::cc::Exception("Can't set header '%s' value - invalid value!", Name().c_str());
                                }
                            } else if ( NGX_OK != ParseKVHeader(tmp, it->first, value_) ) {
                                throw ::cc::Exception("Can't set header '%s' value - invalid value!", Name().c_str());
                            }
                            alt_name_ = name;
                            if ( true == a_first_of ) {
                                cnt++;
                                break;
                            }
                        }
                        cnt++;
                    }
                    if ( cnt != 1 ) {
                        if ( 0 == cnt && nullptr != a_default ) {
                            value_ = (*a_default);
                            set_   = true;
                        } else {
                            std::stringstream ss; ss << a_allowed[0];
                            for ( size_t idx = 1 ; idx < a_allowed.size() ; ++idx  ) {
                                ss << ", " << a_allowed[idx];
                            }
                            if ( 0 == cnt ) {
                                throw ::cc::Exception("Missing or invalid header '%s' for 'one of' [%s]!", Name().c_str(), ss.str().c_str());
                            } else {
                                if ( false == a_first_of ) {
                                    throw ::cc::Exception("Multiple headers provided, accepting only 'one of' [%s]!", ss.str().c_str());
                                }
                            }
                        }
                    } else {
                        set_ = true;
                    }
                }
                
                //
                // VARIABLE
                //
                template <typename T, class V> class Variable : public ::cc::NonCopyable, public ::cc::NonMovable
                {
                    
                public: // Const Data
                    
                    const std::string name_;
                    const T&          default_;
                    const bool        can_default_;
                    
                private: // Data
                    
                    V value_;
                    
                public: // Constructor(s) / Destructor
                    
                    Variable () = delete;
                    Variable (const char* const a_name, const T& a_default, const bool a_can_default)
                    : name_(a_name), default_(a_default), can_default_(a_can_default), value_(a_name, a_default)
                    {
                        /* empty */
                    }
                    
                    virtual ~Variable ()
                    {
                        /* empty */
                    }
                    
                public: // Inline Method(s) / Function(s)
                    
                    inline V& value ()
                    {
                        return value_;
                    }
                    
                }; // end of class 'Variable'
                
//                //
//                //
//                //
//                template <class V> class XAttrVariable : public ::cc::NonCopyable, public ::cc::NonMovable
//                {
//
//                public: // Const Data
//
//                    const std::string name_;
//
//                private: // Refs
//
//                    V& value_;
//
//                public: // Constructor(s) / Destructor
//
//                    XAttrVariable () = delete;
//                    XAttrVariable (const char* const a_name, V& a_value)
//                    : name_(a_name), value_(a_value)
//                    {
//                        /* empty */
//                    }
//
//                    virtual ~XAttrVariable ()
//                    {
//                        /* empty */
//                    }
//
//                public: // Inline Method(s) / Function(s)
//
//                    inline V& value ()
//                    {
//                        return value_;
//                    }
//
//                }; // end of class 'XAttrVariable'
                
                //
                // StringHeader
                //
                class StringHeader : public Header<std::string>, public ::cc::NonCopyable
                {
                    
                public: // Constructor(s) / Destructor
                    
                    StringHeader (const char* const a_name, const std::string& a_default)
                    : Header(a_name, a_default)
                    {
                        /* empty */
                    }
                    
                    virtual ~StringHeader ()
                    {
                        /* empty */
                    }
                    
                public: // Operator(s)
                    
                    inline bool operator == (const std::string& a_value) const
                    {
                        return ( 0 == value_.compare(a_value) );
                    }
                    
                    inline bool operator != (const std::string& a_value) const
                    {
                        return ( 0 != value_.compare(a_value) );
                    }                              
                    
                };
                
                //
                // ContentDisposition
                //
                class ContentDisposition final : public StringHeader
                {
                    
                private: // Data
                    
                    std::string field_name_;
                    std::string filename_;
                    
                public: // Constructor(s) / Destructor
                    
                    ContentDisposition (const char* const a_name = "Content-Disposition", const std::string& a_default = "inline")
                        : StringHeader(a_name, a_default)
                    {
                        parse_function_ = [this] (const std::string& a_header) -> ngx_int_t {
                            const ngx_int_t rv = ngx::utls::nrs_ngx_parse_content_disposition(a_header, value_, field_name_, filename_);
                            if ( NGX_OK == rv ) {
                                if ( 0 != filename_.length() ) {
                                    tmp_ = value_ + "; filename=\"" + filename_ + "\"";
                                } else {
                                    tmp_ = value_;
                                }
                            }
                            return rv;
                        };
                    }
                    
                    virtual ~ContentDisposition ()
                    {
                        /* empty */
                    }
                    
                public: // Operator(s)
                    
                    inline operator const std::string& () const
                    {
                        return tmp_;
                    }
                    
                    inline void operator=(const std::map<std::string, std::string>& a_map)
                    {
                        const std::map<std::string, std::string*> map = {
                            { "content-disposition", &value_ },
                            { "filename"           , &filename_   }
                        };
                        for ( auto it : map ) {
                            const auto param_it = a_map.find(it.first);
                            if ( a_map.end() != param_it ) {
                                (*it.second) = param_it->second;;
                            }
                        }
                        if ( 0 != filename_.length() ) {
                            tmp_ = value_ + "; filename=\"" + filename_ + "\"";
                        } else {
                            tmp_ = value_;
                        }
                        set_ = true;
                    }
                    
                public: // Method(s) / Function(s)

                    inline const std::string& filename () const
                    {
                        return filename_;
                    }

                    inline const std::string& disposition () const
                    {
                        return value_;
                    }
                    
                    inline void Validate () const
                    {
                        if ( not ( 0 == strcasecmp(value_.c_str(), "attachment") || 0 == strcasecmp(value_.c_str(), "inline") ) ) {
                            throw cc::Exception( "Missing, invalid or unsupported '%s' header!", Name().c_str());
                        }
                    }
                    
                };
                
                //
                // ContentType
                //
                class ContentType final : public StringHeader
                {
                    
                private: // Data
                    
                    std::string charset_;
                    
                public: // Constructor(s) / Destructor
                    
                    ContentType (const char* const a_default)
                    : StringHeader("Content-Type", a_default)
                    {
                        parse_function_ = [this] (const std::string& a_header) -> ngx_int_t {
                            const ngx_int_t rv = ngx::utls::nrs_ngx_parse_content_type(a_header, value_, charset_);
                            if ( NGX_OK == rv ) {
                                if ( 0 != charset_.length() ) {
                                    tmp_ = value_ + "; charset=" + charset_;
                                } else {
                                    tmp_ = value_;
                                }
                            }
                            return rv;
                        };
                    }
                    
                    virtual ~ContentType ()
                    {
                        /* empty */
                    }    
                    
                public: // Operator(s)
                   
                    inline operator const std::string& () const
                    {
                        return tmp_;
                    }
                    
                };
                
                //
                // ContentLength
                //
                class ContentLength final : public Header<size_t>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    ContentLength ()
                    : Header("Content-Length", 0)
                    {
                        parse_function_ = [this] (const std::string& a_header) -> ngx_int_t {
                            return ngx::utls::nrs_ngx_parse_content_length(a_header, value_);
                        };
                    }
                    
                    virtual ~ContentLength ()
                    {
                        /* empty */
                    }
                    
                public: // Method(s) / Function(s)
                    
                    inline const std::string& value ()
                    {
                        tmp_ = std::to_string(value_);
                        return tmp_;
                    }
                    
                    inline std::string value () const
                    {
                        return std::to_string(value_);
                    }
                    
                };
                
                //
                // XHeader
                //
                template <typename T> class XHeader : public Header<T>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XHeader (const char* const a_name, const T& a_default)
                    : Header<T>(a_name, a_default)
                    {
                        /* empty */
                    }
                    
                    virtual ~XHeader ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // XContentDisposition
                //
                class XContentDisposition final : public StringHeader
                {
                    
                private: // Data
                    
                    std::string field_name_;
                    std::string filename_;
                    
                public: // Constructor(s) / Destructor
                    
                    XContentDisposition (const std::string& a_default)
                    : StringHeader("X-CASPER-CONTENT-DISPOSITION", a_default)
                    {
                        parse_function_ = [this] (const std::string& a_header) -> ngx_int_t {
                            const ngx_int_t rv = ngx::utls::nrs_ngx_parse_content_disposition(a_header, value_, field_name_, filename_);
                            if ( NGX_OK == rv ) {
                                if ( 0 != filename_.length() ) {
                                    tmp_ = value_ + "; filename=\"" + filename_ + "\"";
                                } else {
                                    tmp_ = value_;
                                }
                            }
                            return rv;
                        };
                    }
                    
                    virtual ~XContentDisposition ()
                    {
                        /* empty */
                    }
                    
                public: // Method(s) / Function(s)
                                        
                    inline const std::string& filename () const
                    {
                        return filename_;
                    }
                    
                    inline const std::string& disposition () const
                    {
                        return value_;
                    }
           
                    inline void operator=(const std::map<std::string, std::string>& a_map)
                    {
                        const std::map<std::string, std::string*> map = {
                            { "content-disposition", &value_ },
                            { "filename"           , &filename_   }
                        };
                        for ( auto it : map ) {
                            const auto param_it = a_map.find(it.first);
                            if ( a_map.end() != param_it ) {
                                (*it.second) = param_it->second;;
                            }
                        }
                        if ( 0 != filename_.length() ) {
                            tmp_ = value_ + "; filename=\"" + filename_ + "\"";
                        } else {
                            tmp_ = value_;
                        }
                        set_ = true;
                    }
                    
                    inline const std::string& value ()
                    {
                        if ( 0 != filename_.length() && 0 == strcasecmp(value_.c_str(), "attachment")  ) {
                            tmp_ = value_ + "; filename=\"" + filename_ + "\"";
                        } else {
                            tmp_ = value_;
                        }
                        return tmp_;
                    }
                    
                };
                
                //
                // XContentType
                //
                class XContentType final : public StringHeader
                {
                    
                private: // Data
                    
                    std::string charset_;
                    
                public: // Constructor(s) / Destructor
                    
                    XContentType (const char* const a_default)
                    : StringHeader("X-CASPER-CONTENT-TYPE", a_default)
                    {
                        /* empty */
                    }
                    
                    virtual ~XContentType ()
                    {
                        /* empty */
                    }
                    
                public: // Method(s) / Function(s)
                    
                    inline const std::string& value ()
                    {
                        if ( 0 != charset_.length() ) {
                            tmp_ = value_ + "; charset=" + charset_;
                        } else {
                            tmp_ = value_;
                        }
                        return tmp_;
                    }
                    
                };
                
                //
                // XAccess
                //
                class XAccess final : public StringHeader
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XAccess (const char* const a_name = "X-CASPER-ACCESS")
                    : StringHeader(a_name, "")
                    {
                        /* empty */
                    }
                    
                    virtual ~XAccess ()
                    {
                        /* empty */
                    }
                };
                
                //
                // XFilename
                //
                class XFilename final : public StringHeader
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XFilename (const char* const a_name = "X-CASPER-FILENAME")
                    : StringHeader(a_name, "")
                    {
                        /* empty */
                    }
                    
                    virtual ~XFilename ()
                    {
                        /* empty */
                    }
                    
                    inline const std::string& value ()
                    {
                        return value_;
                    }
                    
                };
                
                //
                // XArchivedBy
                //
                class XArchivedBy final : public StringHeader
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XArchivedBy (const char* const a_name = "X-CASPER-ARCHIVED-BY")
                    : StringHeader(a_name, "")
                    {
                        /* empty */
                    }
                    
                    virtual ~XArchivedBy ()
                    {
                        /* empty */
                    }
                    
                };
            
                //
                // XBillingID
                //
                class XBillingID final : public XHeader<uint64_t>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XBillingID (const char* const a_name = "X-CASPER-BILLING-ID")
                        : XHeader<uint64_t>(a_name, 0)
                    {
                        /* empty */
                    }
                    
                    virtual ~XBillingID ()
                    {
                        /* empty */
                    }
                    
                };
            
                //
                // XBillingType
                //
                class XBillingType final : public StringHeader
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XBillingType (const char* const a_name = "X-CASPER-BILLING-TYPE")
                        : StringHeader(a_name, "")
                    {
                        /* empty */
                    }
                    
                    virtual ~XBillingType ()
                    {
                        /* empty */
                    }
                    
                };
            
                //
                // XValidateIntegrity
                //
                class XValidateIntegrity final : public XHeader<bool>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XValidateIntegrity (const char* const a_name = "X-CASPER-VALIDATE-INTEGRITY")
                        : XHeader<bool>(a_name, false)
                    {
                        /* empty */
                    }
                    
                    virtual ~XValidateIntegrity ()
                    {
                        /* empty */
                    }
                };
                
                //
                // XStringID
                //
                class XStringID final : public StringHeader
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XStringID (const char* const a_name)
                    : StringHeader(a_name, "")
                    {
                        /* empty */
                    }
                    
                    virtual ~XStringID ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // XNumber
                //
                class XNumber : public XHeader<uint64_t>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XNumber (const char* const a_name, const uint64_t& a_default)
                    : XHeader(a_name, a_default)
                    {
                        /* empty */
                    }
                    
                    virtual ~XNumber ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // XNumericID
                //
                class XNumericID final : public XNumber
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XNumericID (const char* const a_name)
                    : XNumber(a_name, 0)
                    {
                        /* empty */
                    }
                    
                    virtual ~XNumericID ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // XHexNumber
                //
                class XHexNumber final : public XNumber
                {
                    
                public: // Constructor(s) / Destructor
                    
                    XHexNumber (const char* const a_name)
                    : XNumber(a_name, 0)
                    {
                        /* empty */
                    }
                    
                    virtual ~XHexNumber ()
                    {
                        /* empty */
                    }
                    
                };
                
                //
                // TYPE DEF(S)
                //
                typedef std::map<std::string, std::string> H2EMap;
                
            } // end of namespace 'api'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#include "ngx/casper/broker/cdn-common/types.cc"

#endif // NRS_NGX_CASPER_BROKER_CDN_COMMON_TYPES_H_
