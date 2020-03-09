/**
 * @file archive.h
 *
 * Copyright (c) 2017-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_XATTR_ARCHIVE_H_
#define NRS_XATTR_ARCHIVE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/fs/file.h" // XAttr
#include "cc/utc_time.h"
#include "cc/hash/md5.h"

#include <set>
#include <regex>      // std::regex

#include "json/json.h"

#include "ngx/casper/broker/cdn-common/types.h"
#include "ngx/casper/broker/cdn-common/act.h"

namespace nb
{
    
    class Archive final : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Data Type(s)
        
        typedef struct {
            std::string id_;
            std::string uri_;
            bool        as_json_;
            std::string config_file_uri_;
            bool        edition_;
        } OpenArgs;
        
        typedef struct {
            std::string           reserved_id_;
            std::set<std::string> headers_;
            std::string           dir_prefix_;
            std::string           config_file_uri_;
        } CreateArgs;
        
        typedef struct : public OpenArgs {
        } ValidateArgs;
        
        typedef struct : public OpenArgs {
        } UpdateArgs;

    private: // Const Data

        const std::string archivist_;
        const std::string writer_;

    private: // Data
        
        std::string uri_;
        std::string tmp_;

    private: // Helper(s)
        
        cc::fs::file::XAttr*               xattr_;        
        ngx::casper::broker::cdn::H2EMap   h2e_map_;
        Json::Value                        act_config_;
        std::map<std::string, std::string> act_headers_;
        ngx::casper::broker::cdn::ACT*     act_;
        
    public: // Constructor(s) / Destructor
        
        Archive () = delete;
        Archive (const std::string& a_archivist, const std::string& a_writer);
        virtual ~Archive ();
        
    public: // Method(s) / Function(s)
        
        void Open        (const OpenArgs& a_args);
        void Create      (const CreateArgs& a_args);
        void Validate    (const ValidateArgs& a_args);
        void Update      (const UpdateArgs& a_args);

    public: // Method(s) / Function(s)
        
        void SetXAttr    (const std::string& a_name, const std::string& a_value);
        void SetXAttrs   (const std::map<std::string,std::string>& a_attrs);
        void GetXAttr    (const std::string& a_name, std::string& o_value);
        void GetXAttr    (const std::regex& a_expr, std::map<std::string,std::string>& o_map);
        void RemoveXAttr (const std::string& a_name, std::string* o_value);
        void RemoveXAttr (const std::regex& a_expr, std::map<std::string,std::string>& o_map);
        void ListXAttrs  (const std::function<void(const char* const, const char* const)>& a_callback);
        void ListXAttrs  (const std::regex& a_expr, const std::function<void(const char* const, const char* const)>& a_callback);
        
    }; // end of class 'Archive'
    
} // end of namespace 'nb'

#endif // NRS_XATTR_ARCHIVE_H_
