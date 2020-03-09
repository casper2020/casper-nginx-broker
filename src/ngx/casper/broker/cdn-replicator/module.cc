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
#pragma mark - REPLICATOR MODULE
#endif

#include "ngx/casper/broker/cdn-replicator/module.h"

#include "ngx/casper/broker/cdn-replicator/module/version.h"

#include "ngx/casper/broker/cdn-common/errors.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#include "ngx/version.h"

#include <regex>   // std::regex

const char* const ngx::casper::broker::cdn::replicator::Module::sk_rx_content_type_ = "application/octet-stream";
const char* const ngx::casper::broker::cdn::replicator::Module::sk_tx_content_type_ = "application/vnd.api+json;charset=utf-8";

ngx::casper::broker::cdn::replicator::Module::Config ngx::casper::broker::cdn::replicator::Module::s_config_ = {
    /* replicator_dir_prefix_ */ "",
    /* loaded_                */ false
};

#ifdef __APPLE__
#pragma mark - CONSTRUCTOR(S) / DESTRUCTOR
#endif

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_cdn_replicator_loc_conf
 * @param a_ngx_cdn_replicator_loc_conf
 */
ngx::casper::broker::cdn::replicator::Module::Module (const ngx::casper::broker::Module::Config& a_config,
                                                   const ngx::casper::broker::Module::Params& a_params,
                                                   ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_replicator_module_loc_conf_t& a_ngx_cdn_replicator_loc_conf)
: ngx::casper::broker::cdn::common::Module("cdn_replicator", a_config, a_params,
                                           /* a_settings */
                                           {
                                               /* disposition_ */
                                               {
                                                   /* default_value_ */
                                                   std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.response.content_disposition.data), a_ngx_loc_conf.cdn.response.content_disposition.len),
                                                   /* allow_from_url_ */
                                                   false
                                               },
                                               /* implemented_methods_ */
                                               {
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
    archive_(/* a_archivist */ NGX_INFO, /* a_writer */ NGX_CASPER_BROKER_CDN_REPLICATOR_MODULE_INFO, s_ast_config_, ctx_.request_.headers_, s_h2e_map_, s_config_.replicator_dir_prefix_, /* a_replicator */ NGX_CASPER_BROKER_CDN_REPLICATOR_MODULE_INFO),
    x_id_("X-NUMERIC-ID"), x_moves_uri_("X-STRING-ID"), x_replaces_id_("X-CASPER-REPLACES-ID")
{
    // ...
    body_read_supported_methods_ = {
        NGX_HTTP_POST, NGX_HTTP_PUT, NGX_HTTP_PATCH
    };
    // ...
    body_read_bypass_methods_ = {
        NGX_HTTP_DELETE
    };
    // ... one shot config initializer ...
    if ( false == s_config_.loaded_ ) {
        if ( 0 != a_ngx_cdn_replicator_loc_conf.directory_prefix.len ) {
            s_config_.replicator_dir_prefix_ = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_cdn_replicator_loc_conf.directory_prefix.data), a_ngx_cdn_replicator_loc_conf.directory_prefix.len));
        } else {
            s_config_.replicator_dir_prefix_ = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.directories.archive_prefix.data), a_ngx_loc_conf.cdn.directories.archive_prefix.len));
        }
        s_config_.loaded_ = true;
    }
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::replicator::Module::~Module ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark - SETUP & RUN STEPS
#endif

/**
 * @brief 'Pre-run' setup.
 *
 * @return NGX_OK on success, on failure NGX_ERROR.
 */
ngx_int_t ngx::casper::broker::cdn::replicator::Module::Setup ()
{
    const ngx_int_t crv = ngx::casper::broker::cdn::common::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }
    
    // ... grab local configuration ...
    const ngx_http_casper_broker_cdn_replicator_module_loc_conf_t* loc_conf =
        (const ngx_http_casper_broker_cdn_replicator_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_cdn_replicator_module);
    if ( NULL == loc_conf ) {
        // ... set error
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                    "An error occurred while fetching this module configuration - nullptr!"
        );
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    
    // ... read x_id ..
    try {
        // ... read mandatory header(s) ....
        x_replicator_agent_.Set({"X-CASPER-REPLICATOR-AGENT"}, ctx_.request_.headers_, /* a_default */ nullptr, /* a_first_of */ true);
        // ... read mandatory header(s) per request method....
        switch (ctx_.ngx_ptr_->method) {
            case NGX_HTTP_POST:
                // ... read mandatory header(s) ....
                x_id_.Set({"X-CASPER-ENTITY-ID", "X-CASPER-USER-ID"}, ctx_.request_.headers_);
                // ... check if it's a move - not supported!
                x_moves_uri_.Set({"X-CASPER-MOVES-URI"}, ctx_.request_.headers_, &sk_empty_string_);
                if ( true == x_moves_uri_.IsSet() && x_moves_uri_ != sk_empty_string_ ) {
                    NGX_BROKER_MODULE_SET_NOT_ALLOWED(ctx_, "X-CASPER-MOVES-URI is not allowed in replication context!");
                } else {
                    ctx_.response_.return_code_ = NGX_OK;
                }
                break;
            case NGX_HTTP_PUT:
            case NGX_HTTP_PATCH:
            {
                // ... pick replaceable id ...
                try {
                    x_replaces_id_.Set(ctx_.request_.headers_);
                } catch (const ::cc::Exception& a_cc_exception) {
                    throw ngx::casper::broker::cdn::BadRequest(a_cc_exception.what());
                }
                const std::string fake_uri = '/' + (const std::string&)x_replaces_id_;
                // ... ensure file exists and get 'id' header name ...
                archive_.Open(fake_uri, [this](const std::string& a_xhvn) -> ngx::casper::broker::cdn::Archive::Mode {
                    // ... track 'id' header name ...
                    x_id_.Set({a_xhvn}, ctx_.request_.headers_);
                    // ... track 'old' file info ...
                    replication_.old_.id_  = archive_.id();
                    replication_.old_.uri_ = archive_.uri();
                    // ... cancel 'open' ...
                    return ngx::casper::broker::cdn::Archive::Mode::NotSet;
                });
                ctx_.response_.return_code_ = NGX_OK;
            }
                break;
            case NGX_HTTP_DELETE:
                break;
            default:
                NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
                break;
        }
                
     } catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request_exception) {
         NGX_BROKER_MODULE_SET_BAD_REQUEST(ctx_, a_bad_request_exception.what());
     } catch (const ngx::casper::broker::cdn::MethodNotAllowed& a_not_allowed_exception) {
         NGX_BROKER_MODULE_SET_NOT_ALLOWED(ctx_, a_not_allowed_exception.what());
     } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
         NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_, a_not_found.what());
     } catch (const ngx::casper::broker::cdn::Conflict& a_conflict) {
         NGX_BROKER_MODULE_SET_CONFLICT_ERROR(ctx_, a_conflict.what());
     } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
         NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_internal_server_error.what());
     } catch (const ::cc::Exception& a_ev_exception) {
         NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_ev_exception.what());
     }
    
    // ... if an error was set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    
    // ... grab xattrs to replicate ...
    for ( auto attr : ctx_.request_.headers_ ) {
        // ... if it's not an user.com.cldware.archive xattr ...
        if ( 0 != strncasecmp(attr.first.c_str(), "x-user-com-cldware-", sizeof(char) * 19) ) {
            // ... next ...
            continue;
        }
        // .. translate key to . notation ...
        std::string key = attr.first;
        std::replace(key.begin(), key.end(), '-', '.');
        std::replace(key.begin(), key.end(), '_', '-');
        // ... skip 'x-user-' ...
        key = XATTR_ARCHIVE_PREFIX + std::string(key, sizeof(char) * 7, std::string::npos);
        // ... track xattr ...
        replication_.old_.xattrs_[key] = attr.second;
    }

    // ... intercept body?
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method || NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
        // .. create a new file ...
        try {
            // ... new or a replacement?
            std::string reserved_id;
            if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method ) {
                // ... a new file - use id provided in xattrs ...
                const std::string key = XATTR_ARCHIVE_PREFIX "com.cldware.archive.id";
                const auto reserved_id_it = replication_.old_.xattrs_.find(key);
                if ( replication_.old_.xattrs_.end() == reserved_id_it || 0 == reserved_id_it->second.length() ) {
                    throw ngx::casper::broker::cdn::BadRequest(("Missing or invalid header '" + key + "' value!").c_str());
                }
                reserved_id = reserved_id_it->second;
            } else {
                // ... a replacemente file - use id from request URI ...
                ngx::casper::broker::cdn::Archive::IDFromURLPath(ctx_.request_.uri_, reserved_id);
            }
            // ... create file ...
            archive_.Create(x_id_, content_length(), &reserved_id);
            // ... from now on, track file local uri - so it can be deleted if an error occurs ...
            replication_.new_.id_  = archive_.id();
            replication_.new_.uri_ = archive_.uri();
        } catch (const ngx::casper::broker::cdn::Conflict& a_conflict) {
            NGX_BROKER_MODULE_SET_CONFLICT_ERROR(ctx_, a_conflict.what());
        }  catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request_exception) {
            NGX_BROKER_MODULE_SET_BAD_REQUEST(ctx_, a_bad_request_exception.what());
        } catch (const ngx::casper::broker::cdn::MethodNotAllowed& a_not_allowed_exception) {
            NGX_BROKER_MODULE_SET_NOT_ALLOWED(ctx_, a_not_allowed_exception.what());
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
        // ... if still 'good to go' ...
        if ( NGX_OK == ctx_.response_.return_code_ ) {
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
 * @brief Process the previously defined HTTP request.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::replicator::Module::Run ()
{
    // ... if an error occurred...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    // ... start as bad request ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;
    // ... process request ...
    try {
        
        switch (ctx_.ngx_ptr_->method) {
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
        
    } catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request_exception) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST(ctx_, a_bad_request_exception.what());
    } catch (const ngx::casper::broker::cdn::MethodNotAllowed& a_not_allowed_exception) {
        NGX_BROKER_MODULE_SET_NOT_ALLOWED(ctx_, a_not_allowed_exception.what());
    } catch (const ngx::casper::broker::cdn::NotFound& a_not_found) {
        NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_, a_not_found.what());
    } catch (const ngx::casper::broker::cdn::Conflict& a_conflict) {
        NGX_BROKER_MODULE_SET_CONFLICT_ERROR(ctx_, a_conflict.what());
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_internal_server_error.what());
    } catch (const ::cc::Exception& a_ev_exception) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_ev_exception.what());
    } catch (...) {
        try {
            CC_EXCEPTION_RETHROW(/* a_unhandled */ true);
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    // ... since there is no async operations ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... something went wrong, try to rollback action ...
        TryRollback();
    } else {
        // ... commit and we're done ...
        Commit();
    }
    // ... we're done
    return ctx_.response_.return_code_;
}

#ifdef __APPLE__
#pragma mark - HTTP METHODS
#endif

/**
 * @brief Process an HTTP POST request.
 */
void ngx::casper::broker::cdn::replicator::Module::POST ()
{
    //
    // ... a new 'unique' file is open ...
    //
    
    // ... copy 'old' xattrs, because we need to patch them ...
    for ( auto attr : replication_.old_.xattrs_ ) {
        replication_.new_.xattrs_[attr.first] = attr.second;
    }
  
    // ... set replication info attribute ...
    SetReplicationInfoAttr("Create");
        
    // ... close it and write extended attributes ...
    archive_.Close(replication_.new_.xattrs_, sk_preserved_upload_attrs_, r_info_);
    
    // ... validate file and set final response ...
    ValidateAndSetResponse(r_info_);
}

/**
 * @brief Process an HTTP PUT request.
 */
void ngx::casper::broker::cdn::replicator::Module::PUT ()
{
    //
    // ... a new 'unique' file is open ...
    //
    
    // ... copy 'old' xattrs, because we need to patch them ...
    for ( auto attr : replication_.old_.xattrs_ ) {
        replication_.new_.xattrs_[attr.first] = attr.second;
    }

    // ... set replication info ...
    SetReplicationInfoAttr("Update");
    
    // ... close it and write extended attributes ...
    archive_.Close(replication_.new_.xattrs_, sk_preserved_upload_attrs_, r_info_);
    
    // ... validate file and set final response ...
    ValidateAndSetResponse(r_info_);
    
    // ... 'new' archive accepted ...
    r_info_.old_uri_ = replication_.old_.uri_;
    r_info_.old_id_  = replication_.old_.id_;
}

/**
 * @brief Process an HTTP PATCH request.
 */
void ngx::casper::broker::cdn::replicator::Module::PATCH ()
{
    //
    // ... a new 'unique' file is open ...
    //
    
    // ... copy 'old' xattrs, because we need to patch them ...
    for ( auto attr : replication_.old_.xattrs_ ) {
        replication_.new_.xattrs_[attr.first] = attr.second;
    }

    // ... set replication info ...
    SetReplicationInfoAttr("Patch");
    
    // ... close it and write extended attributes ...
    archive_.Close(replication_.new_.xattrs_, sk_preserved_upload_attrs_, r_info_);
    
    // ... validate file and set final response ...
    ValidateAndSetResponse(r_info_);
    
    // ... 'new' archive accepted ...
    r_info_.old_uri_ = replication_.old_.uri_;
    r_info_.old_id_  = replication_.old_.id_;
}

/**
 * @brief Process an HTTP DELETE request.
 */
void ngx::casper::broker::cdn::replicator::Module::DELETE ()
{
     // ... delete permanently ...
    archive_.Delete(ctx_.request_.uri_, /* a_quarantine */ nullptr, r_info_);
    
    // ... and we're done!
    ctx_.response_.status_code_ = NGX_HTTP_NO_CONTENT;
    ctx_.response_.return_code_ = NGX_OK;
}

/**
 * @brief Set replication information.
 *
 * @param a_operation Create, Update, Patch or Delete.
 */
void ngx::casper::broker::cdn::replicator::Module::SetReplicationInfoAttr (const char* const a_operation)
{
    const std::string& agent = (const std::string&)x_replicator_agent_;

    Json::Value data;
    
    const auto it = replication_.new_.xattrs_.find(XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.data");
    if ( replication_.new_.xattrs_.end() != it && it->second.length() > 0 ) {
        // ... only when we're replicating a previously replicated file ...
        if ( false == json_reader_.parse(it->second, data) ) {
            data = Json::Value(Json::ValueType::objectValue);
        }
    } else {
        data = Json::Value(Json::ValueType::objectValue);
    }
    
    // ...new entry?
    if ( false == data.isMember("history") ) {
        data["history"] = Json::Value(Json::ValueType::arrayValue);
    }
    Json::Value& entry = data["history"].append(Json::Value(Json::ValueType::objectValue));
    // { "history": [ { "operation": "", at: "", "by": "" }] }
    entry["operation"] = a_operation;
    entry["by"]        = NGX_CASPER_BROKER_CDN_REPLICATOR_MODULE_INFO;
    entry["at"]        = cc::UTCTime::NowISO8601WithTZ();
    entry["requestee"] = agent;

    replication_.new_.xattrs_[XATTR_ARCHIVE_PREFIX "com.cldware.archive.replication.data"] = json_writer_.write(data);
}

/**
 * @brief Validate new written file, on success set final response.
 *
 * @param a_info New file basic information.
 */
void ngx::casper::broker::cdn::replicator::Module::ValidateAndSetResponse (const ngx::casper::broker::cdn::Archive::RInfo& a_info)
{
    // ... now validate file integrity ...
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method ) {
        // ... id should be aready set correctly ...
        archive_.Validate(ctx_.request_.uri_ + "/" + replication_.new_.id_,
                          &replication_.old_.xattrs_[XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5"],
                          /* a_id */ nullptr,
                          &replication_.old_.xattrs_
        );
    } else {
        // ... since we are writing to a temporary file, id must be compared agains it's xattr ...
        archive_.Validate(ctx_.request_.uri_ + "/" + replication_.new_.id_,
                          &replication_.old_.xattrs_[XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5"],
                          &replication_.new_.xattrs_[XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"],
                          &replication_.old_.xattrs_
        );
    }
    
    // ... prepare response ...
    json_value_               = Json::Value(Json::ValueType::objectValue);
    json_value_["id"  ]       = replication_.new_.xattrs_[XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"];
    json_value_["type"]       = "cdn-replicator";
    json_value_["attributes"] = Json::Value(Json::ValueType::objectValue);

    // ... update response with data collected after processing request ...
    for ( auto& attr : a_info.attrs_ ) {
        if ( false == attr.serialize_ || 0 == attr.value_.length() ) {
            continue;
        }
        if ( Json::ValueType::uintValue == attr.tag_ ) {
            json_value_["attributes"][attr.key_] = static_cast<Json::UInt64>(std::strtoull(attr.value_.c_str(), nullptr, 0));
        } else {
            json_value_["attributes"][attr.key_] = attr.value_;
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

#ifdef __APPLE__
#pragma mark - HELPER METHODS - Commit & Rollback
#endif

/**
 * @brief Accept new create file and delete old ( if needed ).
 */
void ngx::casper::broker::cdn::replicator::Module::Commit ()
{
    // ... POST ( create ), PUT, PATCH creates a temporary file ...
    if ( not ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method || NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ||
              NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) ) {
        // ... ??? ...
        throw ::cc::Exception("Insanity checkpoint reached @ %s:%d!", __FILE__, __LINE__);
    }
    
    // ... create?
    if ( NGX_HTTP_POST == ctx_.ngx_ptr_->method ) {
        // ... nothing to do ...
        return;
    }
    
    // ... update or delete?
    if ( NGX_HTTP_PUT == ctx_.ngx_ptr_->method ||  NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
        // ... sanity checkpoint ...
        if ( 0 != r_info_.old_uri_.compare(replication_.old_.uri_) || 0 != r_info_.new_uri_.compare(replication_.new_.uri_) ) {
            throw ::cc::Exception("Insanity checkpoint reached @ %s:%d!", __FILE__, __LINE__);
        }
        // ... erase old file and keep new ...
        if ( ::cc::fs::File::Exists(r_info_.old_uri_) ) {
            ::cc::fs::File::Erase(r_info_.old_uri_);
        }
    } /* else if ( NGX_HTTP_DELETE == ctx_.ngx_ptr_->method ) {} */
     // ... nothing to do here - 'old' file already deleted ...
}

/**
 * @brief Try to rollback an HTTP request action.
 */
void ngx::casper::broker::cdn::replicator::Module::TryRollback ()
{
    try {
        if  ( NGX_HTTP_POST == ctx_.ngx_ptr_->method || NGX_HTTP_PUT == ctx_.ngx_ptr_->method ||
              NGX_HTTP_PATCH == ctx_.ngx_ptr_->method ) {
            // NGX_HTTP_POST, NGX_HTTP_PUT and NGX_HTTP_PATCH - deletes new temporary file keeping 'old' or 'current' one ...
            // NGX_HTTP_DELETE                                - delete 'quarantine' copy ...
            if ( 0 != r_info_.new_uri_.length() && true == ::cc::fs::File::Exists(r_info_.new_uri_) ) {
                ::cc::fs::File::Erase(r_info_.new_uri_);
            }
        } // else {} // ... NGX_HTTP_DELETE is nop, no file is generated  ...
    } catch (...) {
        // ... since we're trying to rollback, ignore this error ...
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
ngx_int_t ngx::casper::broker::cdn::replicator::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_cdn_replicator_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_cdn_replicator_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_replicator_module);
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

    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_cdn_replicator_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }

    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::common::Errors configuration_errors(params.locale_);

    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_cdn_replicator_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }

    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_replicator_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::replicator::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::replicator::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::replicator::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::replicator::Module::sk_tx_content_type_,
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
                                                         return new ::ngx::casper::broker::cdn::replicator::Module(config, params, *broker_conf, *loc_conf );
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
void ngx::casper::broker::cdn::replicator::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_cdn_replicator_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::replicator::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_cdn_replicator_module, a_data);
}
