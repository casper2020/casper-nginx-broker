/**
 * @file module.cc
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

#include "ngx/casper/broker/cdn-common/module.h"

#include "ngx/casper/broker/cdn-common/archive.h"

#include "ngx/ngx_utils.h"

ngx::casper::broker::cdn::H2EMap ngx::casper::broker::cdn::common::Module::s_h2e_map_        = {};
bool                             ngx::casper::broker::cdn::common::Module::s_h2e_map_set_    = false;

Json::Value                      ngx::casper::broker::cdn::common::Module::s_ast_config_     = Json::Value(Json::ValueType::nullValue);
bool                             ngx::casper::broker::cdn::common::Module::s_ast_config_set_ = false;

const std::string ngx::casper::broker::cdn::common::Module::sk_empty_string_ = "";

const std::set<std::string> ngx::casper::broker::cdn::common::Module::sk_preserved_upload_attrs_ = {
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.protocol",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.method",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.user-agent",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.origin",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.referer",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.ip",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.created.by",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.created.at",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.uri",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.content-type",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.content-length",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.md5",
    XATTR_ARCHIVE_PREFIX "com.cldware.upload.seal"
};

/**
 * @brief Default constructor.
 *
 * @param a_name
 * @param a_config
 * @param a_params
 * @param a_settings
 */
ngx::casper::broker::cdn::common::Module::Module (const char* const a_name,
                                                  const ngx::casper::broker::cdn::common::Module::Config& a_config, const ngx::casper::broker::cdn::common::Module::Params& a_params,
                                                  const ngx::casper::broker::cdn::common::Module::Settings& a_settings)
: ngx::casper::broker::Module(a_name, a_config, a_params),
  content_type_("text/plain"),
  content_disposition_("Content-Disposition", a_settings.disposition_.default_value_),
  settings_(a_settings),
  r_info_({
      /* old_id     */ "",
      /* old_uri_   */ "",
      /* new_id     */ "",
      /* bew_uri_   */ "",
      /* attrs_     */ {
          /*  name_                                                  , key_            , tag_                        , optional_ , value_*/
          { XATTR_ARCHIVE_PREFIX "com.cldware.archive.id"            , "id"            , Json::ValueType::stringValue, false     ,     "", /* serialize_ */ false },
          { XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-type"  , "content-type"  , Json::ValueType::stringValue, false     ,     "", /* serialize_ */ true  },
          { XATTR_ARCHIVE_PREFIX "com.cldware.archive.content-length", "content-length", Json::ValueType::uintValue  , false     ,     "", /* serialize_ */ true  },
          { XATTR_ARCHIVE_PREFIX "com.cldware.archive.md5"           , "md5"           , Json::ValueType::stringValue, false     ,     "", /* serialize_ */ true  }
      }
  })
{
    // ... set JSON writer properties ...
    json_writer_.omitEndingLineFeed();
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::Module::~Module ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief 'Pre-run' setup.
 *
 * @return NGX_OK on success, on failure NGX_ERROR.
 */
ngx_int_t ngx::casper::broker::cdn::common::Module::Setup ()
{    
    const ngx_int_t crv = ngx::casper::broker::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }
    
    // ... content 'content disposition' header ...
    if ( NGX_HTTP_HEAD == ctx_.ngx_ptr_->method || NGX_HTTP_GET == ctx_.ngx_ptr_->method ) {
        try {
            content_disposition_.Set(ctx_.request_.headers_);
        } catch (const ::cc::Exception& a_cc_exception) {
            if ( false == settings_.disposition_.allow_from_url_ ) {
                NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, a_cc_exception.what());
            }
        }
    }

    // ... other required 'content' headers ...
    if ( settings_.required_content_type_methods_.end() != settings_.required_content_type_methods_.find(ctx_.ngx_ptr_->method) ) {
        try {
            if ( NGX_OK == ctx_.response_.return_code_ ) {
                content_type_.Set(ctx_.request_.headers_);
            }
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, a_cc_exception.what());
        }
    }
    
    // ... other required 'content' headers ...
    if ( settings_.required_content_length_methods_.end() != settings_.required_content_length_methods_.find(ctx_.ngx_ptr_->method) ) {
        try {
            if ( NGX_OK == ctx_.response_.return_code_  ) {
                content_length_.Set(ctx_.request_.headers_);
            }
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, a_cc_exception.what());
        }
    }

    
    // ... if an error is already set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        return ctx_.response_.return_code_;
    }
    
    // ... grab 'MAIN' config ...
    ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_module);
    if ( NULL == broker_conf ) {
        // ... set error
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                    "An error occurred while fetching main module configuration - nullptr!"
        );
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    

    // ... method not implemented?
    if ( settings_.implemented_methods_.end() == settings_.implemented_methods_.find(ctx_.ngx_ptr_->method) ) {
        return NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    }
    
    // ... test redirect config ...
    if ( 0 == settings_.redirect_.protocol_->len ) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, "Missing or invalid directive 'nginx_casper_broker_cdn_redirect_protocol' value!");
    } else if ( 0 == settings_.redirect_.host_->len ) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, "Missing or invalid directive 'nginx_casper_broker_cdn_redirect_host' value!");
    }
    
    // ... if an error is already set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        return ctx_.response_.return_code_;
    }
    
    // ... copy redirect info ...
    ctx_.response_.redirect_.protocol_ = std::string(reinterpret_cast<const char*>(settings_.redirect_.protocol_->data), settings_.redirect_.protocol_->len);
    ctx_.response_.redirect_.host_     = std::string(reinterpret_cast<const char*>(settings_.redirect_.host_->data)    , settings_.redirect_.host_->len    );
    ctx_.response_.redirect_.port_     = ( 0 != (*settings_.redirect_.port_) ) ? (*settings_.redirect_.port_) : 80;
    if ( 0 != settings_.redirect_.path_->len ) {
        ctx_.response_.redirect_.path_ = std::string(reinterpret_cast<const char*>(settings_.redirect_.path_->data), settings_.redirect_.path_->len) + '/';
    }
    if ( 0 != settings_.redirect_.location_->len ) {
        ctx_.response_.redirect_.location_ = std::string(reinterpret_cast<const char*>(settings_.redirect_.location_->data), settings_.redirect_.location_->len);
    }
    
    // content-dispostion from URL?
    if ( false == content_disposition_.IsSet() ) {
        // ... check url path  ...
        ngx_str_t args = ngx_null_string;
        switch(ctx_.ngx_ptr_->method) {
            case NGX_HTTP_HEAD:
            case NGX_HTTP_GET:
            {
                const char* uri = ctx_.request_.uri_.c_str();
                const char* pch = strrchr(ctx_.request_.uri_.c_str(), '/');
                if ( nullptr == pch || ( ctx_.request_.uri_.length() - ( pch - uri ) ) < 10 ) { // aBBcccccc
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
                }
                args.data = ctx_.ngx_ptr_->args.data;
                args.len  = ctx_.ngx_ptr_->args.len;
                break;
            }
            default:
                break;
        }
        // ... try from URI params?
        if ( NULL != args.data ) {
            std::map<std::string, std::string> arguments;
            // ... any param to process?
            if ( NGX_OK == ngx::utls::nrs_ngx_get_args(ctx_.ngx_ptr_, &args, arguments) ) {
                std::string tmp_str;
                for ( auto it : arguments ) {
                    if ( NGX_OK == ngx::utls::nrs_ngx_unescape_uri(ctx_.ngx_ptr_, it.second.c_str(), tmp_str) ) {
                        it.second = tmp_str;
                    }
                }
                content_disposition_ = arguments;
            }
        }
    }
    
    // ... validate Content-Disposition ...
    try {
        content_disposition_.Validate();
    } catch (const cc::Exception& a_cc_exception) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, a_cc_exception.what());
    }
    
    // ... if an error is already set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        return ctx_.response_.return_code_;
    }
    
    //
    // Load 'one-shot' config.
    //
    
    // ... h2e config ...
    if ( false == s_h2e_map_set_ ) {        
        try {
            ngx::casper::broker::cdn::Archive::LoadH2EMap(reinterpret_cast<const char*>(broker_conf->cdn.h2e_map.data),
                                                          static_cast<size_t>(broker_conf->cdn.h2e_map.len),
                                                          s_h2e_map_
            );
            s_h2e_map_set_ = true;
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    
    // ... ast config ...
    if ( false == s_ast_config_set_ ) {
        try {
            ngx::casper::broker::cdn::Archive::LoadACTConfig(reinterpret_cast<const char*>(broker_conf->cdn.ast.data),
                                                             static_cast<size_t>(broker_conf->cdn.ast.len),
                                                             s_ast_config_
            );
            s_ast_config_set_ = true;
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    
    // ... done ...
    return ctx_.response_.return_code_;
}
