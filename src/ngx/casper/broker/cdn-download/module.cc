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

#include "ngx/casper/broker/cdn-download/module.h"

#include "ngx/casper/broker/cdn/errors.h"

#include <algorithm>

#include "ngx/ngx_utils.h"

#include <regex>      // std::regex

const char* const ngx::casper::broker::cdn::dl::Module::sk_rx_content_type_ = "application/json";
const char* const ngx::casper::broker::cdn::dl::Module::sk_tx_content_type_ = "application/json;charset=utf-8";

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_log_conf
 * @param a_ngx_cdn_loc_conf
 */
ngx::casper::broker::cdn::dl::Module::Module (const ngx::casper::broker::cdn::dl::Module::Config& a_config, const ngx::casper::broker::cdn::dl::Module::Params& a_params,
                                              ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_download_module_loc_conf_t& /* a_ngx_cdn_loc_conf */)
    : ngx::casper::broker::cdn::common::Module("cdn-dl", a_config, a_params,
                                               {
                                                   /* disposition_ */
                                                   {
                                                       /* default_value_ */
                                                       "attachment",
                                                       /* allow_from_url_ */
                                                       ( NGX_HTTP_HEAD == a_config.ngx_ptr_->method || NGX_HTTP_GET == a_config.ngx_ptr_->method )
                                                   },
                                                   /* implemented_methods_ */
                                                   {
                                                       NGX_HTTP_HEAD, NGX_HTTP_GET
                                                   },
                                                   /* required_content_types_methods_ */
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
      session_(ctx_, this, a_params)
{
     body_read_bypass_methods_ = {
         NGX_HTTP_HEAD, NGX_HTTP_GET
     };
     ctx_.response_.redirect_.supported_methods_.insert(NGX_HTTP_HEAD);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::dl::Module::~Module ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return
 */
ngx_int_t ngx::casper::broker::cdn::dl::Module::Run ()
{
    return session_.Fetch(std::bind(&ngx::casper::broker::cdn::dl::Module::OnSessionFetchSucceeded, this, std::placeholders::_1, std::placeholders::_2));
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Called when a session data is validated and ready to use.
 *
 * @param a_async_request
 * @param a_session
 */
void ngx::casper::broker::cdn::dl::Module::OnSessionFetchSucceeded (const bool /* a_async_request */, const ::ev::casper::Session& a_session)
{
    const char* const tmp = ctx_.request_.uri_.c_str();
    
    const char* pch = strrchr(tmp, '/');
    if ( nullptr == pch ) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, "Invalid URI!");
        NGX_BROKER_MODULE_RESET_REDIRECT(ctx_);
        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
        return;
    }
    
    std::string uri;
    
    const char* args_separator = strrchr(pch, '?');
    if ( nullptr != args_separator ) {
        uri = std::string(pch, static_cast<size_t>(args_separator - pch));
    } else {
        uri = pch;
    }
    
    std::map<std::string, std::string> session_headers;

    ctx_.response_.redirect_.file_ = std::string(uri, 1);

    const std::regex id_expr("([a-z]{1})([a-zA-Z0-9]{2})([a-zA-Z0-9]{6})(\\.[a-z]*)?", std::regex_constants::ECMAScript);
    std::smatch match;
    if ( false == std::regex_search(ctx_.response_.redirect_.file_, match, id_expr) || match.size() < 4 ) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, ( "Unable to setup redirect file with invalid id '" + ctx_.response_.redirect_.file_ + "'!").c_str() );
        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
    } else {
        a_session.GetHeaders({ "entity_id", "user_id", "role_mask", "module_mask" }, session_headers);
        for ( auto it : session_headers ) {
            ctx_.response_.headers_[it.first] = it.second;
        }
        
        ctx_.response_.headers_["X-CASPER-REDIRECT-PROTOCOL"]   = ctx_.response_.redirect_.protocol_;
        ctx_.response_.headers_["X-CASPER-REDIRECT-HOST"]       = ctx_.response_.redirect_.host_;
        ctx_.response_.headers_["X-CASPER-REDIRECT-PORT"]       = std::to_string(ctx_.response_.redirect_.port_);
        ctx_.response_.headers_["X-CASPER-CONTENT-DISPOSITION"] = (const std::string&)content_disposition();
        
        ctx_.response_.status_code_ = NGX_HTTP_OK;
        ctx_.response_.return_code_ = NGX_OK;
        
        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(this);
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Content handler factory.
 *
 * @param a_r
 * @param a_at_rewrite_handler
 */
ngx_int_t ngx::casper::broker::cdn::dl::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_cdn_download_module_loc_conf_t* loc_conf =
        (ngx_http_casper_broker_cdn_download_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_download_module);
    if ( NULL == loc_conf ) {
        return NGX_ERROR;
    }
    
    //
    // 'WARM UP'
    //
    ngx::casper::broker::cdn::dl::Module::Params params = {
        /* in_headers_            */ {},
        /* config_                */ {},
        /* locale_                */ ""
    };
    
    ngx_int_t rv = ngx::casper::broker::cdn::dl::Module::WarmUp(ngx_http_casper_broker_cdn_download_module, a_r, loc_conf->log_token,
                                                            params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::Errors configuration_errors(params.locale_);
    
    rv = ngx::casper::broker::cdn::dl::Module::EnsureDirectives(ngx_http_casper_broker_module, a_r, loc_conf->log_token,
                                                                {
                                                                },
                                                                &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    const ngx::casper::broker::cdn::dl::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_download_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::dl::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::dl::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::dl::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::dl::Module::sk_tx_content_type_,
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [] (const std::string& a_locale) {
            return new ngx::casper::broker::cdn::Errors(a_locale);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ false,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::cdn::dl::Module::Initialize(config, params,
                                                          [&config, &params, &broker_conf, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                              return new ::ngx::casper::broker::cdn::dl::Module(config, params, *broker_conf, *loc_conf);
                                                          }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Called by nginx a request body is read to be read.
 *
 * @param a_request.
 */
void ngx::casper::broker::cdn::dl::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::cdn::dl::Module::ReadBody(ngx_http_casper_broker_cdn_download_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::dl::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::cdn::dl::Module::Cleanup(ngx_http_casper_broker_cdn_download_module, (ngx_http_request_t*) a_data);
}
