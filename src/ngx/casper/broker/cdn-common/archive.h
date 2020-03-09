/**
 * @file archive.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_ARCHIVE_H_
#define NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_ARCHIVE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ngx/casper/broker/cdn-common/types.h"
#include "ngx/casper/broker/cdn-common/act.h"

#include "cc/bitwise_enum.h"
#include "cc/fs/file.h" // XAttr, Writer
#include "cc/hash/md5.h"

#include <set>
#include <map>
#include <string>

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                class Archive final : public ::cc::NonCopyable, public ::cc::NonMovable
                {
                    
                public: // Enum(s)
                    
                    enum class Mode : uint8_t {                        
                        NotSet   = 0x00,
                        Create   = 0x01,
                        Modify   = 0x02,
                        Patch    = 0x04,
                        Delete   = 0x08,
                        Move     = 0x10,
                        Read     = 0x20,
                        Validate = 0x40
                    };
                    
                public: // Data Type(s)
                    
                    typedef struct {
                        const char* const  name_;
                        const char* const  key_;
                        const unsigned int tag_;
                        const bool         optional_;
                        std::string        value_;
                        bool               serialize_;
                    } RAttr;
                    
                    typedef struct {
                        std::string        old_id_;
                        std::string        old_uri_;
                        std::string        new_id_;
                        std::string        new_uri_;
                        std::vector<RAttr> attrs_;
                    } RInfo;
                    
                    typedef struct {
                        size_t      validity_;
                        std::string directory_prefix_;
                    } QuarantineSettings;

                    typedef struct {
                        std::string temporary_prefix_;
                        std::string archive_prefix_;
                    } DirectoriesSettings;
                    
                    typedef struct {
                        DirectoriesSettings directories_;
                        QuarantineSettings  quarantine_;
                    } Settings;

                private: // Data Type(s)
                    
                    typedef struct {
                        std::string id_;
                        std::string path_;
                        std::string filename_;
                        std::string uri_;
                        std::string ext_;
                        uint64_t    size_;
                        std::string xhvn_;
                    } Local;
                    
                private: // Const Data
                    
                    const std::string                         archivist_;
                    const std::string                         writer_;

                private: // Const Refs
                    
                    const Json::Value&                        act_config_;
                    const std::map<std::string, std::string>& headers_;
                    const H2EMap&                             h2e_map_;
                    const std::string&                        dir_prefix_;
                    const std::string                         replicator_;

                private: // Data
                
                    Mode           mode_;
                    Local          local_;
                    uint64_t       bytes_written_;
                    unsigned char* buffer_;
                    std::string    tmp_;

                private: // Helper(s)
                    
                    ACT                           act_;
                    ::cc::fs::file::XAttr*        xattr_;
                    ::cc::fs::file::Writer        fw_;
                    ::cc::hash::MD5               md5_;

                public: // Static Const Data
                    
                    static const std::set<std::string> sk_validation_excluded_attrs_;
                    static const bool                  sk_content_modification_history_enabled_;

                public: // Constructor (s) / Destructor
                    
                    Archive (const std::string& a_archivist, const std::string& a_writer,
                             const Json::Value& a_act_config, const std::map<std::string, std::string>& a_headers, const H2EMap& a_h2e_map,
                             const std::string& a_dir_prefix, const std::string a_replicator = "");
                    virtual ~Archive ();
                    
                public: // Create Mode - Method(s) / Function(s)
                    
                    void   Create (const XNumericID& a_id, const size_t& a_size,
                                  const std::string* a_reserved_id = nullptr);
                    size_t Write  (const unsigned char* const a_data, const size_t& a_size);
                    void   Flush  ();
                    void   Close  (const std::map<std::string, std::string>& a_attrs,
                                   const std::set<std::string>& a_preserving,
                                   RInfo& o_info);
                    void   Destroy ();
                    
                public: // Read Mode - Method(s) / Function(s)
                    
                    
                    void Open     (const std::string& a_path,
                                   const std::function<Mode(const std::string&)> a_callback = nullptr);
                    void Close    (RInfo* o_info);
                    void Close    (bool a_for_redirect, RInfo* o_info);
                    
                public: // Patch Mode - Method(s) / Function(s)
                    
                    void Patch     (const std::string& a_path,
                                    const std::map<std::string, std::string>& a_attrs,
                                    const std::set<std::string>& a_preserving,
                                    const bool a_backup,
                                    RInfo& o_info);
                    
                public: // Delete Mode - Method(s) / Function(s)
                    
                    void Delete     (const std::string& a_path,
                                     const QuarantineSettings* a_quarantine,
                                     RInfo& o_info);

                public: // Update Mode - Method(s) / Function(s)
                    
                    void Update   (const std::string& a_path,
                                   const std::map<std::string, std::string>& a_attrs,
                                   const std::set<std::string>& a_preserving,
                                   const bool a_backup,
                                   const XArchivedBy& a_by,
                                   RInfo& o_info);

                public: // Move Mode - Method(s) / Function(s)
                    
                    void Move       (const std::string& a_uri,
                                     const std::map<std::string, std::string>& a_attrs,
                                     const std::set<std::string>& a_preserving,
                                     const bool a_backup,
                                     RInfo& o_info);
                    
                public: // Rename Helper - Method(s) / Function(s)
                
                    static void IDFromURLPath (const std::string& a_path,
                                               std::string& o_id, std::string* o_ext = nullptr);
                    static void Rename        (const std::string& a_from, const std::string& a_to);
                    
                public: // Validation - Method(s) / Function(s)
                    
                    void Validate   (const std::string& a_path,
                                     const std::string* a_md5 = nullptr, const std::string* a_id = nullptr,
                                     const std::map<std::string,std::string>* a_attrs = nullptr);

                public: // XAttr Method(s) / Function(s)
                    
                    bool HasXAttr      (const std::string& a_name) const;
                    void GetXAttr      (const std::string& a_name, uint64_t& o_value) const;
                    void GetXAttr      (const std::string& a_name, std::string& o_value) const;
                    void GetXAttrs     (std::map<std::string, std::string>& o_attrs) const;
                    
                    void IterateXAttrs (const std::function<void (const char *const, const char *const)>& a_callback) const;
                    
                private: // XAttr Method(s) / Function(s)
                    
                    void SetXAttrs     (const std::map<std::string, std::string>& a_attrs,
                                        const std::set<std::string>& a_preserving,
                                        const std::set<std::string>& a_excluding,
                                        bool a_trusted, bool a_seal);
                    
                    void Fill          (bool a_trusted, RInfo& o_info);
                    
                private: // Static Method(s) / Function(s)
                    
                    static void Rename (const std::string& a_from, const std::string& a_to,
                                        const bool a_backup,
                                        RInfo* o_info = nullptr);

                    static void Copy    (const std::string& a_from, const std::string& a_to, const bool a_overwrite,
                                         std::vector<RAttr>* o_attrs);
                    
                private: // Static Method(s) / Function(s)
                    
                    static void SetContentModificationAttrs (const Archive& a_archive, const XArchivedBy& a_by,
                                                             std::map<std::string, std::string>& o_attrs);
                    static void SetLocalInfoFromURLPath (const std::string& a_path,
                                                         const Archive& a_archive,
                                                         const bool a_append_to_history,
                                                         Local& o_local);

                public: // Static Method(s) / Function(s)
                    
                    static void LoadH2EMap    (const std::string& a_uri, H2EMap& o_map);
                    static void LoadH2EMap    (const char* const a_data, size_t a_length, H2EMap& o_map);
                    static void SetH2EMap     (const std::function<bool(Json::Reader&, Json::Value&)>& a_parse, H2EMap& o_map);
                    
                    static void LoadACTConfig (const std::string& a_uri, Json::Value& o_config);
                    static void LoadACTConfig (const char* const a_data, size_t a_length, Json::Value& o_config);
                    static void SetACTConfig  (const std::function<bool(Json::Reader&, Json::Value&)>& a_parse, Json::Value& o_config);

                public: // Inline Method(s() / Function(s)
                    
                    const std::string& id    () const;
                    const std::string& uri   () const;
                    const uint64_t&    size  () const;
                    std::string        name  (const std::string& a_suggested) const;
                    const Local&       local () const;
                    
                private: // Inline Method(s) / Function(s)
                    
                    void Reset ();
                    
                }; // end of class 'URI'
                                
                /**
                 * @return Archive ID.
                 */
                inline const std::string& Archive::id () const
                {
                    return local_.id_;
                }
                
                /**
                 * @return Local URI.
                 */
                inline const std::string& Archive::uri () const
                {
                    return local_.uri_;
                }
            
                /**
                 * @return Local URI.
                 */
                inline const uint64_t& Archive::size () const
                {
                    return local_.size_;
                }
            
                /**
                 * @brief Select a file name.
                 *
                 * @param a_suggested File name suggestion, "" if none.
                 *
                 * @return The selected filename, worst case scenario it's the archive id.
                 */
                inline std::string Archive::name (const std::string& a_suggested) const
                {
                    if ( 0 != a_suggested.length() ) {
                        return a_suggested;
                    }
                    std::string name;
                    if ( true == HasXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename") ) {
                        GetXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename", name);
                    }
                    if ( 0 == name.length() ) {
                        name = local_.id_;
                    }
                    return name;
                }
            
                /**
                 * @return Read-only access to \link Local \link.
                 */
                inline const Archive::Local& Archive::local () const
                {
                    return local_;
                }

                /**
                 * @brief Reset current context.
                 */
                inline void Archive::Reset ()
                {
                    mode_            = Mode::NotSet;
                    local_.id_       = "";
                    local_.path_     = "";
                    local_.filename_ = "";
                    local_.uri_      = "";
                    local_.ext_      = "";
                    bytes_written_  = 0;
                    if ( nullptr != xattr_ ) {
                        delete xattr_;
                        xattr_ = nullptr;
                    }
                }

                DEFINE_ENUM_WITH_BITWISE_OPERATORS(Archive::Mode);
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_ARCHIVE_H_
