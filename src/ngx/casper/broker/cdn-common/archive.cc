/**
 * @file archive.cc
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

#include "ngx/casper/broker/cdn-common/archive.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#include "osal/osalite.h" // INT64_FMT_ZP

#include "cc/fs/dir.h"
#include "cc/fs/exception.h"
#include "cc/utc_time.h"

#include "cc/b64.h"

#include <fstream> // std::filebuf, std::istream
#include <regex>   // std::regex

#define THROW_INTERNAL_ERROR(a_msg) \
    throw ngx::casper::broker::cdn::InternalServerError(\
        ("@" + std::string(__FUNCTION__) + ": " + a_msg).c_str() \
    )

#define XATTR_WRITE_SANITY_CHECK_BARRIER() \
    if ( nullptr == xattr_ ) { \
        THROW_INTERNAL_ERROR("can't be called - URI to local file is not set!"); \
    } \
    if ( \
        ngx::casper::broker::cdn::Archive::Mode::NotSet \
            == \
        ( mode_ \
                & \
            ( \
                ngx::casper::broker::cdn::Archive::Mode::Create \
                    | \
                ngx::casper::broker::cdn::Archive::Mode::Modify \
                    | \
                ngx::casper::broker::cdn::Archive::Mode::Patch \
                    | \
                ngx::casper::broker::cdn::Archive::Mode::Move \
            ) \
        ) \
    ) { \
        THROW_INTERNAL_ERROR( \
            ("can't write xattr when archive is open in mode " + std::to_string(static_cast<typename std::underlying_type<Mode>::type>(mode_)) + "!").c_str() \
        ); \
    }

#define XATTR_READ_SANITY_CHECK_BARRIER() \
    if ( nullptr == xattr_ ) { \
        THROW_INTERNAL_ERROR("can't be called - URI to local file is not set!"); \
    } \
    if ( \
        ngx::casper::broker::cdn::Archive::Mode::NotSet == mode_ \
    ) { \
        THROW_INTERNAL_ERROR( \
            ("can't read xattr when archive is open in mode " + std::to_string(static_cast<typename std::underlying_type<Mode>::type>(mode_)) + "!").c_str() \
        ); \
    }

#define MODE_SANITY_CHECK_BARRIER(a_modes) \
    if ( ngx::casper::broker::cdn::Archive::Mode::NotSet == ( mode_ & a_modes ) ) { \
        THROW_INTERNAL_ERROR(("can't be called in mode " + std::to_string(static_cast<typename std::underlying_type<Mode>::type>(mode_)) + "!").c_str()); \
    }

#define ARCHIVE_PERMISSION_BARRIER(a_key) \
    act_.Setup(); \
    GetXAttr(a_key, tmp_); \
    if ( 0 == act_.Evaluate(tmp_) ) { \
        throw ngx::casper::broker::cdn::Forbidden();\
    }

#ifdef __APPLE__
#pragma mark - ARCHIVE
#endif

const std::set<std::string> ngx::casper::broker::cdn::Archive::sk_validation_excluded_attrs_ = {
    { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag"  },
    { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.data" },
    { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.seal" }
};

const bool ngx::casper::broker::cdn::Archive::sk_content_modification_history_enabled_ = false;

/**
 * @brief Default constructor.
 *
 * @param a_archivist
 * @param a_writer
 * @param a_act_config
 * @param a_headers
 * @param a_h2e_map    A reference to h2e map
 * @param a_dir_prefix A reference to directory prefix.
 * @param a_replicator Not empty if it's an authorized replicator.
 */
ngx::casper::broker::cdn::Archive::Archive (const std::string& a_archivist, const std::string& a_writer,
                                            const Json::Value& a_act_config,
                                            const std::map<std::string, std::string>& a_headers, const ngx::casper::broker::cdn::H2EMap& a_h2e_map,
                                            const std::string& a_dir_prefix,
                                            const std::string a_replicator)
    : archivist_(a_archivist), writer_(a_writer),
      act_config_(a_act_config), headers_(a_headers), h2e_map_(a_h2e_map), dir_prefix_(a_dir_prefix), replicator_(a_replicator),
      mode_(ngx::casper::broker::cdn::Archive::Mode::NotSet), bytes_written_(0), buffer_(nullptr),
      act_(act_config_, headers_),
      xattr_(nullptr)
{
    local_.size_ = 0;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::Archive::~Archive ()
{
    // ... if we're in create mode or move mode ...    
    if ( ngx::casper::broker::cdn::Archive::Mode::NotSet != ( mode_ & ( ngx::casper::broker::cdn::Archive::Mode::Create | ngx::casper::broker::cdn::Archive::Mode::Move ) ) ) {
        // ... and file as not properly closed ...
        if ( true == fw_.IsOpen() ) {
            // ... keep track of local uri ...
            const std::string local_uri = local_.uri_;
            // ... close it ...
            fw_.Close();
            // ... erase file ...
            if ( true == ::cc::fs::File::Exists(local_uri) ) {
                ::cc::fs::File::Erase(local_uri);
            }
        }
    }
    // ... release xattr
    if ( nullptr != xattr_ ) {
        delete xattr_;
    }
    // ... release buffer ...
    if ( nullptr != buffer_ ) {
        delete [] buffer_;
    }
}

#ifdef __APPLE__
#pragma mark - READ ONLY MODE
#endif

/**
 * @brief Open an pre-existing file.
 *
 * param a_path
 * param a_callback
 */
void ngx::casper::broker::cdn::Archive::Open (const std::string& a_path,
                                              const std::function<ngx::casper::broker::cdn::Archive::Mode(const std::string&)> a_callback)
{
    try {
        
        // ... set local_ struct data from URI path ...
        SetLocalInfoFromURLPath(a_path, *this, /* a_append_extension */ false, local_);
        
        // ... LEGACY FILES: check if file exists ...
        if ( local_.ext_.length() > 0 ) {
            // ... legacy file has extension ...
            std::string uri = local_.uri_;
            // ... if '.' is set ...
            if ( '.' == local_.ext_.c_str()[0] ) {
                // ... just append it ...
                uri += local_.ext_;
            } else {
                // ... append '.' and then extension ...
                uri += '.' + local_.ext_;
            }
            // ... if exists ...
            if ( true == ::cc::fs::File::Exists(uri) ) {
                // ... accept it ..
                local_.uri_  = uri;
                local_.size_ = ::cc::fs::File::Size(local_.uri_);
            } else if ( false == ::cc::fs::File::Exists(local_.uri_) ) {
                // ... legacy file does not exists, also the file without extension does not exist either ...
                throw ngx::casper::broker::cdn::NotFound();
            }
        } else if ( false == ::cc::fs::File::Exists(local_.uri_) ) {
            // ... not a legacy file ...
            throw ngx::casper::broker::cdn::NotFound();
        }

        // ... prepare xattrs helper ...
        xattr_ = new ::cc::fs::file::XAttr(local_.uri_);
        
        // ... open, just for testing purposes only ( exits and obtain xhvn )?
        ngx::casper::broker::cdn::Archive::Mode mode;
        if ( nullptr != a_callback ) {
            mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
            mode  = a_callback(local_.xhvn_);
            if ( ngx::casper::broker::cdn::Archive::Mode::NotSet == mode ) {
                Reset();
                return;
            }
        } else {
            mode = ngx::casper::broker::cdn::Archive::Mode::Read;
        }
        
        // ... set mode.
        mode_ = mode;

        switch(mode_) {
            case ngx::casper::broker::cdn::Archive::Mode::Modify:
            case ngx::casper::broker::cdn::Archive::Mode::Patch:
                // ... check 'Write' permissions ...
                ARCHIVE_PERMISSION_BARRIER(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.w");
                break;
            case ngx::casper::broker::cdn::Archive::Mode::Delete:
                // ... check 'Delete' permissions ...
                ARCHIVE_PERMISSION_BARRIER(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.d");
                break;
            case ngx::casper::broker::cdn::Archive::Mode::Read:
                // ... check 'READ' permissions ...
                ARCHIVE_PERMISSION_BARRIER(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.r");
                break;
            case ngx::casper::broker::cdn::Archive::Mode::Validate:
                break;
            default:
                throw ::cc::Exception("Unable to open file for mode %s - not supported!",
                                      std::to_string(static_cast<typename std::underlying_type<Mode>::type>(mode_)).c_str()
                );
        }
        
    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found_exception) {
        Reset();
        throw a_not_found_exception;
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
        Reset();
        throw a_forbidden;
    } catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request) {
        Reset();
        throw a_bad_request;
    } catch (const ::cc::Exception& a_cc_exception) {
        Reset();
        throw a_cc_exception;
    } catch (...) {
        Reset();
        CC_EXCEPTION_RETHROW(/* a_unhandled */ true);
    }
}

/**
 * @brief Closes a previously open file in read-only mode.
 *
 * @param o_info Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Close (ngx::casper::broker::cdn::Archive::RInfo* o_info)
{
    Close(/* a_for_redirect */ false, o_info);
}

/**
 * @brief Closes a previously open file in read-only mode.
 *
 * @param a_for_redirect When true id will append extension if needed.
 * @param o_info Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Close (bool a_for_redirect, ngx::casper::broker::cdn::Archive::RInfo* o_info)
{
    MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::Read);
    
    // ... set basic info ...
    if ( nullptr != o_info ) {
        Fill(/* a_trusted */ true, *o_info);
        if ( true == a_for_redirect ) {
            const char* l = strchr(o_info->new_id_.c_str(), '.');
            if ( nullptr == l && local().ext_.length() > 0 ) {
                o_info->new_id_ += '.' + local().ext_;
            }
        }
        
    }
    
    // ... reset for re-usage ...
    Reset();
}

#ifdef __APPLE__
#pragma mark - CREATE / WRITE MODE
#endif

/**
 * @brief Creates a new unique file based on a \link XNumericID \link.
 *
 * @param a_id          Numeric ID.
 * @param a_size        Total size ( in bytes ) to be written.
 * @param a_reserved_id Previously reserved id.
 */
void ngx::casper::broker::cdn::Archive::Create (const ngx::casper::broker::cdn::XNumericID& a_id, const size_t& a_size,
                                                const std::string* a_reserved_id)
{
    // ... this archive must not be in a middle of an operation ...
    if ( ngx::casper::broker::cdn::Archive::Mode::NotSet != mode_ ) {
        MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::NotSet);
    }
    
    try {
 
        // ... set common local info setup ...
        const auto it = h2e_map_.find(a_id.Name());
        if ( h2e_map_.end() == it ) {
            throw ::cc::Exception("Unable to setup archive writer: couldn't find '%s' directory component!", a_id.Name().c_str());
        }
        
        const uint64_t& value            = a_id;
        char            id_buffer   [19] = { '\0', '\0' };
        const size_t    max_id_bufer_len = ( sizeof(id_buffer) / sizeof(id_buffer[0] ) ) - ( sizeof(char) * 1 );
        
        const int written = snprintf(id_buffer, max_id_bufer_len, INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3) "/",
                                     (value % 1000000000000) / 1000000000,
                                     (value % 1000000000)    / 1000000   ,
                                     (value % 1000000)       / 1000      ,
                                     (value % 1000)
        );
        if ( written < 0 || written > 16 ) {
            throw ::cc::Exception("Unable to setup archive writer: couldn't prepare id - buffer write error!");
        }
        
        // ... id starts with a prefix ...
        local_.id_ = tolower(it->second[0]);
        
        // ... but make sure only one exists ...
        size_t prefix_count = false;
        for ( auto it2 : h2e_map_ ) {
            if ( it2.second[0] == local_.id_[0] ) {
                prefix_count += 1;
            }
        }
        if ( prefix_count > 1 ) {
            throw ngx::casper::broker::cdn::BadRequest("Multiple id prefix options provided, expecting only one!");
        }
        
        // ... a reserved id was provided?
        if ( nullptr != a_reserved_id ) {
            // ... yes, validated it ...
            const std::regex id_expr("([a-z]{1})([a-zA-Z]{2})([a-zA-Z0-9]{6})(\\..+)?", std::regex_constants::ECMAScript);
            std::smatch match;
            if ( false == std::regex_search((*a_reserved_id), match, id_expr) || match.size() < 5 ) {
                throw ::cc::Exception("Unable to setup archive writer: invalid or reserved id '%s'!", a_reserved_id->c_str());
            }
            // ... ensure valid prefix ...
            const char id_prefix = match[1].str()[0];
            if ( id_prefix != local_.id_[0] ) {
                throw ngx::casper::broker::cdn::BadRequest("Invalid id prefix!");
            }
            // ... grab id path component ...
            tmp_ = match[2].str();
            // ... append ( 2 chars ) path component to id ...
            id_buffer[16] = tmp_[0];
            id_buffer[17] = tmp_[1];
            id_buffer[18] = '\0';
            // ... set id ...
            local_.id_ += tmp_;
            // ... set path ...
            local_.path_     = ( dir_prefix_ + it->second + '/' + id_buffer + '/' );
            // ... set uri, name and ext ...
            local_.uri_      = local_.path_;
            local_.filename_ = match[3].str();
            local_.ext_      = match[4].str();
            // ... check if files does not exists ...
            tmp_ = ( local_.uri_ + local_.filename_ + local_.ext_ ) ;
            if ( true == ::cc::fs::File::Exists(tmp_) ) {
                throw ngx::casper::broker::cdn::Conflict("An archive with the same id already exists!");
            }
            // ... ensure directory exists ...
            ::cc::fs::Dir::Make(local_.path_.c_str());
            // ... open a reserved file in write mode ...
            fw_.Open(tmp_, ::cc::fs::File::Mode::Write);
        } else {
            // ... no, create file parent directory - generate random 2 chars for name ...
            static const char   dictionary[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            static const size_t limit        = (sizeof(dictionary) / sizeof(dictionary[0]) - 1);
            for ( size_t idx = 0; idx < ( sizeof(char) * 2 ) ; ++idx ) {
                id_buffer[written + idx] = dictionary[random() % limit];
                local_.id_ += ( id_buffer[written + idx] );
            }
            id_buffer[18] = '\0';
            // ... set path and uri ...
            local_.path_ = ( dir_prefix_ + it->second + "/" + id_buffer + "/" );
            local_.uri_  = local_.path_;
            // ... open a new unique file in write mode ...
            fw_.Open(local_.path_, /* a_prefix */ "", /* a_extension */ "", a_size);
        }
        
        // ... reset writer control variable(s) ...
        bytes_written_ = 0;
        
        // ... fetch file name ...
        cc::fs::File::Name(fw_.URI(), local_.filename_);
        
        // ... finalize local data setup ...
        local_.id_   += local_.filename_;
        // local_.ext_ is only used by 'reserved' id operation
        if ( nullptr != a_reserved_id && nullptr == strstr(local_.filename_.c_str(), local_.ext_.c_str()) ) {
            local_.uri_  += local_.filename_ + local_.ext_ ;
        } else {
            local_.uri_  += local_.filename_;
        }
        local_.size_ = a_size;

        // ... prepare xattrs helper ...
        xattr_ = new ::cc::fs::file::XAttr(local_.uri_);
        
        // ... we're in 'Create' mode ...
        mode_ = ngx::casper::broker::cdn::Archive::Mode::Create;
        
        // ... prepare MD5 calculation ...
        md5_.Initialize();
       
    } catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request) {
        Reset();
        throw a_bad_request;
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
       Reset();
       throw a_forbidden;
    } catch (const ngx::casper::broker::cdn::Conflict& a_conflict) {
       Reset();
        throw a_conflict;
    } catch (const ngx::casper::broker::cdn::MethodNotAllowed& a_not_allowed) {
        Reset();
        throw a_not_allowed;
    } catch (const ::cc::Exception& a_cc_exception) {
        Reset();
        throw a_cc_exception;
    } catch (...) {
        Reset();
        CC_EXCEPTION_RETHROW(/* a_unhandled */ true);
    }
}

/**
 * @brief Writer data the currently open file.
 *
 * @param a_data Data to write.
 * @param a_size Number of bytes to write.
 *
 * @return Number of bytes written.
 */
size_t ngx::casper::broker::cdn::Archive::Write (const unsigned char* const a_data, const size_t& a_size)
{
    MODE_SANITY_CHECK_BARRIER((ngx::casper::broker::cdn::Archive::Mode::Create | ngx::casper::broker::cdn::Archive::Mode::Modify));

    const size_t bw = fw_.Write(a_data, a_size);
    
    bytes_written_ += static_cast<uint64_t>(bw);
    
    // ... update MD5 ...
    if ( bw == a_size ) {
        md5_.Update(a_data, a_size);
    }
    
    return bw;
}

/**
 * @brief Flush data to currently open file.
 */
void ngx::casper::broker::cdn::Archive::Flush ()
{
    MODE_SANITY_CHECK_BARRIER((ngx::casper::broker::cdn::Archive::Mode::Create | ngx::casper::broker::cdn::Archive::Mode::Modify));

    fw_.Flush();
}

/**
 * @brief Close the currently open file.
 *
 * @param a_attrs      XAttrs to write ( if any ).
 * @param a_preserving Set of attributes to preserve
 * @param o_info       Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Close (const std::map<std::string, std::string>& a_attrs,
                                               const std::set<std::string>& a_preserving,
                                               ngx::casper::broker::cdn::Archive::RInfo& o_info)
{
    MODE_SANITY_CHECK_BARRIER((ngx::casper::broker::cdn::Archive::Mode::Create |
                               ngx::casper::broker::cdn::Archive::Mode::Modify |
                               ngx::casper::broker::cdn::Archive::Mode::Move |
                               ngx::casper::broker::cdn::Archive::Mode::Patch)
    );
    
    if ( nullptr == xattr_ ) {
        THROW_INTERNAL_ERROR("can't be called - URI to local file is not set!");
    }

    if ( true == fw_.IsOpen() ) {
        
        fw_.Flush();
        if ( fw_.Size() != bytes_written_ ) {
            fw_.Close();
            throw ::cc::Exception("Bytes written differs from actual file size !");
        } else {
            fw_.Close();
        }
        // ... reset ...
        bytes_written_ = 0;

        // ... write attributes now ...
        SetXAttrs(a_attrs, a_preserving, /* a_excluding */ { XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5" }, /* a_trusted */ false, /* a_seal */ false);

        // ... finalize and save MD5 calculation ...
        xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5", md5_.Finalize());
        
    } else {
        // ... write attributes now ...
        SetXAttrs(a_attrs, a_preserving, /* a_excluding */ {}, /* a_trusted */ false, /* a_seal */ false);
    }
    
    // ... ensure minimum required tracking attributes ...
    
    if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist") ) {
        xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", archivist_);
    }
    if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archived.at") ) {
        xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archived.at", cc::UTCTime::NowISO8601WithTZ());
    }
    
    if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.created.by") ) {
        xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.created.by", writer_);
    }
    if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.created.at") ) {
        xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.created.at", cc::UTCTime::NowISO8601WithTZ());
    }
    
    // ... seal attributes ...
    xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", tmp_);
    xattr_->Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                 reinterpret_cast<const unsigned char*>(tmp_.c_str()), tmp_.length(),
                 &sk_validation_excluded_attrs_
    );

    // ... seal replication attributes ...
    if ( 0 != replicator_.length() ) {                
        if ( false == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag") ) {
            tmp_ = ::cc::base64_url_unpadded::encode(replicator_);
            xattr_->Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag", tmp_);
        } else {
            xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag", tmp_);
        }
        xattr_->Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.seal",
                     {
                        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.tag" },
                        { XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.data" }
                     },
                     reinterpret_cast<const unsigned char*>(tmp_.c_str()), tmp_.length()
        );
    }
    
    // ... set basic info ...
    Fill(/* a_trusted */ true, o_info);
        
    // ... reset for re-usage ...
    Reset();
}


/**
 * @brief Closes a previously open file in create mode and delete it.
 */
void ngx::casper::broker::cdn::Archive::Destroy ()
{
    MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::Create);

    // ... close file ...
    fw_.Close();
    
    // ... delete it ...
    ::cc::fs::File::Erase(local_.uri_);

    // ... and reset for re-usage ...
    Reset();
}

#ifdef __APPLE__
#pragma mark - PATCH MODE
#endif

/**
 * @brief Patch an pre-existing file xattrs.
 *
 * @param a_path       URL path component
 * @param a_attrs      XAttrs to override ( if any ).
 * @param a_preserving Set of attributes to preserve ( by keeping 'a_uri' file attributes ).
 * @param a_backup     When true a copy of this file will be made and attributes are applied in the new copy,
 *                     otherwise it will be applied directly on provided file URI.
 * @param o_info       Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Patch (const std::string& a_path,
                                               const std::map<std::string, std::string>& a_attrs,
                                               const std::set<std::string>& a_preserving,
                                               const bool a_backup,
                                               ngx::casper::broker::cdn::Archive::RInfo& o_info)
{
    MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::Create);
    
    if ( true == a_backup ) {
        // ... try to open 'original' file in 'patch' mode ...
        ngx::casper::broker::cdn::Archive old_archive(archivist_, writer_, act_config_, headers_, h2e_map_, dir_prefix_, replicator_);
        old_archive.Open(a_path, [] (const std::string& /* a_xhvn */) {
            return ngx::casper::broker::cdn::Archive::Mode::Patch;
        });
        // ... since we've 'patch' permissions, revert mode ...
        old_archive.mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
        // ... grab xattrs
        std::map<std::string, std::string> old_attrs;
        old_archive.IterateXAttrs([&old_attrs](const char *const a_key, const char *const a_value) {
            old_attrs[a_key] = a_value;
        });
        // ... keep track of some 'old' properties ...
        const std::string old_id  = old_archive.id();
        const std::string old_uri = old_archive.uri();
        
        // ... and 'close' original file ...
        old_archive.Close(/* o_info */ nullptr);
        // ... overwrite with new attrs ...
        for ( auto attr : a_attrs ) {
            old_attrs[attr.first] = attr.second;
        }
        old_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"] = id();
        
        // ... set xattrs and 'close' this new file ...
        Close(old_attrs, a_preserving, o_info);

        // ... restore 'old' info ...
        o_info.old_id_  = old_id;
        o_info.old_uri_ = old_uri;
        
        // ... read new xattrs - ( during close process new xattrs can be added,
        //     e.g. if permissions.hr xattr was set, it is complied and new xattrs are added ...
        std::map<std::string, std::string> new_attrs;
        ::cc::fs::file::XAttr dst_xattrs(o_info.new_uri_);
        dst_xattrs.Iterate([&new_attrs](const char *const a_key, const char *const a_value) {
            new_attrs[a_key] = a_value;
        });
        
        // ... new file should be empty ( if not it will be overwritten with old data ! ) ...
        ::cc::fs::File::Copy(o_info.old_uri_, o_info.new_uri_, /* a_overwrite */ true,
                             &new_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5"]
        );
        new_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length"] = std::to_string(::cc::fs::File::Size(o_info.new_uri_));

        // ... restore xattrs ...
        for ( auto attr : new_attrs ) {
            dst_xattrs.Set(attr.first, attr.second);
        }
        
        dst_xattrs.Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", tmp_);
        dst_xattrs.Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                        reinterpret_cast<const unsigned char*>(tmp_.c_str()), tmp_.length(),
                        &sk_validation_excluded_attrs_
        );
        
        // ... finish filling info ...
        for ( auto& attr : o_info.attrs_ ) {
            if ( true == attr.optional_ ) {
                if ( true == dst_xattrs.Exists(attr.name_) ) {
                    dst_xattrs.Get(attr.name_, attr.value_);
                }
            } else {
                dst_xattrs.Get(attr.name_, attr.value_);
            }
        }
        
    } else {
        // ... try to open it in 'patch' mode ...
        Open(a_path, [] (const std::string& /* a_xhvn */) {
            return ngx::casper::broker::cdn::Archive::Mode::Patch;
        });
        
        // ... add / replace 'new' xattrs ( if any ) ...
        SetXAttrs(a_attrs, a_preserving, /* a_excluding */ {},  /* a_trusted */ true, /* a_seal */ true);
        
        // ... switch mode ( since operation is just a xattrs add / replace ) to read ...
        mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
        
        // ... 'close' it ...
        Close(&o_info);
    }
}

#ifdef __APPLE__
#pragma mark - DELETE MODE
#endif

/**
 * @brief Delete an pre-existing file.
 *
 * @param a_path       URL path component.
 * @param a_quarantine When not null a copy of this file will be made to quarantine directory,
 *                     otherwise it will be deleted now.
 * @param o_info       Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Delete (const std::string& a_path,
                                                const ngx::casper::broker::cdn::Archive::QuarantineSettings* a_quarantine,
                                                ngx::casper::broker::cdn::Archive::RInfo& o_info)
{
    if ( ngx::casper::broker::cdn::Archive::Mode::NotSet != mode_ ) {
        MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::NotSet);
    }
    
    if ( nullptr != a_quarantine ) {
        
        // ... try to open it in 'delete' mode ...
        Open(a_path, [] (const std::string& /* a_xhvn */) {
            return ngx::casper::broker::cdn::Archive::Mode::Delete;
        });
        
        // ... keep track of old info ...
        o_info.old_id_  = id();
        o_info.old_uri_ = uri();
        
        // ... keep track of new info ...
        o_info.new_id_  = id();
        // ... set quarantine path ...
        o_info.new_uri_ = a_quarantine->directory_prefix_;
        // ... get time w/offset ...
        cc::UTCTime::HumanReadable now_hrt = cc::UTCTime::ToHumanReadable(cc::UTCTime::OffsetBy(a_quarantine->validity_));
        char                       now_hrt_b[27];

        const int w = snprintf(now_hrt_b, 26, "%04d-%02d-%02d/",
                               static_cast<int>(now_hrt.year_), static_cast<int>(now_hrt.month_), static_cast<int>(now_hrt.day_)
        );
        if ( w < 0 || w > 26 ) {
            throw ::cc::Exception("Unable to quarantine file - output dir buffer write error!");
        }
        o_info.new_uri_ += now_hrt_b;
        o_info.new_uri_ += ( local().path_.c_str() + dir_prefix_.length() );

        // ... ensure quarantine path exists ...
        ::cc::fs::Dir::Make(o_info.new_uri_.c_str());
        
        // ... set quarantine uri ...
        o_info.new_uri_ += local_.filename_ + local_.ext_;

        // ... switch mode ( since operation is just a delete ) to read ...
        mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
        
        // ... 'close' it ...
        Close(/* o_info */ nullptr);
                
        // ... copy file to quarantine ..
        ngx::casper::broker::cdn::Archive::Copy(o_info.old_uri_, o_info.new_uri_, /* a_overwrite */ true, /* o_attrs */ &o_info.attrs_);
        
    } else {

        // ... try to open it in 'delete' mode ...
        Open(a_path, [] (const std::string& /* a_xhvn */) {
            return ngx::casper::broker::cdn::Archive::Mode::Delete;
        });
        
        // ... keep track of local URI ...
        const std::string local_uri = local_.uri_;
        
        // ... switch mode ( since operation is just a delete ) to read ...
        mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
            // ... 'close' it ...
        Close(&o_info);
        
        // ... and erase it ...
        ::cc::fs::File::Erase(local_uri);
        
    }
}

#ifdef __APPLE__
#pragma mark - Replace
#endif

/**
 * @brief Update a pre-existing file.
 *
 * @param a_path       URL path component.
 * @param a_attrs      XAttrs to override ( if any ).
 * @param a_preserving Set of attributes to preserve ( by keeping 'a_uri' file attributes ).
 * @param a_backup     When true a copy of this file will be made and new content / attributes are applied in the new copy,
 *                     otherwise it will be applied directly on provided file URI.
 * @param a_by         Usualy user agent who asked for this operation.
 * @param o_info       Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Update (const std::string& a_path,
                                                const std::map<std::string, std::string>& a_attrs,
                                                const std::set<std::string>& a_preserving,
                                                const bool a_backup,
                                                const ngx::casper::broker::cdn::XArchivedBy& a_by,
                                                ngx::casper::broker::cdn::Archive::RInfo& o_info)
{
    MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::Create);

    // ... switch mode ( if an exception is thrown we must be in the correct mode ) ...
    mode_ = ngx::casper::broker::cdn::Archive::Mode::Modify;
    
    // ... open old archive for 'w' permissions check ...
    ngx::casper::broker::cdn::Archive old(archivist_, writer_, act_config_, headers_, h2e_map_, dir_prefix_);
    
    //
    // ... ensure 'w' permissions ( on old file )  ...
    //
    // ... try to open it in 'modify' mode ...
    old.Open(a_path, [] (const std::string& /* a_xhvn */) -> ngx::casper::broker::cdn::Archive::Mode {
        return ngx::casper::broker::cdn::Archive::Mode::Modify;
    });
    
    const std::string cur_local_uri = old.local_.uri_;
    // do NOT call: old.Close();
    
    //
    // ... copy 'old' file attributes to temporary one ( this archive instance file )
    //
    const ::cc::fs::file::XAttr old_xattrs(cur_local_uri);
    
    // ... load all 'old' xattrs ...
    std::map<std::string, std::string> old_attrs;
    old_xattrs.Iterate([&old_attrs](const char *const a_key, const char *const a_value) {
        old_attrs[a_key] = a_value;
    });
    old_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"] = id();
    
    // ... exclude 'old' attributes ...
    std::set<std::string> tmp_set;
    for ( auto it : a_attrs ) {
        const auto old_it = old_attrs.find(it.first);
        if ( old_attrs.end() != old_it ) {
            tmp_set.insert(it.first);
        }
    }
    for ( auto it : tmp_set ) {
        old_attrs.erase(old_attrs.find(it));
    }
    
    // ... merge ...
    std::map<std::string, std::string> final_attrs;
    
    // ... first, old attrs ....
    for ( auto it : old_attrs ) {
        final_attrs[it.first] = it.second;
    }
    // ... first, new attrs ....
    for ( auto it : a_attrs ) {
        final_attrs[it.first] = it.second;
    }
    
    // ... set modification attributes  ...
    SetContentModificationAttrs(old, a_by, final_attrs);
    
    // ... close and do update ...
    const std::string swap_local_file = local_.uri_;

    // ... keep track of IDs and URIS ...
    const std::string new_id  = id();
    const std::string new_uri = uri();
    const std::string old_id  = old.id();
    const std::string old_uri = old.uri();
    
    // ... switch mode ( just for closing ) to read ...
    old.mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
    old.Close(/* o_info */ nullptr);

    // ... 'close' it  ( attributes are written here ) ...
    Close(final_attrs, a_preserving, o_info);
    
    // ... make a backup or rename it now?
    if ( true == a_backup ) {
        // ... make a backup:
        // - by keeping both files alive until caller decides to purge old file
        // - and updating returning info
        //
        o_info.new_id_  = new_id;
        o_info.new_uri_ = new_uri;
        o_info.old_id_  = old_id;
        o_info.old_uri_ = old_uri;
    } else {
        // ... trying an 'atomic' swap ...
        try {
            // ... succeeded, swap returning info ..
            o_info.new_id_  = old_id;
            o_info.new_uri_ = old_uri;
            o_info.old_id_  = "";
            o_info.old_uri_ = "";
            // ... move temporary ( 'swap' ) file to current location ( by renaming it ) ...
            ::cc::fs::File::Rename(swap_local_file, cur_local_uri);
        } catch (const ::cc::fs::Exception& a_fs_exception) {
            // ... move failed, erase temporary ( 'swap' ) file ...
            ::cc::fs::File::Erase(swap_local_file);
            // ... rethrow exception ...
            throw a_fs_exception;
            // ... keep returning info ...
            // ... nop ...
        }
    }
}

#ifdef __APPLE__
#pragma mark - Move
#endif

/**
 * @brief Moves a pre-existing file.
 *
 * @param a_uri        Local URI of file to move.
 * @param a_attrs      Map of attributes name / value.
 * @param a_preserving Set of attributes to preserve ( by keeping 'a_uri' file attributes ).
 * @param a_backup     When true a copy of this file will be made and attributes are applied in the new copy,
 *                     otherwise it will be applied directly on provided file URI.
 * @param o_info       Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Move (const std::string& a_uri,
                                              const std::map<std::string, std::string>& a_attrs,
                                              const std::set<std::string>& a_preserving,
                                              const bool a_backup,
                                              ngx::casper::broker::cdn::Archive::RInfo& o_info)
{
    MODE_SANITY_CHECK_BARRIER(ngx::casper::broker::cdn::Archive::Mode::Create);
    
    // ... switch mode ( if an exception is thrown we must be in the correct mode ) ...
    mode_ = ngx::casper::broker::cdn::Archive::Mode::Move;
    
    // ... make sure it exists ...
    if ( false == ::cc::fs::File::Exists(a_uri) ) {
        throw ngx::casper::broker::cdn::NotFound();
    }

    // ... preserve new file uri ...
    const std::string  swap_file_local_uri = local_.uri_;

    // ... 'close' and write provided attrs ...
    Close(a_attrs, a_preserving, o_info);
    
    // @remarks: o_info.old_id_ && o_info.old_uri_
    //           should not be set since we're 'moving' a temporary file
    //           if move fails we don't want to delete it
    
    //
    // PRE-MOVE XATTRS ADJUSTMENTS
    //
    ::cc::fs::file::XAttr cur_xattrs(a_uri);
    ::cc::fs::file::XAttr swp_xattrs(swap_file_local_uri);
    
    //
    // ... do not trust upload md5 info -
    // ... it could be changed after upload and / or before this API is called ...
    //
    // ... release previous buffer ( if any ) ...
    if ( nullptr != buffer_ ) {
        delete [] buffer_;
    }
    // ... allocate new 'read' buffer ...
    buffer_ = new unsigned char[10485760];
    // ... initialize MD5 ...
    md5_.Initialize();
    bool eof = false; size_t len = 0;
    ::cc::fs::File fr; fr.Open(a_uri, ::cc::fs::file::Writer::Mode::Read);
    while ( 0 != ( len = fr.Read(buffer_, 10485760, eof) ) ) {
        md5_.Update(buffer_, len);
        if ( true == eof ) {
            break;
        }
    }
    fr.Close();
    // ... release buffer ...
    delete [] buffer_;
    buffer_ = nullptr;
    //  ... finalize MD5 calculation ...
    const std::string md5 = md5_.Finalize();
    // ... set 'md5' attribute ...
    swp_xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5", md5);
    // ... copy original attributes ...
    for ( auto it : a_preserving ) {
        if ( true == cur_xattrs.Exists(it) ) {
            cur_xattrs.Get(it, tmp_);
            swp_xattrs.Set(it, tmp_);
        }
    }

    // ... update r_info ...
    o_info.old_uri_ = a_uri;
    ::cc::fs::File::Name(a_uri, o_info.old_id_);
    
    // ... trying an 'atomic' swap ...
    try {
        // ... 'move' file to new location ( by renaming it ) and retrieve xattrs ( if required ) ...
        ngx::casper::broker::cdn::Archive::Rename(a_uri, swap_file_local_uri, a_backup, &o_info);
    } catch (const ::cc::fs::Exception& a_fs_exception) {
        // ... move failed, erase temporary ( 'swap' ) file ...
        ::cc::fs::File::Erase(swap_file_local_uri);
        // ... re-throw exception ...
        throw a_fs_exception;
    }
}

/**
 * @brief Extract an id from an URL path.
 *
 * @param a_path URL path component.
 * @param o_id   Valid ID will be set here.
 * @param o_ext  Outputs extension ( if any ).
 */
void ngx::casper::broker::cdn::Archive::IDFromURLPath (const std::string& a_path,
                                                       std::string& o_id, std::string* o_ext)
{
    const char* start_ptr = strrchr(a_path.c_str(), '/');
    if ( nullptr == start_ptr ) {
        throw ngx::casper::broker::cdn::BadRequest(
            "Unable to extract archive ID from URI path '%s' - path must start with '/'!", a_path.c_str()
        );
    }
    start_ptr += sizeof(char);
        
    const char* const args_ptr = strrchr(start_ptr, '?');
    const char* const end_ptr  = ( nullptr != args_ptr ? args_ptr : a_path.c_str() + a_path.length() );

    // ... can't extract file ID from URI path?
    if ( 0 == ( end_ptr - start_ptr ) ) {
        throw ngx::casper::broker::cdn::BadRequest(
            "Unable to extract archive ID from URI path '%s'!", a_path.c_str()
        );
    }
    
    // ... extract id from URL path ...
    const char* const ext_dot_ptr = strrchr(start_ptr, '.');
    if ( nullptr != ext_dot_ptr ) {
        o_id = std::string(start_ptr, ext_dot_ptr - start_ptr);
    } else if ( nullptr != args_ptr ) {
        o_id = std::string(start_ptr, args_ptr - start_ptr);
    } else {
        o_id = start_ptr;
    }
    
    // ... validated extracted id ...
    const std::regex id_expr("([a-z]{1})([a-zA-Z]{2})([a-zA-Z0-9]{6})(\\..+)?", std::regex_constants::ECMAScript);
    std::smatch match;
    if ( false == std::regex_search(o_id, match, id_expr) || match.size() < 5 ) {
        throw ngx::casper::broker::cdn::BadRequest(
            "Unable to extract archive ID from URI path '%s' - invalid ID format '%s'!", a_path.c_str(), o_id.c_str()
        );
    }
    
    // ... optionally, extract extension from URL path ( if any ) ...
    if ( nullptr != o_ext ) {
        if ( nullptr != ext_dot_ptr ) {
            if ( nullptr != args_ptr ) {
                (*o_ext) = std::string(ext_dot_ptr + sizeof(char), args_ptr - ext_dot_ptr - sizeof(char));
            } else {
                (*o_ext) = std::string(ext_dot_ptr + sizeof(char));
            }
        } else {
            (*o_ext) = "";
        }
    }
}

/**
 * @brief Rename a file preserving it's attrs.
 *
 * @param a_from   Source local URI.
 * @param a_to     Destination local URI ( final attritutes must be set here ).
 */
void ngx::casper::broker::cdn::Archive::Rename (const std::string& a_from, const std::string& a_to)
{
    ngx::casper::broker::cdn::Archive::Rename(a_from, a_to, /* a_backup */ false);
}

#ifdef __APPLE__
#pragma mark - VALIDATE
#endif

/**
 * @brief Validate this archive.
 *
 * @param a_path URL path component of file to be tested.
 * @param a_md5 MD5 value to be tested.
 * @param a_id  ID to compare to instead of file name ( used by when replicated to a temporary file ).
 * @param a_attrs Attributes to compare against written ones.
 */
void ngx::casper::broker::cdn::Archive::Validate (const std::string& a_path,
                                                  const std::string* a_md5, const std::string* a_id,
                                                  const std::map<std::string,std::string>* a_attrs)
{
    // ... try to open it in 'patch' mode ...
    Open(a_path, [] (const std::string& /* a_xhvn */) {
        return ngx::casper::broker::cdn::Archive::Mode::Validate;
    });
        
    const auto close_archive = [this] () {
        // ... switch mode ( since operation is just a xattrs add / replace ) to read ...
        mode_ = ngx::casper::broker::cdn::Archive::Mode::Read;
        // ... 'close' it ...
        Close(/* o_info */ nullptr);
    };
    
    try {
        
        std::string      tmp;
        std::string      expected_id;
        size_t           expected_size;
        std::string      expected_md5;
        std::string      actual_name;
        std::string      actual_archivist;
        bool             eof;
        unsigned char    buffer [1024];

        ::cc::fs::File file; file.Open(local().uri_, cc::fs::File::Mode::Read);
        // ... id ...
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.id", expected_id);
        if ( nullptr != a_id ) {
            if ( 0 != expected_id.compare(*a_id) ) {
                throw cc::Exception("ID invalid or mismatch!");
            }
        } else {
            ::cc::fs::File::Name(local().uri_, actual_name);
            if ( expected_id.size() != 9  || 0 != expected_id.compare(3, 6, actual_name) ) {
                throw cc::Exception("ID invalid or mismatch!");
            }
        }
        // ... size ...
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length", tmp);
        if ( 1 != sscanf(tmp.c_str(), "%zd", &expected_size) ) {
            throw cc::Exception("Size mismatch or invalid format got %s!", tmp.length() > 0 ? tmp.c_str() : "<empty>");
        }
        if ( file.Size() != static_cast<uint64_t>(expected_size) ) {
            throw cc::Exception("Size mismatch!");
        }
        // ... md5 ...
        ::cc::hash::MD5 md5;
        md5.Initialize();
        do {
            md5.Update(buffer, file.Read(buffer, 1024, eof));
        } while ( false == eof );
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5", expected_md5);
        if ( 0 != expected_md5.compare(md5.Finalize()) ) {
            throw cc::Exception("MD5 mismatch!");
        }
        if ( nullptr != a_md5 ) {
            if ( 0 != expected_md5.compare(*a_md5) ) {
                throw cc::Exception("MD5 mismatch!");
            }
        }
        // ... xattrs seal ...
        xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", actual_archivist);
        xattr_->Validate(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                         reinterpret_cast<const unsigned char*>(actual_archivist.c_str()), actual_archivist.length(),
                         &sk_validation_excluded_attrs_
        );
        // ... ensure xattrs correctly copied ...
        if ( nullptr != a_attrs ) {
            for ( auto lhs_attr : *a_attrs ) {
                // ... get xattr - must exist ...
                xattr_->Get(lhs_attr.first, tmp);
                // ... skip replication data ...
                if ( sk_validation_excluded_attrs_.end() != sk_validation_excluded_attrs_.find(lhs_attr.first) ) {
                    continue;
                }
                // ... compare non-excluded xattr ...
                if ( 0 != lhs_attr.second.compare(tmp) ) {
                    throw cc::Exception("Attribute %s value differs: got '%s', expected '%s'!",
                                        lhs_attr.first.c_str(), tmp.c_str(), lhs_attr.second.c_str()
                    );
                }
            }
        }
        // ... done ...
        close_archive();
    } catch (...) {
        // ... done ...
        close_archive();
        // ... rethrow ...
        CC_EXCEPTION_RETHROW(/* a_unhandled */ true);
    }
}

#ifdef __APPLE__
#pragma mark - XATTRs - Extended Attributes
#endif

/**
 * @brief Check if an extended attribute is set.
 *
 * @param a_name Attribute name.
 */
bool ngx::casper::broker::cdn::Archive::HasXAttr (const std::string& a_name) const
{
    XATTR_READ_SANITY_CHECK_BARRIER();
    
    return xattr_->Exists(a_name);
}

/**
 * @brief Obtain an extended attribute value.
 *
 * @param a_name  Attribute name.
 * @param o_value Attribute value as uint64_t.
 */
void ngx::casper::broker::cdn::Archive::GetXAttr (const std::string& a_name, uint64_t& o_value) const
{
    XATTR_READ_SANITY_CHECK_BARRIER();
    
    std::string tmp;
    
    xattr_->Get(a_name, tmp);
    
    o_value = std::stoull(tmp);
}

/**
 * @brief Obtain an extended attribute value.
 *
 * @param a_name  Attribute name.
 * @param o_value Attribute value as string.
 */
void ngx::casper::broker::cdn::Archive::GetXAttr (const std::string& a_name, std::string& o_value) const
{
    XATTR_READ_SANITY_CHECK_BARRIER();
    
    xattr_->Get(a_name, o_value);
}

/**
 * @brief Obtain a set of extended attributes.
 *
 * @param a_attrs Map of attributes names to retrieve.
 */
void ngx::casper::broker::cdn::Archive::GetXAttrs (std::map<std::string, std::string>& o_attrs) const
{
    XATTR_READ_SANITY_CHECK_BARRIER();
    
    std::set<std::string> keys;
    for ( auto it : o_attrs ) {
        o_attrs[it.first] = "";
        keys.insert(it.first);
    }
    
    std::string tmp;
    for ( auto name : keys ) {
        xattr_->Get(name, tmp);
        o_attrs[name] = tmp;
    }
}

/**
 * @brief Iterate all extended attributes.
 *
 * @param a_callback Function to call to deliver each iteration entry.
 */
void ngx::casper::broker::cdn::Archive::IterateXAttrs (const std::function<void (const char *const, const char *const)>& a_callback) const
{
    XATTR_READ_SANITY_CHECK_BARRIER();
    
    xattr_->Iterate(a_callback);
}

#ifdef __APPLE__
#pragma mark - XATTRs - Extended Attributes - PRIVATE
#endif

/**
 * @brief Write a set of extended attributes.
 *
 * @param a_attrs Map of attributes name / value.
 * @param a_preserving Set of attributes to preserve ( ignoring attrs from a_attrs ).
 * @param a_excluding  Set of attributes to exclude ( from inode ).
 * @param a_trusted True if theres no need to do a sanity check, false otherwise.
 */
void ngx::casper::broker::cdn::Archive::SetXAttrs (const std::map<std::string, std::string>& a_attrs,
                                                   const std::set<std::string>& a_preserving,
                                                   const std::set<std::string>& a_excluding,
                                                   bool a_trusted, bool a_seal)
{
    if ( false == a_trusted ) {
        XATTR_WRITE_SANITY_CHECK_BARRIER();
    }
    
    for ( auto it : a_attrs ) {
        if ( a_preserving.end() != a_preserving.find(it.first) && true == xattr_->Exists(it.first) ) {
            continue;
        }
        xattr_->Set(it.first, it.second);
    }
    
    const auto it = a_attrs.find(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr");
    if ( a_attrs.end() != it ) {
        for ( auto it2 : { XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.r", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.w", XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.d" } ) {
            if ( true == xattr_->Exists(it2) ) {
                xattr_->Remove(it2);
            }
        }
        act_.Compile(it->second, [this] (const std::string& a_key, const std::string& a_value) {
            for ( auto ch : a_key ) {
                xattr_->Set(std::string(XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.ast.") + ch, a_value);
            }
        });
    }
    
    std::map<std::string, std::string> attrs;
    if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.by") ) {
        if ( 0 == replicator_.length() ) {
            if ( true == xattr_->Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count") ) {
                xattr_->Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count", tmp_);
                attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = std::to_string((std::strtoull(tmp_.c_str(), nullptr, 0) + 1));
            } else {
                attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.count"] = "1";
            }
            attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.by"] = writer_;
            attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.modified.at"] = cc::UTCTime::NowISO8601WithTZ();
        }
    } else {
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.by"] = writer_;
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.created.at"] = cc::UTCTime::NowISO8601WithTZ();
    }
    
    for ( auto it : attrs ) {
        xattr_->Set(it.first, it.second);
    }
    
    for ( auto it : a_excluding ) {
        if ( true == xattr_->Exists(it) ) {
            xattr_->Remove(it);
        }
    }
    
    // ... seal xattrs?
    if ( true == a_seal ) {
        xattr_->Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal", reinterpret_cast<const unsigned
                     char*>(archivist_.c_str()), archivist_.length(),
                     &sk_validation_excluded_attrs_
        );
    }
}

/**
 * @brief Retrieve basic information.
 *
 * @param a_trusted True if theres no need to do a sanity check, false otherwise.
 * @param o_info    Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Fill (bool a_trusted, ngx::casper::broker::cdn::Archive::RInfo &o_info)
{
    if ( false == a_trusted ) {
        XATTR_WRITE_SANITY_CHECK_BARRIER();
    }
    o_info.new_id_  = id();
    o_info.new_uri_ = uri();
    o_info.old_id_  = "";
    o_info.old_uri_ = "";
    for ( auto& attr : o_info.attrs_ ) {
        if ( true == attr.optional_ ) {
            if ( true == xattr_->Exists(attr.name_) ) {
                xattr_->Get(attr.name_, attr.value_);
            }
        } else {
            xattr_->Get(attr.name_, attr.value_);
        }
    }
}

/**
 * @brief Rename an archive file.
 *
 * @param a_from   Source local URI.
 * @param a_to     Destination local URI ( final attritutes must be set here ).
 * @param a_backup When true a copy of this file will be made and attributes are applied in the new copy,
 *                 otherwise it will be applied directly on provided file URI.
 * @param o_info   Important information to return.
 */
void ngx::casper::broker::cdn::Archive::Rename (const std::string& a_from, const std::string& a_to,
                                                const bool a_backup,
                                                ngx::casper::broker::cdn::Archive::RInfo* o_info)
{
    //
    // ... ensure attributes are set ...
    // ... rename is specific to the underlying filesystem implementation, unclear of xattrs behaviour ...
    //
    
    //
    // ⚠️ at the time of this implementation:
    //
    // - macOS and debian implementations will replace file if exist.
    // - final destination ( a_to_uri ) file is already reserved and attributes set
    // - original source ( a_from_uri ) contains old attributes that are no longer required
    // - after calling rename API the original attributes are kept and destination one are lost
    // - first must copy attributes of final destination, call rename API and then restore the attributes
    //
    
    std::map<std::string, std::string> attrs;
        
    // ... copy xattrs, a_to is correct because destination file already has the final attributes ...
    ::cc::fs::file::XAttr src_xattrs(a_to);
    src_xattrs.Iterate([&attrs](const char *const a_key, const char *const a_value) {
        attrs[a_key] = a_value;
    });

    // ... rename ...
    if ( true == a_backup ) {
        ::cc::fs::File::Copy(a_from, a_to, /* a_overwrite */ true);
    } else {
        ::cc::fs::File::Rename(a_from, a_to);
    }
    
    // ... restore xattrs ...
    ::cc::fs::file::XAttr dst_xattrs(a_to);
    for ( auto it : attrs ) {
        dst_xattrs.Set(it.first, it.second);
    }
    
    // ... seal xattrs, file content was copied as xattrs - must be sealed again  ...
    std::string archivist;
    dst_xattrs.Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", archivist);
    dst_xattrs.Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                reinterpret_cast<const unsigned char*>(archivist.c_str()), archivist.length(),
                &sk_validation_excluded_attrs_
    );
    
    // ... update r_info?
    if ( nullptr != o_info ) {
        for ( auto& attr : (*o_info).attrs_ ) {
            if ( true == attr.optional_ ) {
                if ( true == dst_xattrs.Exists(attr.name_) ) {
                    dst_xattrs.Get(attr.name_, attr.value_);
                }
            } else {
                dst_xattrs.Get(attr.name_, attr.value_);
            }
        }
    }
}

/**
 * @brief Copy an archive file.
 *
 * @param a_from  Source local URI.
 * @param a_to    Destination local URI ( final attritutes must be set here ).
 * @param a_overwrite When true, overwrite destination file ( if any ).
 * @param o_attrs When not null, returns requested xattrs.
 */
void ngx::casper::broker::cdn::Archive::Copy (const std::string& a_from, const std::string& a_to, const bool a_overwrite,
                                              std::vector<ngx::casper::broker::cdn::Archive::RAttr>* o_attrs)
{
    //
    // ... ensure attributes are set ...
    // ... rename is specific to the underlying filesystem implementation, unclear of xattrs behaviour ...
    //
    
    //
    // ⚠️ at the time of this implementation:
    //
    // - final destination ( a_to_uri ) file is already reserved and attributes set
    // - original source ( a_from_uri ) contains old attributes that are no longer required
    // - after calling rename API the original attributes are kept and destination one are lost
    // - first must copy attributes of final destination, call rename API and then restore the attributes
    //
    
    std::map<std::string, std::string> attrs;
    
    // ... copy xattrs ...
    ::cc::fs::file::XAttr src_xattrs(a_from);
    src_xattrs.Iterate([&attrs](const char *const a_key, const char *const a_value) {
        attrs[a_key] = a_value;
    });
    
    // ... rename ...
    ::cc::fs::File::Copy(a_from, a_to, a_overwrite);
    
    // ... restore xattrs ...
    ::cc::fs::file::XAttr dst_xattrs(a_to);
    for ( auto it : attrs ) {
        dst_xattrs.Set(it.first, it.second);
    }
    
    // ... seal xattrs ...
    std::string archivist;
    dst_xattrs.Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.archivist", archivist);
    dst_xattrs.Seal(XATTR_ARCHIVE_PREFIX "com.cldware.archive.xattrs.seal",
                   reinterpret_cast<const unsigned char*>(archivist.c_str()), archivist.length(),
                   &sk_validation_excluded_attrs_
    );

    // ... update o_attrs?
    if ( nullptr != o_attrs ) {
        for ( auto& attr : (*o_attrs) ) {
            if ( true == attr.optional_ ) {
                if ( true == dst_xattrs.Exists(attr.name_) ) {
                    dst_xattrs.Get(attr.name_, attr.value_);
                }
            } else {
                dst_xattrs.Get(attr.name_, attr.value_);
            }
        }
    }
}

#ifdef __APPLE__
#pragma mark - HELPERS
#endif

/**
 * @brief Set modification xattrs for a specific archive.
 *
 * @param a_archive The archive to ready 'old' modification attributes ( if any ) from.
 * @param a_by      Usualy user agent who asked for this operation.
 * @param o_attrs   Map where attributes should be appended to.
 */
void ngx::casper::broker::cdn::Archive::SetContentModificationAttrs (const ngx::casper::broker::cdn::Archive& a_archive,
                                                                     const ngx::casper::broker::cdn::XArchivedBy& a_by,
                                                                     std::map<std::string, std::string>& o_attrs)
{
    const ::cc::fs::file::XAttr old_xattrs(a_archive.uri());

    std::string tmp;

    // ... modification basic attributes ...
    o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.at"] = cc::UTCTime::NowISO8601WithTZ();
    o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.by"] = a_archive.writer_;
    if ( true == old_xattrs.Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.count") ) {
        old_xattrs.Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.count", tmp);
        o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.count"] = std::to_string((std::strtoull(tmp.c_str(), nullptr, 0) + 1));
    } else {
        o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.count"] = "1";
    }
    // ... if history is no enabled ...
    if ( false == sk_content_modification_history_enabled_ ) {
        // ... set requestee ...
        o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.requestee"] = (const std::string&)a_by;
        // ... and we're done ...
        return;
    }
    // ... append to history? ...
    Json::FastWriter writer; writer.omitEndingLineFeed();
    Json::Value      array;
    // ... history exists?
    if ( true == old_xattrs.Exists(XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.history") ) {
        // ... yes, grab JSON string ...
        old_xattrs.Get(XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.history", tmp);
        // ... parse ...
        Json::Reader reader;
        if ( false == reader.parse(tmp, array) ) {
            array = Json::Value(Json::ValueType::arrayValue);
        }
    } else {
        // ... no, first entry - prepare array ...
        array = Json::Value(Json::ValueType::arrayValue);
    }
    // ... set entry data ...
    Json::Value& entry = array.append(Json::ValueType::objectValue);
    entry["requestee"] = (const std::string&)a_by;
    entry["at"]        = o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.at"];
    entry["operation"] = "Update";
    entry["id"       ] = a_archive.id();
    // ... serialize all history ...
    o_attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.modified.history"] = writer.write(array);
}

/**
 * @brief Extract archive local info from an URL path.
 *
 * @param a_path             URL Path.
 * @param a_archive          Archive reference to access config variables.
 * @param o_append_extension When true, extension ( legacy, if any ) will be appended to local URI.
 * @param o_local            Local archive info.
 */
void ngx::casper::broker::cdn::Archive::SetLocalInfoFromURLPath (const std::string& a_path,
                                                                 const ngx::casper::broker::cdn::Archive& a_archive,
                                                                 const bool a_append_extension,
                                                                 ngx::casper::broker::cdn::Archive::Local& o_local)
{
    // ... grab id and extension ...
    IDFromURLPath(a_path, o_local.id_, &o_local.ext_);

    // ... local path starts with directory prefix ...
    o_local.path_ = a_archive.dir_prefix_;

    // ... search for x-header-variable name
    std::string xhvn;
    for ( auto it : a_archive.h2e_map_ ) {
        if ( o_local.id_[0] == it.second.c_str()[0] ) {
            o_local.xhvn_  = it.first;
            o_local.path_ += it.second;
            break;
        }
    }
    
    // ... ensure xhvn exist on map ...
    if ( 0 == o_local.xhvn_.length() ) {
        throw ngx::casper::broker::cdn::BadRequest("Unrecognized h2e prefix '%c'!", o_local.id_[0]);
    }
    
    // ... an X-CASPER-<> header must be present ...
    const auto it = std::find_if(a_archive.headers_.begin(), a_archive.headers_.end(), ngx::casper::broker::cdn::Header<std::string>::KeyComparator(o_local.xhvn_));
    if ( a_archive.headers_.end() == it ) {
        throw ngx::casper::broker::cdn::BadRequest("Missing or invalid h2e header value for prefix %s", o_local.xhvn_.c_str());
    }
    
    // ... ensure xhvn value ...
    const std::string xhvn_value = it->second;
    if ( 0 == xhvn_value.length() || xhvn_value.length() > 12 ) {
        throw ngx::casper::broker::cdn::BadRequest("Invalid h2e prefix value %s!", xhvn_value.c_str());
    }
    
    // ... build path component ...
    const int64_t id_as_number   = static_cast<int64_t>(std::atoi(xhvn_value.c_str()));
    char          id_buffer [16] = { 0 };
    const int written = snprintf(id_buffer, sizeof(id_buffer) / sizeof(id_buffer[0]), INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3) "/" INT64_FMT_ZP(3),
                                 (id_as_number % 1000000000000) / 1000000000,
                                 (id_as_number % 1000000000)    / 1000000   ,
                                 (id_as_number % 1000000)       / 1000      ,
                                 (id_as_number % 1000)
    );
    if ( written < 0 || written > 15 ) {
        throw ngx::casper::broker::cdn::InternalServerError("Unable to prepare id - buffer write error!");
    }

    // ... prepare path ...
    o_local.path_ += '/';
    o_local.path_ += id_buffer;
    o_local.path_ += '/';
    
    // ... set filename ...
    o_local.filename_ = std::string(o_local.id_, 3);
    
    // ... finalize path ...
    o_local.path_ += std::string(o_local.id_, 1, 2) + '/';
    
    // ... set URI ...
    o_local.uri_ = ( o_local.path_ + o_local.filename_ );
    if ( true == a_append_extension && o_local.ext_.length() > 0 ) {
        // ... if '.' is set ...
        if ( '.' == o_local.ext_.c_str()[0] ) {
            // ... just append it ...
            o_local.uri_ += o_local.ext_;
        } else {
            // ... append '.' and then extension ...
            o_local.uri_ += '.' + o_local.ext_;
        }
    }

    // ... set file size ( if exists ) ...
    if ( true == ::cc::fs::File::Exists(o_local.uri_) ) {
        o_local.size_ = ::cc::fs::File::Size(o_local.uri_);
    } else {
        o_local.size_ = 0;
    }
}

#ifdef __APPLE__
#pragma mark - CONFIG HELPERS
#endif

/**
 * @brief Load H2EMap.
 *
 * param a_uri
 * param o_map
 */
void ngx::casper::broker::cdn::Archive::LoadH2EMap (const std::string& a_uri, ngx::casper::broker::cdn::H2EMap& o_map)
{
    std::filebuf file;
    if ( nullptr == file.open(a_uri, std::ios::in) ) {
        throw ::cc::Exception(("An error occurred while opening file '" + a_uri + "' to read ACT configuration!" ));
    }
    std::istream in_stream(&file);
    SetH2EMap([&in_stream] (Json::Reader& a_reader, Json::Value& o_config) {
        return a_reader.parse(in_stream, o_config);
    }, o_map);
}

/**
 * @brief Load H2EMap.
 *
 * param a_data
 * param a_length
 * param o_map
 */
void ngx::casper::broker::cdn::Archive::LoadH2EMap (const char* const a_data, size_t a_length, ngx::casper::broker::cdn::H2EMap& o_map)
{
    SetH2EMap([a_data, &a_length] (Json::Reader& a_reader, Json::Value& o_config) {
        return a_reader.parse(a_data, a_data + a_length, o_config);
    }, o_map);
}

/**
 * @brief Set H2EMap.
 *
 * param a_parse
 * param o_map
 */
void ngx::casper::broker::cdn::Archive::SetH2EMap (const std::function<bool(Json::Reader&, Json::Value& )>& a_parse,
                                                  ngx::casper::broker::cdn::H2EMap& o_map)
{
    Json::Value config;

    try {
        Json::Reader json_reader;
        if ( false == a_parse(json_reader, config) ) {
            // ... an error occurred ...
            const auto errors = json_reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw ::cc::Exception(( "An error occurred while parsing JSON 'h2e' payload: " +  errors[0].message + "!" ));
            } else {
                throw ::cc::Exception("An error occurred while parsing JSON 'h2e' payload!");
            }
        }
        if ( true == config.isNull( ) || false == config.isObject() ) {
            throw ::cc::Exception("Invalid JSON 'h2e' payload - an object is expected!");
        }
    } catch (const Json::Exception& a_json_exception) {
        throw ::cc::Exception(( "An error occurred while parsing JSON 'h2e' payload: " +  std::string(a_json_exception.what()) + "!" ));
    }
    
    if ( true == config.isMember("h2e") ) {
        config = config["h2e"];
    }
    
    for ( auto member : config.getMemberNames() ) {
        const Json::Value& value = config[member];
        if ( true == value.isNull() || false == value.isString() || 0 == value.asString().length() ) {
            throw ::cc::Exception(( "Invalid JSON 'h2e' payload - expected '" +  member + "' value to be a valid string!"));
        }
        o_map[member] = value.asString();
    }
    
    std::set<char> tmp;
    for ( auto it : o_map ) {
        const char ch = it.second.c_str()[0];
        if ( tmp.end() != tmp.find(ch) ) {
            throw ::cc::Exception("Duplicated 'h2e' value's first char - first char of each value must be unique!");
        }
        tmp.insert(ch);
    }
}

/**
 * @brief Load the ACT configuration.
 *
 * param a_uri
 * param o_config
 */
void ngx::casper::broker::cdn::Archive::LoadACTConfig (const std::string& a_uri, Json::Value& o_config)
{
    std::filebuf file;
    if ( nullptr == file.open(a_uri, std::ios::in) ) {
        throw ::cc::Exception(("An error occurred while opening file '" + a_uri + "' to read ACT configuration!" ));
    }    
    std::istream in_stream(&file);
    SetACTConfig([&in_stream] (Json::Reader& a_reader, Json::Value& o_config) {
        return a_reader.parse(in_stream, o_config);
    }, o_config);
}

/**
 * @brief Load the ACT configuration.
 *
 * param a_data
 * param a_length
 * param o_config
 */
void ngx::casper::broker::cdn::Archive::LoadACTConfig (const char* const a_data, size_t a_length, Json::Value& o_config)
{
    SetACTConfig([a_data, &a_length] (Json::Reader& a_reader, Json::Value& o_config) {
        return a_reader.parse(a_data, a_data + a_length, o_config);
    }, o_config);
}

/**
 * @brief Set the ACT configuration.
 *
 * param o_config
 */
void ngx::casper::broker::cdn::Archive::SetACTConfig (const std::function<bool(Json::Reader&, Json::Value& )>& a_parse, Json::Value& o_config)
{
    
    try {
        Json::Reader json_reader;
        if ( false == a_parse(json_reader, o_config) ) {
            
            const auto errors = json_reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw ::cc::Exception(("An error occurred while parsing JSON 'ast' payload: " +  errors[0].message + "!" ));
            } else {
                throw ::cc::Exception("An error occurred while parsing JSON 'h2e' payload!");
            }
        }
        if ( true == o_config.isNull( ) || false == o_config.isObject() ) {
            throw ::cc::Exception("Invalid JSON 'ast' payload - an object is expected!");
        }
    } catch (const Json::Exception& a_json_exception) {
        throw ::cc::Exception(( "An error occurred while parsing JSON 'ast' payload: " +  std::string(a_json_exception.what()) + "!" ));
    }
    
    const char* const from   = "x-casper-";
    const size_t      length = strlen(from);
    std::string       name;
    Json::Value&      variables = o_config["variables"];
    Json::Value       new_variables = Json::Value(Json::ValueType::objectValue);
    
    for ( auto type : variables.getMemberNames() ) {
        Json::Value& object = variables[type];
        
        new_variables[type] = Json::Value(Json::ValueType::arrayValue);
        
        for ( auto member : object.getMemberNames() ) {
            const Json::Value& value = object[member];
            name = member;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            auto pos = name.find(from);
            if ( std::string::npos == pos ) {
                throw ::cc::Exception(
                    ( "An error occurred while validating JSON 'ast' config field '" + name + "' - prefix '" + from + "' not found!" )
                );
                break;
            }
            name.replace(pos, length, "");
            for ( std::string::size_type idx = 0 ; ( idx = name.find('-', idx) ) != std::string::npos ; ) {
                name.replace(idx, 1, "_");
                idx += 1;
            }
            Json::Value& new_variable = new_variables[type].append(Json::Value(Json::ValueType::objectValue));
            new_variable["header"]  = member;
            new_variable["name"]    = name;
            new_variable["default"] = value;
        }
    }
    
    Json::Value& hex_variables = new_variables["hex"];
    if ( false == hex_variables.isNull() ) {
        for ( Json::ArrayIndex idx = 0 ; idx < hex_variables.size() ; ++idx ) {
            char* end_ptr = nullptr;
            const auto value = std::strtoull(hex_variables[idx]["default"].asCString(), &end_ptr, 16);
            if ( nullptr == end_ptr ) {
                throw ::cc::Exception(
                    "An error occurred while validating JSON 'ast' config hex variable '%s' - could not convert value '%s' to uint64_t!",
                     hex_variables[idx]["name"].asCString(), hex_variables[idx]["default"].asCString()
                );
            } else {
	      hex_variables[idx]["default"] = static_cast<Json::UInt64>(value);
            }
        }
    }
    
    o_config["variables"] = new_variables;
}
