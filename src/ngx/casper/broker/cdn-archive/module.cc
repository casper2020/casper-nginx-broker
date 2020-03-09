/**
 * @file module.h
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

#ifdef __APPLE__
#pragma mark - ARCHIVE MODULE
#endif

#include "ngx/casper/broker/cdn-archive/module.h"

#include "ngx/casper/broker/cdn-archive/module/version.h"

#include "ngx/casper/broker/cdn-common/errors.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#include "ngx/casper/broker/tracker.h"

#include "ngx/ngx_utils.h"

#include "ngx/version.h"

#include "cc/fs/file.h" // XAttr

#include "cc/utc_time.h"

#include <algorithm>

const char* const ngx::casper::broker::cdn::archive::Module::sk_rx_content_type_ = "application/octet-stream";
const char* const ngx::casper::broker::cdn::archive::Module::sk_tx_content_type_ = "application/vnd.api+json;charset=utf-8";

ngx::casper::broker::cdn::Archive::Settings ngx::casper::broker::cdn::archive::Module::s_archive_settings_ = {
    /* directories_ */ {
        /* temporary_prefix_ */ "",
        /* archive_prefix_ */ ""
    },
    /* quarantine_ */ {
      /* validity_ */ 0,
      /* directory_prefix_ */ ""
    }
};

ngx::casper::broker::cdn::archive::db::Synchronization::Settings ngx::casper::broker::cdn::archive::Module::s_sync_registry_settings_ = {
    /* tube_     */ "",
    /* ttr_      */ 300, // 5 min
    /* validity_ */ 290, // 4 min and 50 sec
    /* slaves_   */ Json::Value(Json::ValueType::nullValue)
};

#ifdef __APPLE__
#pragma mark - CONSTRUCTOR(S) / DESTRUCTOR
#endif

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_cdn_archive_loc_conf
 * @param a_ngx_cdn_archive_loc_conf
 */
ngx::casper::broker::cdn::archive::Module::Module (const ngx::casper::broker::Module::Config& a_config,
                                                   const ngx::casper::broker::Module::Params& a_params,
                                                   ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_archive_module_loc_conf_t& a_ngx_cdn_archive_loc_conf)
: ngx::casper::broker::cdn::common::Module("cdn_archive", a_config, a_params,
                                           /* a_settings */
                                           {
                                               /* disposition_ */
                                               {
                                                   /* default_value_ */
                                                   std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.response.content_disposition.data), a_ngx_loc_conf.cdn.response.content_disposition.len),
                                                   /* allow_from_url_ */
                                                   ( NGX_HTTP_HEAD == a_config.ngx_ptr_->method || NGX_HTTP_GET == a_config.ngx_ptr_->method )
                                               },
                                               /* implemented_methods_ */
                                               {
                                                   NGX_HTTP_HEAD,
                                                   NGX_HTTP_GET,
                                                   NGX_HTTP_POST,
                                                   NGX_HTTP_PUT,
                                                   NGX_HTTP_PATCH,
                                                   NGX_HTTP_DELETE
                                               },
                                               /* required_content_type_methods_ */
                                               {
                                                   NGX_HTTP_POST,
                                                   NGX_HTTP_PUT
                                               },
                                               /* required_content_length_methods_ */
                                               {
                                                   NGX_HTTP_POST,
                                                   NGX_HTTP_PUT
                                               },
                                               /* redirect_ */
                                               {
                                                   /* protocol_ */ &a_ngx_loc_conf.cdn.redirect.protocol,
                                                   /* host_     */ &a_ngx_loc_conf.cdn.redirect.host,
                                                   /* port_     */ &a_ngx_loc_conf.cdn.redirect.port,
                                                   /* path_     */ &a_ngx_loc_conf.cdn.redirect.path,
                                                   /* location_ */ &a_ngx_loc_conf.cdn.redirect.location
                                               }
                                           }
 ),
 archive_(/* a_archivist */ NGX_INFO, /* a_writer */ NGX_CASPER_BROKER_CDN_ARCHIVE_MODULE_INFO, s_ast_config_, ctx_.request_.headers_, s_h2e_map_, s_archive_settings_.directories_.archive_prefix_),
 x_id_("X-NUMERIC-ID"), x_moves_uri_("X-STRING-ID"),
 x_content_type_(/* a_default */ sk_rx_content_type_),
 x_content_disposition_(
    /* a_default */ std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.response.content_disposition.data), a_ngx_loc_conf.cdn.response.content_disposition.len)
 ),
 db_ {
     /* data_ */
     {
         /* new_ */ {
             /* id_   */ "",
             /* uri_  */ "",
             /* size_ */  0
         },
         /* old_ */ {
             /* id_   */ "",
             /* uri_  */ "",
             /* size_ */  0
         },
         /* headers_ */  {},
         /* xattrs_   */ {},
         /* billing_ */ {
             /* id_   */ 0,
             /* type_ */ "",
             /* unaccounted_ */ false
         }
     },
     /* registry_*/ nullptr,
     /* billing_ */ nullptr
 },
 job_(ctx_, this, a_ngx_loc_conf)
{
    // ...
    body_read_supported_methods_ = {
        NGX_HTTP_POST, NGX_HTTP_DELETE, NGX_HTTP_PUT
    };
    // ...
    body_read_bypass_methods_ = {
        NGX_HTTP_HEAD, NGX_HTTP_GET, NGX_HTTP_PATCH
    };
    // ... one shot
    if ( 0 == s_archive_settings_.directories_.archive_prefix_.length() ) {
        // ... required directories ...
        s_archive_settings_.directories_.temporary_prefix_ = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.directories.temporary_prefix.data), a_ngx_loc_conf.cdn.directories.temporary_prefix.len));
        s_archive_settings_.directories_.archive_prefix_   = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.directories.archive_prefix.data), a_ngx_loc_conf.cdn.directories.archive_prefix.len));
        // ... quarentine ...
        s_archive_settings_.quarantine_.directory_prefix_  = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_cdn_archive_loc_conf.quarantine.directory_prefix.data),a_ngx_cdn_archive_loc_conf.quarantine.directory_prefix.len));
        s_archive_settings_.quarantine_.validity_          = static_cast<size_t>(a_ngx_cdn_archive_loc_conf.quarantine.validity);
    }
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::archive::Module::~Module ()
{
    if ( nullptr != db_.synchronization_ ) {
        delete db_.synchronization_;
    }
    if ( nullptr != db_.billing_ ) {
        delete db_.billing_;
    }
}

#ifdef __APPLE__
#pragma mark - SETUP & RUN STEPS
#endif

/**
 * @brief 'Pre-run' setup.
 *
 * @return NGX_OK on success, on failure NGX_ERROR.
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::Setup ()
{
    
    // ... track if we're moving a file ...
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method ) {
        x_moves_uri_.Set({"X-CASPER-MOVES-URI"}, ctx_.request_.headers_, &sk_empty_string_);
        if ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) {
            // ... don't inforce 'Content-Length' header
            const auto it = settings_.required_content_length_methods_.find(NGX_HTTP_POST);
            if ( settings_.required_content_length_methods_.end() != it ) {
                settings_.required_content_length_methods_.erase(it);
            }
        }
    }

    const ngx_int_t crv = ngx::casper::broker::cdn::common::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }
    
    // ... grab local configuration ...
    const ngx_http_casper_broker_cdn_archive_module_loc_conf_t* loc_conf =
        (const ngx_http_casper_broker_cdn_archive_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_cdn_archive_module);
    if ( NULL == loc_conf ) {
        // ... set error
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                    "An error occurred while fetching this module configuration - nullptr!"
        );
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    
    //
    // Load 'one-shot' config.
    //
    
    // ... module db sync settings  ...
    if ( true == s_sync_registry_settings_.slaves_.isNull() ) {
        
        s_sync_registry_settings_.tube_ = (
                                           loc_conf->sync.tube.len > 0
                                           ?
                                            std::string(reinterpret_cast<const char*>(loc_conf->sync.tube.data), loc_conf->sync.tube.len)
                                           :
                                            ""
        );
        s_sync_registry_settings_.ttr_      = static_cast<uint64_t>(loc_conf->sync.ttr);
        s_sync_registry_settings_.validity_ = static_cast<uint64_t>(loc_conf->sync.validity);
        
        try {
            const char* const start_ptr = reinterpret_cast<const char* const>(loc_conf->sync.slaves.data);
            const char* const end_ptr   = start_ptr + loc_conf->sync.slaves.len;
            Json::Value  hosts;
            Json::Reader json_reader;
            if ( false == json_reader.parse(start_ptr, end_ptr, hosts) ) {
                // ... an error occurred ...
                const auto errors = json_reader.getStructuredErrors();
                if ( errors.size() > 0 ) {
                    throw ::cc::Exception("An error occurred while parsing JSON 'slaves' payload: %s",
                                           errors[0].message.c_str()
                    );
                } else {
                    throw ::cc::Exception("An error occurred while parsing JSON 'slaves' payload!");
                }
            }
            if ( true == hosts.isNull( ) || false == hosts.isArray() ) {
                throw ::cc::Exception("An error occurred while parsing JSON 'slaves' payload: an array is expected!");
            }

            s_sync_registry_settings_.slaves_ = Json::Value(Json::ValueType::arrayValue);

            for ( Json::ArrayIndex idx = 0 ; idx < hosts.size() ; ++idx ) {
                Json::Value& slave = s_sync_registry_settings_.slaves_.append(Json::Value(Json::ValueType::objectValue));
                slave["url"] = hosts[idx];
            }
            
        } catch (const Json::Exception& a_json_exception) {
            s_sync_registry_settings_.slaves_ = Json::Value::null;
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_json_exception.what());
        } catch (const ::cc::Exception& a_cc_exception) {
            s_sync_registry_settings_.slaves_ = Json::Value::null;
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
        
    }

    // ... if an error was set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    
    // ... read x_id ..
    try {
        x_archived_by_.Set({"X-CASPER-ARCHIVED-BY", "USER-AGENT"}, ctx_.request_.headers_, /* a_default */ nullptr, /* a_first_of */ true);
        switch (ctx_.ngx_ptr_->method) {
            case NGX_HTTP_HEAD:
                break;
            case NGX_HTTP_GET:
            {
                const bool validate_integrity = false;
                x_validate_integrity_.Set(ctx_.request_.headers_, &validate_integrity);
            }
                break;
            case NGX_HTTP_POST:
                // ... read mandatory header(s) ...
                x_id_.Set({"X-CASPER-ENTITY-ID", "X-CASPER-USER-ID"}, ctx_.request_.headers_);
                x_billing_id_.Set(ctx_.request_.headers_);
                x_billing_type_.Set(ctx_.request_.headers_);
                // ... track billing info ...
                db_.sync_data_.billing_.id_   = (const uint64_t&)x_billing_id_;
                db_.sync_data_.billing_.type_ = (const std::string&)x_billing_type_;
                // ... check if we're moving a file ...
                if ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) {
                    db_.sync_data_.old_.uri_  = ( s_archive_settings_.directories_.temporary_prefix_ + (const std::string&)x_moves_uri_ );
                    if ( false == ::cc::fs::File::Exists(db_.sync_data_.old_.uri_) ) {
                        throw ngx::casper::broker::cdn::NotFound();
                    }
                    db_.sync_data_.old_.size_ = ::cc::fs::File::Size(db_.sync_data_.old_.uri_);
                    ::cc::fs::File::Name(db_.sync_data_.old_.uri_, db_.sync_data_.old_.id_);
                }
                break;
            case NGX_HTTP_PUT:
            case NGX_HTTP_PATCH:
            case NGX_HTTP_DELETE:
                // ... ensure file exists and get 'id' header name ...
                archive_.Open(ctx_.request_.uri_, [this](const std::string& a_xhvn) -> ngx::casper::broker::cdn::Archive::Mode {
                    // ... track 'id' header name ...
                    x_id_.Set({a_xhvn}, ctx_.request_.headers_);
                    // ... track 'old' file info ...
                    db_.sync_data_.old_.id_   = archive_.id();
                    db_.sync_data_.old_.uri_  = archive_.uri();
                    db_.sync_data_.old_.size_ = archive_.size();
                    // ... track billing info ...
                    archive_.GetXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.id"  , db_.sync_data_.billing_.id_);
                    archive_.GetXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.type", db_.sync_data_.billing_.type_);
                    // ... cancel 'open' ...
                    return ngx::casper::broker::cdn::Archive::Mode::NotSet;
                });
                break;
            default:
                NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
                break;
        }
            
    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
        NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_, a_not_found.what());
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
        NGX_BROKER_MODULE_SET_FORBIDDEN_ERROR(ctx_, a_forbidden.what());
    }  catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST(ctx_, a_bad_request.what());
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_internal_server_error.what());
    } catch (const ::cc::Exception& a_cc_exception) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
    } catch (...) {
        try {
            CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    
    // ... if an error was set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }

    // ... intercept body?
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method || NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
        // ... yes, but first ensure file creation and important data collection ...
        try {
            // .. create a new file ...
            archive_.Create(x_id_, content_length());
            // ... track 'new' file info ...
            db_.sync_data_.new_.id_   = archive_.id();
            db_.sync_data_.new_.uri_  = archive_.uri();
            if ( NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
                // ... PATCH won't modify file contents, keep old size ...
                db_.sync_data_.new_.size_ = db_.sync_data_.old_.size_;
            } else if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method ) {
                if ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) {
                    // ... MOVE keep old size ...
                    db_.sync_data_.new_.size_ = db_.sync_data_.old_.size_;
                } else {
                    db_.sync_data_.new_.size_ = static_cast<uint64_t>((const size_t&)content_length());
                }
            } else { // NGX_HTTP_PUT
                db_.sync_data_.new_.size_ = static_cast<uint64_t>((const size_t&)content_length());
            }
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
        // ... if still 'good to go' ...
        if ( NGX_OK == ctx_.response_.return_code_ && NGX_HTTP_PATCH != ctx_.ngx_ptr_->method ) {
            // ... set body interceptor ...
            ctx_.request_.body_interceptor_ = {
                /* dst_ */
                [this] () -> const std::string& {
                    return archive_.uri();
                },
                /* writer_ */
                [this](const unsigned char* const a_data, const size_t& a_size) -> ssize_t {
                    try {
                        return static_cast<ssize_t>(archive_.Write(a_data, a_size));
                    } catch (const ::cc::Exception& a_cc_exception) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
                        return -1;
                    } catch (...) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while writing data to file!");
                        return -1;
                    }
                },
                /* flush_ */
                [this]() -> ssize_t {
                    try {
                        archive_.Flush();
                    } catch (const ::cc::Exception& a_cc_exception) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
                        return -1;
                    } catch (...) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while flushing to file!");
                        return -1;
                    }
                    return 0;
                },
                /* close_ */
                [this]() -> ssize_t {
                    try {
                        //
                        // NOTICE: do no close it here
                        //
                        // just a flush if an error occures during this request temporary file must be deleted
                        // ( if file not closed, archive object instance will delete it )
                        //
                        archive_.Flush();
                    } catch (const ::cc::Exception& a_cc_exception) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
                        return -1;
                    } catch (...) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while closing file!");
                        return -1;
                    }
                    return 0;
                }
            };
        }
    }
    
    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Ensure requirements are respected before executing requrest..
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::Run ()
{
    // ... if an error occurred...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    // ... start as bad request ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;
    // ... first, ensure billing settings are acceptable ...
    if ( NGX_HTTP_HEAD == ctx_.ngx_ptr_->method || NGX_HTTP_GET == ctx_.ngx_ptr_->method || NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
        // ... except on this methods, nothing to check here ...
        return Perform();
    } else if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method || NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) {
        // ... check billing limits for this request,
        // ( if acceptable then Perform will be called ) ...
        return FetchBillingInformation();
    } else {
        // ... not expecting any other method ...
        NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    }
    
    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::Perform ()
{
    // ... process request ...
    try {
        switch (ctx_.ngx_ptr_->method) {
            case NGX_HTTP_HEAD:
                HEAD();
                break;
            case NGX_HTTP_GET:
                GET();
                break;
            case NGX_HTTP_POST:
                POST();
                break;
            case NGX_HTTP_PUT:
                PUT();
                break;
            case NGX_HTTP_PATCH:
                PATCH();
                break;
            case NGX_HTTP_DELETE:
                DELETE();
                break;
            default:
                NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
                break;
        }
    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
        NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_, a_not_found.what());
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
        NGX_BROKER_MODULE_SET_FORBIDDEN_ERROR(ctx_, a_forbidden.what());
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_internal_server_error.what());
    } catch (const ::cc::Exception& a_ev_exception) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_ev_exception.what());
    } catch (...) {
        try {
            CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    // ... register archive operation ...
    if ( NGX_OK != RegisterSyncOperation() ) {
        // ... something went wrong, try to rollback action ...
        TryRollback();
    }
    // ... we're done ...
    return ctx_.response_.return_code_;
}

#ifdef __APPLE__
#pragma mark - HTTP METHODS
#endif

/**
 * @brief Process an HTTP HEAD request.
 */
void ngx::casper::broker::cdn::archive::Module::HEAD ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    //   <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //       - X-CASPER-USER-ID
    //       - ... see, AST config ...
    //
    //   BODY: NONE
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //        - Content-Length: body size in bytes
    //        - Content-Disposition: in-headers['Content-Disposition'] || nginx_casper_broker_cdn_content_disposition ;
    //                               filename = 'X-CASPER-FILENAME' || url args || com.cldware.archive.filename || <id>[.ext]
    //
    //      optional:
    //
    //        - Content-Type: if xattr ‘com.cldware.content-type‘ was previously set
    //
    //   BODY: NONE
    //
    //   STATUS CODE ( one of ):
    //
    //       - 200 OK
    //       - 403 FORBIDDEN             - 'r' access expression evaluated to false
    //       - 404 NOT FOUND             - if file does not exist
    //       - 500 INTERNAL SERVER ERROR
    //
    
    try {
        
        //
        // ENSURE required headers ( archive_.Open will take care of that )
        //
        // ... nop ...
        
        //
        // READ optional header(s)
        //
        x_content_disposition_.Set(ctx_.request_.headers_, /* a_default */ &content_disposition().disposition());
        if ( true == content_disposition().IsSet() ) {
            x_filename_.Set (ctx_.request_.headers_, /* a_default */ &content_disposition().filename());
        } else {
            x_filename_.Set (ctx_.request_.headers_, /* a_default */ &x_content_disposition_.filename());
        }

        // ... this headers can default to their xattr value ( if set ) ...
        //
        // ENSURE required headers ( archive_.Open will take care of that )
        //
        // ... prepare reader ...
        archive_.Open(ctx_.request_.uri_);

        // ... load 'minimum' xattrs for 'GET' response ...
        const bool x_content_type_can_be_overriden_by_xattr = ( false == x_content_type_.IsSet() || 0 == x_content_type_.value().length() );
        if ( false == x_content_type_.IsSet() || true == x_content_type_can_be_overriden_by_xattr ) {
            archive_.GetXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type", x_content_type_);
        }
        
        x_content_disposition_ = {
            { "disposition", x_content_disposition_.disposition()  },
            { "filename"   , archive_.name(x_filename_)            }
        };

        // ... no need to retrieve xattrs ...
        r_info_.attrs_.clear();
        
        // ... close archive ...
        archive_.Close(/*a_for_redirect */ true, &r_info_);
        
        // .. since we're closing a read-only, re-map new to old ...
        r_info_.old_id_  = r_info_.new_id_;
        r_info_.old_uri_ = r_info_.new_uri_;
        r_info_.new_id_  = "";
        r_info_.new_uri_ = "";

        // ... set redirect headers ...
        ctx_.response_.headers_["Content-Length"]      = std::to_string(::cc::fs::File::Size(r_info_.old_uri_));
        ctx_.response_.headers_["Content-Type"]        = (const std::string&)x_content_type_;
        ctx_.response_.headers_["Content-Disposition"] = x_content_disposition_.value();
        
        //
        // RESPONSE - SYNCHRONOUS VIA RETURN
        //
        ctx_.response_.redirect_.asynchronous_ = false;
        ctx_.response_.status_code_            = NGX_HTTP_OK;
        ctx_.response_.return_code_            = NGX_OK; // synchronous request, we're done
              
        // ... reset r-info ...
        r_info_.old_id_  = "";
        r_info_.old_uri_ = "";

    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_not_found;
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_forbidden;
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_internal_server_error;
    } catch (const cc::Exception& a_cc_exception) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_cc_exception;
    } catch (...) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
    }
}

/**
 * @brief Process an HTTP GET request.
 */
void ngx::casper::broker::cdn::archive::Module::GET ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    //   <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //       - X-CASPER-USER-ID
    //       - ... see, AST config ...
    //
    //     optional:
    //
    //       - X-CASPER-CONTENT-DISPOSITION
    //       - X-CASPER-FILENAME
    //
    //   BODY: NONE
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //        - Content-Length: body size in bytes
    //        - Content-Disposition: in-headers['Content-Disposition'] || nginx_casper_broker_cdn_content_disposition ;
    //                               filename = 'X-CASPER-FILENAME' || url args || com.cldware.archive.filename || <id>[.ext]
    //
    //      optional:
    //
    //        - Content-Type: if xattr ‘com.cldware.content-type‘ was previously set
    //
    //   BODY: file contents
    //
    //   STATUS CODE ( one of ):
    //
    //       - 200 OK
    //       - 403 FORBIDDEN             - 'r' access expression evaluated to false
    //       - 404 NOT FOUND             - if file does not exist
    //       - 500 INTERNAL SERVER ERROR
    //
 
    try {
        
        //
        // ENSURE required headers ( archive_.Open will take care of that )
        //
        // ... nop ...
        
        //
        // READ optional header(s)
        //
        x_content_disposition_.Set(ctx_.request_.headers_, /* a_default */ &content_disposition().disposition());
        if ( true == content_disposition().IsSet() ) {
            x_filename_.Set (ctx_.request_.headers_, /* a_default */ &content_disposition().filename());
        } else {
            x_filename_.Set (ctx_.request_.headers_, /* a_default */ &x_content_disposition_.filename());
        }

        // ... this headers can default to their xattr value ( if set ) ...
        //
        // ENSURE required headers ( archive_.Open will take care of that )
        //
        // ... prepare reader ...
        archive_.Open(ctx_.request_.uri_);

        // ... load 'minimum' xattrs for 'GET' response ...
        const bool x_content_type_can_be_overriden_by_xattr = ( false == x_content_type_.IsSet() || 0 == x_content_type_.value().length() );
        if ( false == x_content_type_.IsSet() || true == x_content_type_can_be_overriden_by_xattr ) {
            archive_.GetXAttr(XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type", x_content_type_);
        }
        
        x_content_disposition_ = {
            { "disposition", x_content_disposition_.disposition()  },
            { "filename"   , archive_.name(x_filename_)            }
        };

        // ... no need to retrieve xattrs ...
        r_info_.attrs_.clear();
        
        // ... close archive ...
        archive_.Close(/*a_for_redirect */ true, &r_info_);
        
        // .. since we're closing a read-only, re-map new to old ...
        r_info_.old_id_  = r_info_.new_id_;
        r_info_.old_uri_ = r_info_.new_uri_;
        r_info_.new_id_  = "";
        r_info_.new_uri_ = "";

        // ... validate file before delivering it?
        if ( x_validate_integrity_.IsSet() && true == x_validate_integrity_ ) {
            archive_.Validate('/' + r_info_.old_id_);
        }

        // ... set redirect headers ...
        ctx_.response_.headers_["X-CASPER-CONTENT-TYPE"]        = (const std::string&)x_content_type_;
        ctx_.response_.headers_["X-CASPER-CONTENT-DISPOSITION"] = x_content_disposition_.value();
        ctx_.response_.headers_["X-CASPER-FILE-LOCAL-TRY"]      =
            ( r_info_.old_uri_.c_str() + s_archive_settings_.directories_.archive_prefix_.length() );
        
        //
        // RESPONSE - VIA INTERNAL REDIRECT
        //
        ctx_.response_.redirect_.file_         = r_info_.old_id_;
        ctx_.response_.redirect_.internal_     = true;
        ctx_.response_.redirect_.asynchronous_ = false;
        ctx_.response_.status_code_            = NGX_HTTP_OK;
        ctx_.response_.return_code_            = NGX_DECLINED; // synchronous request must return NGX_DECLINED so it can be properly handled in content phase
              
        // ... reset r-info ...
        r_info_.old_id_  = "";
        r_info_.old_uri_ = "";

    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_not_found;
    } catch (const ngx::casper::broker::cdn::Forbidden& a_forbidden) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_forbidden;
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_internal_server_error;
    } catch (const cc::Exception& a_cc_exception) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        throw a_cc_exception;
    } catch (...) {
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
    }
}

/**
 * @brief Process an HTTP POST request.
 */
void ngx::casper::broker::cdn::archive::Module::POST ()
{
    //
    // URN: /<location>/
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //            or
    //         X-CASPER-USER-ID
    //       - X-CASPER-ACCESS      - it will set or replace the xattr ‘com.cldware.access‘ value
    //
    //     optional:
    //
    //       - Content-Type         - it will set the xattr ‘com.cldware.content-type‘ value
    //       - X-CASPER-FILENAME    - it will set the xattr ‘com.cldware.filename‘ value
    //         or
    //       - X-CASPER-MOVES-URI
    //
    //   BODY: FILE CONTENT
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - Content-Type: application/vnd.api+json;charset=utf-8
    //
    //   BODY: { "type": "cdn-archive", "id": "aBBcccccc", "attributes": { "name": "", "path": "", "content-type": "", "content-length": 0 } } or an JSONAPI error object
    //
    //   STATUS CODE ( one of ):
    //
    //       - 200 OK
    //       - 403 FORBIDDEN             - 'w' access expression evaluated to false
    //       - 500 INTERNAL SERVER ERROR
    //
    
    //
    // ENSURE required headers
    //
    // ... ( archive_.Open will take care of all other required headers ) ...
    //
    x_access_.Set(ctx_.request_.headers_);
    
    //
    // READ optional header(s)
    //
    // ... Content-Type ...
    x_content_type_.Set (ctx_.request_.headers_, /* a_default */ &sk_empty_string_);
    
    // ...  X-CASPER-FILENAME
    x_filename_.Set(ctx_.request_.headers_, /* a_default */ &sk_empty_string_);
  
    // finally: write attributes
    
    // writer_        = NGX_CASPER_BROKER_CDN_ARCHIVE_MODULE_INFO
    // x_archived_by_ = X-CASPER-ARCHIVED-BY | USER_AGENT | NGX_INFO
    
    // [ X ] [ MANDATORY ] com.cldware.archive.id                           = <aBBcccccc>
    // [ X ] [ MANDATORY ] com.cldware.archive.content-type                 = <string>
    // [ X ] [ MANDATORY ] com.cldware.archive.content-length               = <numeric>
    // [ X ] [           ] com.cldware.archive.filename                     = <string>
    // [ X ] [ MANDATORY ] com.cldware.archive.md5                          = <hex string>
    // [ X ] [ MANDATORY ] com.cldware.archive.archivist                    = NGX_INFO
    // [ X ] [ MANDATORY ] com.cldware.archive.archived.by                  = x_archived_by_
    // [ X ] [ MANDATORY ] com.cldware.archive.archived.at                  = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [ X ] [ MANDATORY ] com.cldware.archive.created.by                   = writer_
    // [ X ] [ MANDATORY ] com.cldware.archive.created.at                   = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [ X ] [           ] com.cldware.archive.modified.by                  = writer_
    // [ X ] [           ] com.cldware.archive.modified.at                  = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [   ] [           ] com.cldware.archive.modified.history             = [{"requestee":"curl/7.54.0","at":"YYYY-MM-DDTHH:MM:SS+ZZ:ZZ","operation":"Update"}]

    // [ X ] [ MANDATORY ] com.cldware.archive.billing.id                   = <numeric>
    // [ X ] [ MANDATORY ] com.cldware.archive.billing.type                 = <string>

    // [ X ] [ MANDATORY ] com.cldware.archive.permissions.hr               = <string>, ex: 'rwd= user_id = 1'
    // [ X ] [ MANDATORY ] com.cldware.archive.permissions.ast.r            =
    // [ X ] [ MANDATORY ] com.cldware.archive.permissions.ast.w            =
    // [ X ] [ MANDATORY ] com.cldware.archive.permissions.ast.d            =

    // [ X ] [           ] com.cldware.archive.xattrs.created.at            = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [ X ] [           ] com.cldware.archive.xattrs.created.by            = writer_
    // [ X ] [           ] com.cldware.archive.xattrs.modified.at           = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [ X ] [           ] com.cldware.archive.xattrs.modified.by           = writer_
    
    // ... when moving a file uploaded by ul module ...
    // [ X ] [           ] com.cldware.upload.protocol                      = HTTP/1.1
    // [ X ] [           ] com.cldware.upload.method                        = POST
    // [ X ] [           ] com.cldware.upload.user-agent                    = curl/7.54.0
    // [ X ] [           ] com.cldware.upload.origin                        = http://localhost:3201
    // [ X ] [           ] com.cldware.upload.referer                       = http://localhost:3201/test
    // [ X ] [           ] com.cldware.upload.ip                            = 127.0.0.1
    // [ X ] [           ] com.cldware.upload.created.by                    = ul
    // [ X ] [           ] com.cldware.upload.created.at                    = YYYY-MM-DDTHH:MM:SS+ZZ:ZZ
    // [ X ] [           ] com.cldware.upload.uri                           = <local URI>
    // [ X ] [           ] com.cldware.upload.content-type                  = <string>
    // [ X ] [           ] com.cldware.upload.content-length                = <numeric>
    
    std::map<std::string, std::string> attrs;
    
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"] = archive_.id();

    // ... set content info ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type"]   = (const std::string&)content_type();
    
    // ... set 'civilized' file name ...
    if ( true == x_filename_.IsSet() && x_filename_ != sk_empty_string_ ) {
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename"] = (const std::string&)x_filename_;
        r_info_.attrs_.push_back(
             /*  name_                                                  , key_  , tag_                        , optional_ , value_, serialize_ */
             { XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename"      , "name", Json::ValueType::stringValue, false     ,     "",       true  }
        );
    }
    
    // ... write tracking info ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.archived.by"] = (const std::string&)x_archived_by_;
    
    // ... write permissions ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr"] = (const std::string&)x_access_;
    
    // ... billing info ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.id"]   = (const std::string&)x_billing_id_;
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.billing.type"] = (const std::string&)x_billing_type_;

    //
    // X-CASPER-MOVES-URI OR CREATE?
    //
    if ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) {
        // ... set final attribute(s) ...
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length"] = std::to_string(db_.sync_data_.old_.size_);
        // ... move ( includes closing ) and write extended attributes ...
        archive_.Move(db_.sync_data_.old_.uri_, attrs, sk_preserved_upload_attrs_, /* a_backup */ true, r_info_);
    } else {
        // ... set final attribute(s) ...
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length"] = std::to_string((const size_t&)content_length());
        // ... close it and write extended attributes ...
        archive_.Close(attrs, sk_preserved_upload_attrs_, r_info_);
    }

    // ... set success response and we're done ...
    SetSuccessResponse(r_info_);
}

/**
 * @brief Process an HTTP PUT request.
 */
void ngx::casper::broker::cdn::archive::Module::PUT ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    // <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //       - X-CASPER-USER-ID
    //       - ... see, AST config ...
    //
    //     optional:
    //
    //       - Content-Type - it will set or replace the xattr ‘com.cldware.content-type‘ value
    //
    //   BODY: NEW FILE CONTENT
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - Content-Type: application/vnd.api+json;charset=utf-8
    //
    //   BODY: { "type": "cdn-archive", "id": "aBBcccccc", "attributes": { "name": "", "path": "", "content-type": "", "content-length": 0 } } or an JSONAPI error object
    //
    //   STATUS CODE ( one of ):
    //
    //       - 200 OK                    - accepted, attributes changes
    //       - 204 NO CONTENT            - accepted, one or more attributes changed
    //       - 403 FORBIDDEN             - 'w' access expression evaluated to false
    //       - 404 NOT FOUND             - if file does not exist
    //       - 500 INTERNAL SERVER ERROR
    //
    
    //
    // ENSURE required headers
    //
    // ... nop ...
    
    //
    // ... since a new 'unique' file is open ...
    //
    std::map<std::string, std::string> attrs;

    //
    // READ optional header(s)
    //
    try {
        x_content_type_.Set(ctx_.request_.headers_);
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type"] = (const std::string&)content_type();
    } catch (const ::cc::Exception& /* a_cc_exception */) {
        // ... eat it ...
    }
    
    //
    // SET required xattr
    //
    // ... new content-length ...
    attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length"] = std::to_string((const size_t&)content_length());
    
    // ... update archive ( close will also be done by this function call ) ...
    archive_.Update(ctx_.request_.uri_, attrs, sk_preserved_upload_attrs_, /* a_backup */ true, x_archived_by_, r_info_);
    
    // ... set success response and we're done ...
    SetSuccessResponse(r_info_);
}

/**
 * @brief Process an HTTP PATCH request.
 */
void ngx::casper::broker::cdn::archive::Module::PATCH ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    // <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //       - X-CASPER-USER-ID
    //       - ... see, AST config ...
    //
    //     optional:
    //
    //       - X-CASPER-ACCESS   - it will set or replace the xattr ‘com.cldware.access‘ value
    //       - X-CASPER-FILENAME - it will set or replace the xattr ‘com.cldware.filename‘ value
    //
    //   BODY: NONE
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - Content-Type: application/vnd.api+json;charset=utf-8
    //       - Content-Length:
    //
    //   BODY: { "type": "cdn-archive", "id": "aBBcccccc", "attributes": { "name": "", "path": "", "type": "", "length": 0 } } or an JSONAPI error object
    //
    //   STATUS CODE ( one of ):
    //
    //       - 200 OK
    //       - 403 FORBIDDEN             - 'w' access expression evaluated to false
    //       - 404 NOT FOUND             - if file does not exist
    //       - 500 INTERNAL SERVER ERROR
    //
        
    //
    // ENSURE required headers ( archive_.Patch will take care of that )
    //
    // ... nop ...
    std::map<std::string, std::string> attrs;
    
    // ... READ optional header(s) ...
    try {
        x_access_.Set(ctx_.request_.headers_);
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr"] = (const std::string&)x_access_;
        r_info_.attrs_.push_back(
                                 /*  name_                                                  , key_          , tag_                        , optional_ , value_, serialize_ */
                                 { XATTR_ARCHIVE_PREFIX "com.cldware.archive.permissions.hr" , "permissions" , Json::ValueType::stringValue, false     ,     "",       true  }
        );
    } catch (const ::cc::Exception& /* a_cc_exception */) {
        // ... eat it ...
    }

    try {
        x_filename_.Set(ctx_.request_.headers_);
        attrs[XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename"] = (const std::string&)x_filename_;
        r_info_.attrs_.push_back(
            /*  name_                                                  , key_  , tag_                        , optional_ , value_, serialize_ */
            { XATTR_ARCHIVE_PREFIX "com.cldware.archive.filename"      , "name", Json::ValueType::stringValue, false     ,     "",       true  }
        );
    } catch (const ::cc::Exception& /* a_cc_exception */) {
        // ... eat it ...
    }

    //
    // no need to call archive_.Close():
    //
    // 'patch' and destroy operations will take care of that
    //
    
    // ... any changes?
    if ( 0 != attrs.size() ) {
        // ... 'patch' file attributes ...
        archive_.Patch(ctx_.request_.uri_, attrs, sk_preserved_upload_attrs_, /* a_backup */ true, r_info_);
        // ... set success response and we're done ...
        SetSuccessResponse(r_info_);
    } else {
        // ... cancel archive creation operation ...
        archive_.Destroy();
        // ... set not modified response and we're done ...
        SetNotModifiedResponse();
    }
}

/**
 * @brief Process an HTTP DELETE request.
 */
void ngx::casper::broker::cdn::archive::Module::DELETE ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    // <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
    //
    //       - X-CASPER-ENTITY-ID
    //       - X-CASPER-USER-ID
    //       - ... see, AST config ...
    //
    //     optional:
    //
    //   BODY: NONE
    //
    // RESPONSE:
    //
    //   HEADERS:
    //
    //     required:
    //
    //   BODY: NONE
    //
    //   STATUS CODE ( one of ):
    //
    //       - 202 ACCEPTED
    //       - 204 NO CONTENT
    //       - 403 FORBIDDEN             - 'd' access expression evaluated to false
    //       - 404 NOT FOUND             - if file does not exist
    //       - 500 INTERNAL SERVER ERROR
    //
    
    //
    // ENSURE required headers ( archive_.Delete will take care of that )
    //
    // ... nop ...
    
    // ... 'delete' file by moving it to quarantine ...
    archive_.Delete(ctx_.request_.uri_, &s_archive_settings_.quarantine_, r_info_);

    // ... track 'new' file info ...
    db_.sync_data_.new_.id_  = r_info_.new_id_;
    db_.sync_data_.new_.uri_ = r_info_.new_uri_;

    // ...set no content response ...
    SetNoContentResponse();
}

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Billing
#endif

/**
 * @brief Call this method to fetch billing information.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::FetchBillingInformation ()
{
    db_.billing_ = new ngx::casper::broker::cdn::common::db::Billing(ctx_.loggable_data_ref_);
    // ... from now on the response will be asynchronous ...
    ctx_.response_.asynchronous_ = true;
    ctx_.response_.return_code_  = NGX_OK;
    // ... gather billing info and check if there is enough space to store or update file ...
    db_.billing_->Get(db_.sync_data_.billing_.id_, db_.sync_data_.billing_.type_,
        {
            /* success_ */
            std::bind(&ngx::casper::broker::cdn::archive::Module::OnFetchBillingInformationSucceeded, this, std::placeholders::_1, std::placeholders::_2),
            /* failure_ */
            std::bind(&ngx::casper::broker::cdn::archive::Module::OnFetchBillingInformationFailed, this, std::placeholders::_1, std::placeholders::_2)
        }
    );
    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief This method will be called when billing information was retrieved.
 *
 * @param a_status HTTP status code.
 * @param a_json JSON parsed response.
 */
void ngx::casper::broker::cdn::archive::Module::OnFetchBillingInformationSucceeded (uint16_t a_status, const Json::Value& a_json)
{
    //
    bool error_set = true;
    // ... error?
    if ( 200 != a_status ) {
        // ... set error ...
        if ( 404 == a_status ) {
            NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_,
                    "Billing entity not found!"
            );
        } else {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                    ( "Unexpected status code " + std::to_string(a_status) + " while obtaining billing information!" ).c_str()
            );
        }
    } else {
        // ... mark if it's unaccounted ...
        db_.sync_data_.billing_.unaccounted_ = a_json.get("unaccounted", false).asBool();
        // ... if it's a delete ...
        if ( NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) {
            // ... pass - delete will release space ...
            error_set = false;
        } else {
            // ... check limits?
            if ( true == db_.sync_data_.billing_.unaccounted_ ) {
                // ... pass - no limit check ...
                error_set = false;
            } else {
                const int64_t accounted_space_limit = static_cast<int64_t>(a_json["accounted_space_limit"].asInt64());
                const int64_t accounted_used_space  = static_cast<int64_t>(a_json["accounted_used_space"].asInt64());
                
                int64_t op_delta_space;
                if ( NGX_HTTP_PUT == ctx_.ngx_ptr_->method ) {
                    op_delta_space = db_.sync_data_.old_.size_ - db_.sync_data_.new_.size_;
                } else {
                    op_delta_space = db_.sync_data_.new_.size_;
                }
                const int64_t new_accounted_space = accounted_used_space + op_delta_space;
                if ( new_accounted_space >= accounted_space_limit ) {
                    NGX_BROKER_MODULE_SET_PAYLOAD_TOO_LARGE_ERROR(ctx_,
                          "Billing global quota exceeded!"
                    );
                } else {
                    // ... pass - under limit ...
                    error_set = false;
                }
            }
        }
    }
    // ... if error is NOT set ...
    if ( false == error_set ) {
        // ... handle request ...
        Perform();
    } else {
        // ... nothing else to do here ...
        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
    }
}

/**
 * @brief This method will be called when billing information retrieval falied.
 *
 * @param a_status HTTP status code.
 * @param a_ev_exception Exception occurred during information fetch.
 */
void ngx::casper::broker::cdn::archive::Module::OnFetchBillingInformationFailed (uint16_t a_status, const ::ev::Exception& a_ev_exception)
{
    // ... set error ...
    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N(ctx_, "BROKER_AN_ERROR_OCCURRED_MESSAGE", a_ev_exception.what());
    // ... finalize request ...
    NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
}

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Synchronization
#endif

/**
 * @brief Call this method to register an archive operation.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::RegisterSyncOperation ()
{
    // ... record 'sync' operation?
    if ( NGX_HTTP_HEAD == ctx_.ngx_ptr_->method || NGX_HTTP_GET == ctx_.ngx_ptr_->method || NGX_HTTP_NOT_MODIFIED == ctx_.response_.status_code_ ) {
        // ... HTTP HEAD, GET or no modifications occurred, no need to register operation ...
        return NGX_OK;
    }
    
    // ... collect all X-CASPER-HEADERS ...
    for ( auto header : ctx_.request_.headers_ ) {
        if ( nullptr != strcasestr(header.first.c_str(), "content-")   ||
            nullptr != strcasestr(header.first.c_str(), "x-casper-")  ||
            nullptr != strcasestr(header.first.c_str(), "user-agent")
            ) {
            db_.sync_data_.headers_[header.first] = header.second;
        }
    }
    
    // ... collect all xattrs ...
    ::cc::fs::file::XAttr xattrs_reader(db_.sync_data_.new_.uri_);
    xattrs_reader.IterateOrdered([this] (const char *const a_name, const char *const a_value) {
        db_.sync_data_.xattrs_[a_name] = a_value;
    });
    
    // ... prepare registry ...
    db_.synchronization_  = new ngx::casper::broker::cdn::archive::db::Synchronization(ctx_.loggable_data_ref_, s_sync_registry_settings_,
                                /* a_callbacks */
                                {
                                    /* success_ */
                                    std::bind(&ngx::casper::broker::cdn::archive::Module::OnRegisterSyncOperationSucceeded, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                                    /* failure_ */
                                    std::bind(&ngx::casper::broker::cdn::archive::Module::OnRegisterSyncOperationFailed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                                }
    );
    
    try {
        // ... from now on the response will be asynchronous ...
        ctx_.response_.asynchronous_ = true;
        // ... register operation at database ...
        switch (ctx_.ngx_ptr_->method) {
            case NGX_HTTP_POST:
                db_.synchronization_->Register(ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Create, db_.sync_data_);
                break;
            case NGX_HTTP_PUT:
                db_.synchronization_->Register(ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Update, db_.sync_data_);
                break;
            case NGX_HTTP_PATCH:
                db_.synchronization_->Register(ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Patch, db_.sync_data_);
                break;
            case NGX_HTTP_DELETE:
                db_.synchronization_->Register(ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Delete, db_.sync_data_);
                break;
            default:
                throw ::cc::Exception("Don't know how to set synchronization record for method '%*.*s'!",
                                      static_cast<int>(ctx_.ngx_ptr_->method_name.len), static_cast<int>(ctx_.ngx_ptr_->method_name.len),
                                      reinterpret_cast<const char* const>(ctx_.ngx_ptr_->method_name.data)
                );
        }
    } catch (const ::cc::Exception& a_cc_exception) {
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_cc_exception);
    } catch (const Json::Exception& a_json_exception) {
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_json_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_bad_alloc);
    } catch (const std::runtime_error& a_rte) {
        // ... set exception ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_rte);
    } catch (const std::exception& a_std_exception) {
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_std_exception);
    } catch (...) {
        auto exception = STD_CPP_GENERIC_EXCEPTION_TRACE();
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR_I18N(ctx_, "BROKER_AN_ERROR_OCCURRED_MESSAGE", exception.c_str());
    }
            
    // ... async request setup failed?
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... response will be synchronous and error should be already set! ...
        ctx_.response_.asynchronous_ = false;
    }

    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief This method will be called when archive operation was successfully registered.
 *
 * @param a_operation Operation, one of \link ngx::casper::broker::cdn::archive::db::Registry::Operation \link.
 * @param a_status    HTTP status code.
 * @param a_json      JSON parsed response.
 */
void ngx::casper::broker::cdn::archive::Module::OnRegisterSyncOperationSucceeded (const ngx::casper::broker::cdn::archive::db::Synchronization::Operation a_operation,
                                                                                  const uint16_t a_status, const Json::Value& a_json)
{
    // ... error?
    if ( not ( 200 == a_status || 204 == a_status ) ) {
        // ... yes, try to rollback action ...
        TryRollback();
        // ... convert operation enum value to string ...
        std::stringstream ss; ss << "" << a_operation << "";
        // ... set error ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                ( "An error occurred while registering archive synchronization operation '" + ss.str() + "': " + std::to_string(a_status) + '!' ).c_str()
        );
        // ... finalize request ...
        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
    } else {
        // ... no, succeeded ...
        Commit();
        // ... set follow-up job payload ...
        Json::Value job = Json::Value::null;
        // ... set job properties  ...
        job                       = Json::Value(Json::ValueType::objectValue);
        job["tube"]               = s_sync_registry_settings_.tube_;
        job["ttr"]                = s_sync_registry_settings_.ttr_;
        job["validity"]           = s_sync_registry_settings_.validity_;
        job["payload"]            = Json::Value(Json::ValueType::objectValue);
        job["payload"]["sync_id"] = a_json.asUInt64();
        // ... submit job
        TrySubmitJob(job);
    }
}

/**
 * @brief This method will be called when archive operation registration failed.
 *
  * @param a_operation   Operation, one of \link ngx::casper::broker::cdn::archive::db::Registry::Operation \link.
 * @param a_status       HTTP status code.
 * @param a_ev_exception Exception occurred during information registration.
 */
void ngx::casper::broker::cdn::archive::Module::OnRegisterSyncOperationFailed (const ngx::casper::broker::cdn::archive::db::Synchronization::Operation a_operation,
                                                                               const uint16_t a_status, const ::ev::Exception& a_ev_exception)
{
    // ... try to rollback action ...
    TryRollback();
    // ... convert operation enum value to string ...
    std::stringstream ss; ss << "" << a_operation << "";
    // ... set error ...
    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
            ( "An error occurred while registering archive synchronization operation '" + ss.str() + "': " + std::to_string(a_status) + " - " + a_ev_exception.what() ).c_str()
    );
    // ... finalize request ...
    NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
}

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Response
#endif

/**
 * @brief Set success response.
 *
 * @param a_info        New file basic information.
 * @param o_other_attrs Other modified attributes ( not xattrs ).
 */
void ngx::casper::broker::cdn::archive::Module::SetSuccessResponse (const ngx::casper::broker::cdn::Archive::RInfo& a_info,
                                                                    const Json::Value* o_other_attrs)
{
    //
    // ... prepare response payload ...
    //
    json_value_               = Json::Value(Json::ValueType::objectValue);
    json_value_["id"  ]       = a_info.new_id_;
    json_value_["type"]       = "cdn-replicator";
    json_value_["attributes"] = Json::Value(Json::ValueType::objectValue);
    
    Json::Value& attributes = json_value_["attributes"];

    // ... update response with data collected after processing request ...
    for ( auto& attr : a_info.attrs_ ) {
        // ... if not serializable ...
        if ( false == attr.serialize_ || 0 == attr.value_.length() ) {
            continue;
        }
        // ... convert ...
        switch(static_cast<Json::ValueType>(attr.tag_)) {
            case Json::ValueType::intValue:
                attributes[attr.key_] = static_cast<Json::Int64>(std::strtoll(attr.value_.c_str(), nullptr, 0));
                break;
            case Json::ValueType::uintValue:
                attributes[attr.key_] = static_cast<Json::UInt64>(std::strtoull(attr.value_.c_str(), nullptr, 0));
                break;
            case Json::ValueType::stringValue:
                attributes[attr.key_] = attr.value_;
                break;
            default:
                // ... not supported ...
                break;
        }
    }
    
    // ... other attributes that are not store in xattr ( e.g. billing stuff ) ...
    if ( nullptr != o_other_attrs ) {
        for ( Json::Value::const_iterator attr = o_other_attrs->begin(); o_other_attrs->end() != attr ; ++attr ) {
            switch(attr->type()) {
                case Json::ValueType::intValue:
                case Json::ValueType::uintValue:
                case Json::ValueType::stringValue:
                    attributes[attr.name()] = (*attr);
                    break;
                default:
                    // ... not supported ...
                    break;
            }
        }
    }
    
    //
    // ... set response ...
    //
    ctx_.response_.headers_["Content-Type"] = sk_tx_content_type_;
    ctx_.response_.body_                    = json_writer_.write(json_value_);
    ctx_.response_.length_                  = ctx_.response_.body_.length();
    
    // ... and we're done!
    ctx_.response_.status_code_ = NGX_HTTP_OK;
    ctx_.response_.return_code_ = NGX_OK;
}

/**
 * @brief Set NOT MODIFIED response.
 */
void ngx::casper::broker::cdn::archive::Module::SetNotModifiedResponse ()
{
    ctx_.response_.status_code_ = NGX_HTTP_NOT_MODIFIED;
    ctx_.response_.return_code_ = NGX_OK;
}

/**
* @brief Set NO CONTENT response.
*/
void ngx::casper::broker::cdn::archive::Module::SetNoContentResponse ()
{
    ctx_.response_.status_code_ = NGX_HTTP_NO_CONTENT;
    ctx_.response_.return_code_ = NGX_OK;
}

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Commit & Rollback
#endif

/**
 * @brief Accept new create file and delete old ( if needed ).
 */
void ngx::casper::broker::cdn::archive::Module::Commit ()
{
    // ... POST ( create or move ), PUT, PATCH and DELETE create temporary files ...
    if ( not ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method || NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ||
               NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) ) {
        // ... if it's a ...
        if ( NGX_HTTP_HEAD == ctx_.ngx_ptr_->method || NGX_HTTP_GET == ctx_.ngx_ptr_->method ) {
            // ... nothing to do  ...
            return;
        } else {
            // ... ??? ...
            throw ::cc::Exception("Insanity checkpoint reached @ %s:%d!", __FILE__, __LINE__);
        }
    }
    
    // ... create or move?
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method && not ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) ) {
        // ... create, nothing to do ...
        return;
    }
    
    // ... PATCH without modifications?
    if ( NGX_HTTP_PATCH == ctx_.ngx_ptr_->method &&  NGX_HTTP_NOT_MODIFIED == ctx_.response_.status_code_ ) {
        // ... nothing to do - no attributes modified no new file ...
        return;
    }
    
    // ... sanity checkpoint ...
    if ( 0 != r_info_.old_uri_.compare(db_.sync_data_.old_.uri_) || 0 != r_info_.new_uri_.compare(db_.sync_data_.new_.uri_) ) {
        throw ::cc::Exception("Insanity checkpoint reached @ %s:%d!", __FILE__, __LINE__);
    }
    
    // ... erase old file ...
    if ( ::cc::fs::File::Exists(r_info_.old_uri_) ) {
        ::cc::fs::File::Erase(r_info_.old_uri_);
    }
}

/**
 * @brief Try to rollback an HTTP request action.
 */
void ngx::casper::broker::cdn::archive::Module::TryRollback ()
{
    try {
        if  ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method ||
             NGX_HTTP_PATCH == ctx_.ngx_ptr_->method  || NGX_HTTP_DELETE == ctx_.ngx_ptr_->method) {
            // NGX_HTTP_POST, NGX_HTTP_PUT and NGX_HTTP_PATCH - deletes new temporary file keeping 'old' or 'current' one ...
            // NGX_HTTP_DELETE                                - delete 'quarantine' copy ...
            if ( 0 != r_info_.new_uri_.length() && true == ::cc::fs::File::Exists(r_info_.new_uri_) ) {
                ::cc::fs::File::Erase(r_info_.new_uri_);
            }
        } // else {} // ... NGX_HTTP_HEAD, NGX_HTTP_GET is nop, no file is generated  ...
    } catch (...) {
        // ... since we're trying to rollback, ignore this error ...
    }
}

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Follow-up job related.
#endif

/**
 * @brief Try to submit a beanstalk job ( any error will be ignored ) and finalize request.
 *
 * @param a_payload Job payload.
 */
void ngx::casper::broker::cdn::archive::Module::TrySubmitJob (const Json::Value& a_payload)
{
    // ... sanity check ...
    if ( false == ctx_.response_.asynchronous_ ) {
        throw ::cc::Exception("%s can't be called in a non asynchronous context!", __FUNCTION__);
    }

    // ... if no tube is set ...
    if ( 0 == s_sync_registry_settings_.tube_.length() || true == a_payload.isNull() ) {
        // ... finalize request ...
        ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(
                                                                    /* a_client */
                                                                    this,
                                                                    /* a_ms */
                                                                    10,
                                                                    /* a_callback */
                                                                    [this] () {
                                                                        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                                                    }
        );
    } else {
        // ... try to submit a job ...
        try {
            (void)job_.Submit(a_payload,
                              /* a_success_callback */
                              [this] () {
                                // ... finalize request ...
                                NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                               },
                              /* a_failure_callback */
                              [this] (const ev::Exception& a_ev_exception) {
                                // ... finalize request ...
                                NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                              }
            );
        } catch (const ::ev::Exception& /* a_ev_exception */) {
            // ... no problem, just finalize request ...
            ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(/* a_client */ this, /* a_ms */ 10,
                                                                        /* a_callback */
                                                                        [this] () {
                                                                            NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
                                                                        }
            );
        }
    }
}

#ifdef __APPLE__
#pragma mark - FACTORY
#endif

/**
 * @brief Content handler factory.
 *
 * @param a_r
 * @param a_at_rewrite_handler
 */
ngx_int_t ngx::casper::broker::cdn::archive::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
{
    //
    // GRAB 'MAIN' CONFIG
    //
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        return NGX_ERROR;
    }

    //
    // GRAB 'MODULE' CONFIG
    //
    ngx_http_casper_broker_cdn_archive_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_cdn_archive_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_archive_module);
    if ( NULL == loc_conf ) {
        return NGX_ERROR;
    }

    //
    // 'WARM UP'
    //
    ngx::casper::broker::Module::Params params = {
        /* in_headers_              */ {},
        /* config_                  */ {},
        /* locale_                  */ "",
        /* supported_content_types_ */ { "*/*" }
    };

    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_cdn_archive_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }

    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::common::Errors configuration_errors(params.locale_);

    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_cdn_archive_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }

    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_archive_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::archive::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::archive::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::archive::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::archive::Module::sk_tx_content_type_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [] (const std::string& a_locale) {
            return new ngx::casper::broker::cdn::common::Errors(a_locale);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ false,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };

    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &broker_conf, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::cdn::archive::Module(config, params, *broker_conf, *loc_conf );
                                                     }
    );
}

#ifdef __APPLE__
#pragma mark - STATIC FUNCTIONS - HANDLERS
#endif

/**
 * @brief Called by nginx a request body is read to be read.
 *
 * @param a_request.
 */
void ngx::casper::broker::cdn::archive::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_cdn_archive_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::archive::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_cdn_archive_module, a_data);
}
