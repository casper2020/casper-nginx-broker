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
#pragma mark - PUBLIC MODULE
#endif

#include "ngx/casper/broker/cdn-public/module.h"

#include "ngx/casper/broker/cdn-public/module/version.h"

#include "ngx/casper/broker/cdn-common/errors.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#include "ngx/casper/broker/tracker.h"

#include "ngx/version.h"

#include "cc/auth/jwt.h"
#include "cc/auth/exception.h"

const char* const ngx::casper::broker::cdn::pub::Module::sk_rx_content_type_ = "application/octet-stream";
const char* const ngx::casper::broker::cdn::pub::Module::sk_tx_content_type_ = "application/vnd.api+json;charset=utf-8";

ngx::casper::broker::cdn::Archive::Settings ngx::casper::broker::cdn::pub::Module::s_public_archive_settings_ = {
    /* directories_ */ {
        /* temporary_prefix_ */ "",
        /* archive_prefix_ */ ""
    },
    /* quarantine_ */ {
      /* validity_ */ 0,
      /* directory_prefix_ */ ""
    }
};

ngx::casper::broker::cdn::pub::Module::ASTConfig ngx::casper::broker::cdn::pub::Module::s_public_ast_config_ = {
    /* value_ */ Json::Value(Json::ValueType::nullValue),
    /* se_t   */ false
};

ngx::casper::broker::cdn::pub::Module::JWTConfig ngx::casper::broker::cdn::pub::Module::s_jwt_config_ = {
    /* allowed_iss_        */ Json::Value(Json::ValueType::nullValue),
    /* rsa_public_key_uri_ */ "",
    /* set_                */ false
};

#ifdef __APPLE__
#pragma mark - CONSTRUCTOR(S) / DESTRUCTOR
#endif

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_cdn_public_loc_conf
 * @param a_ngx_cdn_public_loc_conf
 */
ngx::casper::broker::cdn::pub::Module::Module (const ngx::casper::broker::Module::Config& a_config,
                                               const ngx::casper::broker::Module::Params& a_params,
                                               ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_public_module_loc_conf_t& a_ngx_cdn_public_loc_conf)
: ngx::casper::broker::cdn::common::Module("cdn_public", a_config, a_params,
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
                                               },
                                               /* required_content_type_methods_ */
                                               {
                                               },
                                               /* required_content_length_methods_ */
                                               {
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
archive_(/* a_archivist */ NGX_INFO, /* a_writer */ NGX_CASPER_BROKER_CDN_PUBLIC_MODULE_INFO, s_public_ast_config_.value_, fake_headers_, s_h2e_map_, s_public_archive_settings_.directories_.archive_prefix_),
x_content_type_(/* a_default */ sk_rx_content_type_),
x_content_disposition_(
   /* a_default */ std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.response.content_disposition.data), a_ngx_loc_conf.cdn.response.content_disposition.len)
)
{
    // ...
    body_read_bypass_methods_ = {
        NGX_HTTP_HEAD, NGX_HTTP_GET
    };
    // ... one shot directories config  ...
    if ( 0 == s_public_archive_settings_.directories_.archive_prefix_.length() ) {
        s_public_archive_settings_.directories_.archive_prefix_ = OSAL_NORMALIZE_PATH(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.cdn.directories.archive_prefix.data), a_ngx_loc_conf.cdn.directories.archive_prefix.len));
    }
    // ... one-shot alternative ast config ...
    if ( false == s_public_ast_config_.set_ ) {        
        // ... grab 'MAIN' config ...
        ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_module);
        if ( NULL == broker_conf ) {
            // ... set error ...
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                        "An error occurred while fetching main module configuration - nullptr!"
            );
        }
        try {
            ngx::casper::broker::cdn::Archive::LoadACTConfig(reinterpret_cast<const char*>(broker_conf->cdn.ast.data),
                                                             static_cast<size_t>(broker_conf->cdn.ast.len),
                                                             s_public_ast_config_.value_
            );
            s_public_ast_config_.set_ = true;
        } catch (const ::cc::Exception& a_cc_exception) {
            // ... set error ...
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    // ... grab JWT config ...
    if ( false == s_jwt_config_.set_ ) {
        s_jwt_config_.rsa_public_key_uri_ = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_rsa_public_key_uri.data), a_ngx_loc_conf.jwt_rsa_public_key_uri.len);
        try {
            if ( false == json_reader_.parse(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_iss.data), a_ngx_loc_conf.jwt_iss.len), s_jwt_config_.allowed_iss_) ) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Unable to parse 'jwt_iss' directive data!");
            } else if ( true == s_jwt_config_.allowed_iss_.isNull() || Json::ValueType::arrayValue != s_jwt_config_.allowed_iss_.type() || 0 == s_jwt_config_.allowed_iss_.size() ) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Invalid 'jwt_iss' directive data format!");
            }
            s_jwt_config_.set_ = true;
        } catch (const Json::Exception& a_json_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_json_exception);
        }
    }
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::pub::Module::~Module ()
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
ngx_int_t ngx::casper::broker::cdn::pub::Module::Setup ()
{
    // ... if an error is already set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    
    // ... common setup ...
    const ngx_int_t crv = ngx::casper::broker::cdn::common::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }
    
    // ... start as bad request ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;
        
    // ... validate request URI ...
    if ( ctx_.request_.uri_.length() <= ctx_.request_.location_.length() ) {
        // ... rejected ...
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
        // ... done ...
        return ctx_.response_.return_code_;
    }
    const char* ptr = ctx_.request_.uri_.c_str() + ctx_.request_.location_.length();
    while ( '/' == ptr[0] && '\0' != ptr[0] ) {
        ptr++;
    }
    const char* end = ptr;
    while ( '?' != end[0] && '\0' != end[0] ) {
        end++;
    }
    if ( nullptr == end ) {
        // ... rejected ...
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
        // ... done ...
        return ctx_.response_.return_code_;
    }

    const std::string b64 = ::cc::auth::JWT::MakeBrowsersUnhappy(std::string(ptr, end - ptr));
 
    // ... verify JWT signature ...
    const int64_t now = cc::UTCTime::Now();

    try {
        
        for ( Json::ArrayIndex idx = 0; idx < s_jwt_config_.allowed_iss_.size() ; ++idx ) {
            
            // .. try to decode JWT w/ current 'ISS'...
            ::cc::auth::JWT jwt(s_jwt_config_.allowed_iss_[idx].asCString());
            
            try {
                
                jwt.Decode(b64, s_jwt_config_.rsa_public_key_uri_);
                                
                const Json::Value exp = jwt.GetRegisteredClaim(::cc::auth::JWT::RegisteredClaim::EXP);
                if ( true == exp.isNull() ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "JWT %s claim is required!", "exp");
                }
                if ( exp.asInt64() <= static_cast<Json::Int64>(now) ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "JWT is %s!", "expired");
                }
                
                const Json::Value nbf = jwt.GetRegisteredClaim(::cc::auth::JWT::RegisteredClaim::NBF);
                if ( false == nbf.isNull() && nbf.asInt64() > static_cast<Json::Int64>(now) ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "JWT is %s!", "ahead of time");
                }
                                
                //
                // expected payload object 'archive'
                // ...
                // "archive": {
                //     "id:" <string>,
                //     "entity_id": <numeric>,
                //     "user_id": <numeric>
                // }
                // ....
                //
                
                const Json::Value archive = jwt.GetUnregisteredClaim("archive");
                if ( true == archive.isNull() || false == archive.isObject() ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "JWT unregistered claim '%s': not set or invalid!", "archive");
                }

                // ... fake headers so we can keep the archive module behaviour ...
                for ( auto& member : { std::string("entity_id"), std::string("user_id") } ) {
                    if ( true == archive.isMember(member) && true == archive[member].isConvertibleTo(Json::ValueType::stringValue) ) {
                        std::string key = member; std::replace(key.begin(), key.end(), '_', '-');
                        fake_headers_["x-casper-" + key] = archive[member].asString();
                    }
                }
                fake_headers_["X-CASPER-MODULE-MASK"] = "0x20000000";
                
                const Json::Value& id = archive.get("id", Json::Value::null);
                if ( true == id.isNull() || false == id.isString()|| 0 == id.asString().length() ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "Invalid or missing '%s': field!", "id");
                }
                
                // ... simulate uri ...
                uri_ = '/' + id.asString();
                
                // ... x_id_ is set ...
                ctx_.response_.return_code_ = NGX_OK;
                
                // ... we're done here  ...
                break;
                
            }  catch (const ::cc::auth::Exception& a_cc_auth_exception) {
                // ... track exception ...
                NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_cc_auth_exception);
            }
            
            // ... 'ISS' not accepted, try next one ...
        }
        
    } catch (const Json::Exception& a_json_exception) {
        // ... track exception ...
        NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_json_exception);
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {
        // ... track exception ...
        NGX_BROKER_MODULE_SET_BAD_REQUEST_EXCEPTION(ctx_, a_broker_exception);
    }
    // ... done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Ensure requirements are respected before executing requrest..
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::cdn::pub::Module::Run ()
{
    // ... if an error is already set ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    
    // ... start as bad request ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;

    //
    // URN: /<jwt>
    //
    //   <jwt>: defined at \link Setup \link
    //
    // REQUEST:
    //
    //   HEADERS:
    //
    //     required:
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
    
   // ... GET is the only supported method ..

    try {
        switch (ctx_.ngx_ptr_->method) {
            case NGX_HTTP_HEAD:
                HEAD();
                break;
            case NGX_HTTP_GET:
                GET();
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
    
    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Process an HTTP GET request.
 */
void ngx::casper::broker::cdn::pub::Module::HEAD ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    //   <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS: NONE
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
        archive_.Open(uri_);

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
void ngx::casper::broker::cdn::pub::Module::GET ()
{
    //
    // URN: /<location>/<id>[.<ext>]
    //
    //   <id>: archive id, format aBBcccccc
    //
    // REQUEST:
    //
    //   HEADERS: NONE
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
            x_filename_.Set(ctx_.request_.headers_, /* a_default */ &content_disposition().filename());
        } else {
            x_filename_.Set(ctx_.request_.headers_, /* a_default */ &x_content_disposition_.filename());
        }

        // ... this headers can default to their xattr value ( if set ) ...
        //
        // ENSURE required headers ( archive_.Open will take care of that )
        //
        // ... prepare reader ...
        archive_.Open(uri_);

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
        ctx_.response_.headers_["X-CASPER-CONTENT-TYPE"]        = (const std::string&)x_content_type_;
        ctx_.response_.headers_["X-CASPER-CONTENT-DISPOSITION"] = x_content_disposition_.value();
        ctx_.response_.headers_["X-CASPER-FILE-LOCAL-TRY"]      =
        ( r_info_.old_uri_.c_str() + s_public_archive_settings_.directories_.archive_prefix_.length() );
        
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

#ifdef __APPLE__
#pragma mark - FACTORY
#endif

/**
 * @brief Content handler factory.
 *
 * @param a_r
 * @param a_at_rewrite_handler
 */
ngx_int_t ngx::casper::broker::cdn::pub::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_cdn_public_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_cdn_public_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_public_module);
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

    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_cdn_public_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }

    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::common::Errors configuration_errors(params.locale_);

    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_cdn_public_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }

    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_public_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::pub::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::pub::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::pub::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::pub::Module::sk_tx_content_type_,
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
                                                         return new ::ngx::casper::broker::cdn::pub::Module(config, params, *broker_conf, *loc_conf );
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
void ngx::casper::broker::cdn::pub::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_cdn_public_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::pub::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_cdn_public_module, a_data);
}
