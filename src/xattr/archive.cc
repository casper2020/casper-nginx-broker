/**
 * @file archive.cc
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

#include "xattr/archive.h"

#include "cc/exception.h"
#include "cc/fs/dir.h"

#include "ngx/casper/broker/cdn-common/types.h"
#include "ngx/casper/broker/cdn-common/archive.h"

#include <algorithm>  // std::transform

/**
 * @brief Default constructor.
 *
 * @param a_archivist The one that requestes to archive a file.
 * @param a_writer    The one that wrote the archive.
 */
nb::Archive::Archive (const std::string& a_archivist, const std::string& a_writer)
    : archivist_(a_archivist), writer_(a_writer)
{
    xattr_ = nullptr;
    act_   = nullptr;
}

/**
 * @brief Destructor.
 */
nb::Archive::~Archive ()
{
    if ( nullptr != xattr_ ) {
        delete xattr_;
    }
    if ( nullptr != act_ ) {
        delete act_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Opens an existing archive for r/w xattrs.
 *
 * @param a_args Arguments.
 */
void nb::Archive::Open (const nb::Archive::OpenArgs& a_args)
{
    uri_ = a_args.uri_;
    if ( nullptr != xattr_ ) {
        delete xattr_;
    }
    xattr_ = new cc::fs::file::XAttr(uri_);
    if ( nullptr != act_ ) {
        delete act_;
        act_ = nullptr;
    }
    if ( true == a_args.edition_ && a_args.config_file_uri_.length() > 0 ) {
        ngx::casper::broker::cdn::Archive::LoadH2EMap   (a_args.config_file_uri_, h2e_map_);
        ngx::casper::broker::cdn::Archive::LoadACTConfig(a_args.config_file_uri_, act_config_);
        act_ = new ngx::casper::broker::cdn::ACT(act_config_, act_headers_);
    }
}

/**
 * @brief Create an empty file with a specifc URI.
 *
 * @param a_args Arguments.
 */
void nb::Archive::Create (const nb::Archive::CreateArgs& a_args)
{
    if ( nullptr != xattr_ ) {
        delete xattr_;
        xattr_ = nullptr;
    }

    Json::Value                        act_config;
    std::map<std::string, std::string> headers;
    ngx::casper::broker::cdn::H2EMap   h2e_map;
    
    
    for ( auto it : a_args.headers_ ) {
        const auto p = it.find(':');
        std::string key = it.substr(0, p);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        
        std::string value = it.substr(p + sizeof(char));
        
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](char& ch)->bool { return !isspace(ch); }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [](char& ch)->bool { return !isspace(ch); }).base(),value.end());
        
        headers[key] = value;
    }
    
    ngx::casper::broker::cdn::XNumericID   numeric_id("X-NUMERIC-ID");
    ngx::casper::broker::cdn::XFilename    filename;
    ngx::casper::broker::cdn::XContentType content_type("application/octet-stream");
    ngx::casper::broker::cdn::XAccess      access;
    ngx::casper::broker::cdn::XArchivedBy  archived_by;
    ngx::casper::broker::cdn::XBillingID   billing_id;
    ngx::casper::broker::cdn::XBillingType billing_type;
    
    archived_by.Set({"X-CASPER-ARCHIVED-BY", "USER-AGENT"}, headers);

    ngx::casper::broker::cdn::Archive::LoadH2EMap   (a_args.config_file_uri_, h2e_map);
    ngx::casper::broker::cdn::Archive::LoadACTConfig(a_args.config_file_uri_, act_config);
    
    ngx::casper::broker::cdn::Archive a (/* a_archivist */ archivist_, /* a_writer */ writer_,
                                         /* a_act_config */ act_config,
                                         /* a_headers    */ headers, /* a_h2e_map */ h2e_map,
                                         /* a_dir_prefix */ a_args.dir_prefix_
    );
    
    numeric_id.Set({"X-CASPER-ENTITY-ID", "X-CASPER-USER-ID"}, headers);
    filename.Set(headers);
    access.Set(headers);
    billing_id.Set(headers);
    billing_type.Set(headers);

    a.Create(/* a_id */ numeric_id, /* a_size */ 0, /* a_reserved_id */ &a_args.reserved_id_);
    
    std::map<std::string, std::string> attrs = {
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"            , a_args.reserved_id_              },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename"      , (const std::string&)filename     },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.archived.by"   , (const std::string&)archived_by  },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type"  , (const std::string&)content_type },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length", "0"                              },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr", (const std::string&)access       },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.id"    , (const std::string&)billing_id   },
        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.type"  , (const std::string&)billing_type }
    };

    ngx::casper::broker::cdn::Archive::RInfo r_info;
    
    a.Close(attrs, /* a_preserving */ {}, r_info);
    
    (void)r_info;
}

/**
 * @brief Validate this a archive and it's extended attribute.
 *
 * @param a_args Arguments.
 */
void nb::Archive::Validate (const nb::Archive::ValidateArgs& a_args)
{
    bool             eof;
    unsigned char    buffer [1024];
    std::string      tmp;
    
    std::string      tmp_id;
    size_t           tmp_size;
    std::string      tmp_md5;
    std::string      magic;
    std::string      tmp_replication_tag;
    bool             tmp_upload = false;
    
    uri_ = a_args.uri_;
    if ( nullptr != xattr_ ) {
        delete xattr_;
    }
    xattr_ = new cc::fs::file::XAttr(uri_);
    
    //
    // ... is a 'regular' archive?
    //
    std::string attr_prefix;
    if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.id") ) {
        attr_prefix = XATTR_ARCHIVE_PREFIX "com.cldware.archive";
    } else {
        // ... no, is a temporary uploaded file?
        if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.upload.id") ) {
            // ... yes ...
            attr_prefix = XATTR_ARCHIVE_PREFIX "com.cldware.upload";
            tmp_upload  = true;
        } else {
            // ... no ...
            throw cc::Exception("Invalid archive - no valid 'id' attribute found!");
        }
    }
    
    // ... common validation(s) ...
    xattr_->Get(attr_prefix + ".id" , tmp_id );
    xattr_->Get(attr_prefix + ".md5", tmp_md5);
    xattr_->Get(attr_prefix + ".content-length", tmp);
    if ( 1 != sscanf(tmp.c_str(), "%zd", &tmp_size) ) {
        throw cc::Exception("Size mismatch or invalid format got %s!", tmp.length() > 0 ? tmp.c_str() : "<empty>");
    }
    
    // ... calculate content md5 and validate size ...
    {
        ::cc::fs::File file;
        file.Open(uri_, cc::fs::File::Mode::Read);

        ::cc::hash::MD5  md5;
        md5.Initialize();
        do {
            md5.Update(buffer, file.Read(buffer, 1024, eof));
        } while ( false == eof );
        if ( 0 != tmp_md5.compare(md5.Finalize()) ) {
            throw cc::Exception("MD5 mismatch!");
        }
        
        // ... and compare it's size ...
        if ( file.Size() != static_cast<uint64_t>(tmp_size) ) {
            throw cc::Exception("Size mismatch!");
        }
        
        // ... file no longer needs to be open
        file.Close();
    }
    
    // ... id check ...
    {
        std::string tmp_name;
        cc::fs::File::Name(uri_, tmp_name);

        if ( false == tmp_upload ) {
            // ... ensure valid id ...
            if ( tmp_id.size() < 9 ) {
                throw cc::Exception("ID invalid!");
            }
            // ... ensure it matches ...
	    const char* id_start_ptr = tmp_id.c_str() + ( sizeof(char) * 3 );
	    if ( 0 != strcmp(id_start_ptr, tmp_name.c_str()) ) {
	      throw cc::Exception("ID mismatch: expected %s, got %s!", id_start_ptr, tmp_name.c_str());
	    }
        } else {
            // ... compare id to name - should be the same ...
            if ( 0 != tmp_id.compare(tmp_name) ) {
                throw cc::Exception("ID mismatch!");
            }
        }
    }
    
    //
    // SEAL(s) check(s)
    //
    
    // ...is a 'regular' archive?
    if ( false == tmp_upload ) {
        // ... yes, standard check applies ...
        // ... grab 'archivist' ...
        if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist") ) {
            xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", magic);
        } else {
            magic = archivist_;
        }
        // ... validate all except replication attributes ...
        xattr_->Validate(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                         reinterpret_cast<const unsigned char*>(magic.c_str()), magic.length(),
                         &ngx::casper::broker::cdn::Archive::sk_validation_excluded_attrs_
        );
        // ... validate only replication attributes ...
        if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag") ) {
            xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag", tmp_replication_tag);
            xattr_->Validate(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.seal",
                         {
                            { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag" },
                            { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.data" }
                         },
                         reinterpret_cast<const unsigned char*>(tmp_replication_tag.c_str()), tmp_replication_tag.length()
            );
        }
    } else {
        // ... no, special check applies ...
        // ... grab 'creator' ...
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.upload.created.by", magic);
        xattr_->Validate(XATTR_ARCHIVE_PREFIX "com.cldware.upload.seal",
                         reinterpret_cast<const unsigned char*>(magic.c_str()), magic.length(),
                         /* a_excluding_attrs */ nullptr
        );
    }
}

/**
 * @brief Update this a archive integrity check extended attributes.
 *
 * @param a_args Arguments.
 */
void nb::Archive::Update (const nb::Archive::UpdateArgs& a_args)
{
    ::cc::fs::File                     file;
    ::cc::hash::MD5                    md5;
    bool                               eof;
    unsigned char                      buffer [1024];
    std::string                        tmp_archivist;
    std::map<std::string, std::string> attrs;

    uri_ = a_args.uri_;
    if ( nullptr != xattr_ ) {
        delete xattr_;
    }

    // ... pre-update validation ...
    xattr_ = new cc::fs::file::XAttr(uri_);
    if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist") ) {
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", tmp_archivist);
    } else {
        tmp_archivist = archivist_;
    }
    xattr_->Validate(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                     reinterpret_cast<const unsigned char*>(tmp_archivist.c_str()), tmp_archivist.length(),
                     &ngx::casper::broker::cdn::Archive::sk_validation_excluded_attrs_);
    
    // ... open file update content-length and md5 attributes ..
    file.Open(uri_, cc::fs::File::Mode::Read);
    
    // ... content-length ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length"] = std::to_string(file.Size());
    
    // ... MD5 ...
    md5.Initialize();
    do {
        md5.Update(buffer, file.Read(buffer, 1024, eof));
    } while ( false == eof );
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5"] = md5.Finalize();

    // ... update xattrs modification info ....
    if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count") ) {
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count", tmp_);
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = std::to_string((std::strtoull(tmp_.c_str(), nullptr, 0) + 1));
    } else {
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = "1";
    }
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.by"] = writer_;
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.at"] = cc::UTCTime::NowISO8601WithTZ();
    
    // ... set xattrs ...
    for ( auto it : attrs ) {
        xattr_->Set(it.first, it.second);
    }
    
    // ... seal xattrs ...
    xattr_->Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                 reinterpret_cast<const unsigned char*>(tmp_archivist.c_str()), tmp_archivist.length(),
                 &ngx::casper::broker::cdn::Archive::sk_validation_excluded_attrs_
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Set a extended attribute value and re-seal it.
 *
 * @param a_name  Attribute name.
 * @param a_value Attribute value.
 */
void nb::Archive::SetXAttr (const std::string& a_name, const std::string& a_value)
{
    SetXAttrs({{ a_name, a_value }});
}

/**
* @brief Set a set of extended attributes and re-seal it.
*
* @param a_attrs Map of attributes to set.
*/
void nb::Archive::SetXAttrs (const std::map<std::string,std::string>& a_attrs)
{
    std::map<std::string, std::string> attrs;
        
    const auto it = a_attrs.find(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr");
    if ( a_attrs.end() != it ) {
       if ( nullptr == act_ ) {
           throw cc::Exception("Can't set xattrs - no ACT config provided!");
       }
       for ( auto it2 : { XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.r", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.w", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.d" } ) {
           if ( true == xattr_->Exists(it2) ) {
               xattr_->Remove(it2);
           }
       }
        act_->Compile(it->second, [this] (const std::string& a_key, const std::string& a_value) {
            for ( auto ch : a_key ) {
                xattr_->Set(std::string(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.") + ch, a_value);
            }
        });
   }
   
    for ( auto it : a_attrs ) {
        xattr_->Set(it.first, it.second);
    }
    
    if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.by") ) {
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.by"] = writer_;
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.at"] = cc::UTCTime::NowISO8601WithTZ();
    } else {
        if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count") ) {
            xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count", tmp_);
            attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = std::to_string((std::strtoull(tmp_.c_str(), nullptr, 0) + 1));
        } else {
            attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = "1";
        }
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.by"] = writer_;
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.at"] = cc::UTCTime::NowISO8601WithTZ();
    }
    for ( auto it : attrs ) {
        xattr_->Set(it.first, it.second);
    }
    
    if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist") ) {
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", tmp_);
    } else {
        tmp_ = archivist_;
    }
    xattr_->Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                 reinterpret_cast<const unsigned char*>(tmp_.c_str()), tmp_.length(),
                 &ngx::casper::broker::cdn::Archive::sk_validation_excluded_attrs_
    );
    
    for ( auto it : a_attrs ) {
        xattr_->Set(it.first, it.second);
    }
}

/**
 * @brief Retrieve an extended attribute value.
 *
 * @param a_name  Attribute name.
 * @param o_value Attribute value.
 */
void nb::Archive::GetXAttr (const std::string& a_name, std::string& o_value)
{
    xattr_->Get(a_name, o_value);
}

/**
 * @brief Retrieve any extended attribute(s) value(s) that match a regular expression.
 *
 * @param a_expr Attribute regular name expression.
 * @param o_map  Map of attributes where names match provided expression.
*/
void nb::Archive::GetXAttr (const std::regex& a_expr, std::map<std::string,std::string>& o_map)
{
    o_map.clear();
    xattr_->IterateOrdered(a_expr, [&o_map] (const char* const a_key, const char* const a_value) {
        o_map[a_key] = a_value;
    });
}

/**
 * @brief Remove a archive extended attribute value.
 *
 * @param a_name  Attribute name.
 * @param o_value Attribute value.
 */
void nb::Archive::RemoveXAttr (const std::string& a_name, std::string* o_value)
{
    xattr_->Remove(a_name, o_value);
    if ( 0 == a_name.compare(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr") ) {
        for ( auto ast_attr : { XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.r", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.w", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.d" } ) {
            if ( true == xattr_->Exists(ast_attr) ) {
                xattr_->Remove(ast_attr);
            }
        }
    }
}

/**
 * @brief Remove any extended attribute(s) value(s) that match a regular expression.
 *
 * @param a_expr Attribute regular name expression.
 * @param o_map  Map removed attributes where names match provided expression.
*/
void nb::Archive::RemoveXAttr (const std::regex& a_expr, std::map<std::string,std::string>& o_map)
{
    o_map.clear();
    xattr_->IterateOrdered(a_expr, [&o_map] (const char* const a_key, const char* const a_value) {
        o_map[a_key] = a_value;
    });
    for ( auto it : o_map ) {
        RemoveXAttr(it.first, nullptr);
    }
}

/**
 * @brief Iterate all extended attributes.
 *
 * @param a_callback Function to call for each attribute.
 */
void nb::Archive::ListXAttrs (const std::function<void(const char* const, const char* const)>& a_callback)
{
    xattr_->IterateOrdered(a_callback);
}

/**
 * @brief Iterate all extended attributes.
 *
 * @param a_expr Attribute regular name expression.
 * @param a_callback Function to call for each attribute.
 */
void nb::Archive::ListXAttrs (const std::regex& a_expr, const std::function<void(const char* const, const char* const)>& a_callback)
{
    xattr_->IterateOrdered(a_expr, a_callback);
}
