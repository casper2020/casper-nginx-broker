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

#include "ngx/casper/broker/jobify/module.h"

#include "ngx/casper/broker/ext/session.h"

#include "ngx/casper/broker/jobify/errors.h"

#include "ev/casper/session.h"

#include <algorithm>

/**
 * @brief Default constructor.
 *
 * @param a_config
 * @param a_params
 * @param a_ngx_loc_conf
 * @param a_ngx_jobify_loc_conf
 */
ngx::casper::broker::jobify::Module::Module (const ngx::casper::broker::Module::Config& a_config, const ngx::casper::broker::Module::Params& a_params,
                                          ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_jobify_module_loc_conf_t& a_ngx_jobify_loc_conf)
    : ngx::casper::broker::Module("jobify", a_config, a_params),
      job_(ctx_, this, a_params, a_ngx_loc_conf)
{
    // ... set JSON writer properties ...
    json_writer_.omitEndingLineFeed();
    // ...
    body_read_supported_methods_ = {
        NGX_HTTP_POST, NGX_HTTP_PUT, NGX_HTTP_PATCH, NGX_HTTP_DELETE
    };
    body_read_allow_empty_methods_ = {
      NGX_HTTP_POST, NGX_HTTP_PUT, NGX_HTTP_PATCH, NGX_HTTP_DELETE
    };
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::jobify::Module::~Module ()
{
    /* empty */
}

/**
 * @brief Process the previously defined HTTP request.
 *
 * @return
 */
ngx_int_t ngx::casper::broker::jobify::Module::Run ()
{
    const ::ev::casper::Session invalid_session(ctx_.loggable_data_ref_, /* a_iss */ "<invalid_iss>", /* a_sid */ "<invalid_sid>",  /* a_token_prefix */ "<invalid_prefix>",
                                                /* a_test_maintenance_flag */ false
    );
    
    // ... starts as a bad request ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_OK;
           
    // ... validate request URI ...
    if ( ctx_.request_.uri_.length() <= ctx_.request_.location_.length() ) {
        // ... rejected ...
        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR_I18N(ctx_, "BROKER_MISSING_OR_INVALID_URN_ERROR");
        // ... done ...
        return ctx_.response_.return_code_;
    }
    
     uri_ = ( ctx_.request_.uri_.c_str() + ctx_.request_.location_.length() );
    
     // ... first check with gatekeeper ...
     const ev::auth::route::Gatekeeper::Status& status = ::ev::auth::route::Gatekeeper::GetInstance().Allow(
             ctx_.request_.method_, uri_, invalid_session,
             {
               /* */
               std::bind(&ngx::casper::broker::jobify::Module::OnOnGatekeeperDeflectToJob, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
               /* */ nullptr
             },
             ctx_.loggable_data_ref_
     );
    
    // ... on error, prepare response ...
     if ( NGX_HTTP_OK != status.code_ ) {
         // ... set gatekeeper serialized response ...
         return NGX_BROKER_MODULE_SET_RESPONSE(ctx_, status.code_, ctx_.response_.content_type_, json_writer_.write(status.data_));
     } else if ( false == status.deflected_ ) {
         // ... gatekeer did not deflect the request ...
         return NGX_BROKER_MODULE_SET_NOT_ALLOWED(ctx_, "This request can't be deflected!");
     }

    // ... no error, just should be scheduled ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Called when the Gatekeeper deflects request to a job.
 *
 * @param a_tube
 * @param a_ttr
 * @param a_validity
 */
::ev::auth::route::Gatekeeper::DeflectorResult
 ngx::casper::broker::jobify::Module::OnOnGatekeeperDeflectToJob (const std::string& a_tube, ssize_t a_ttr, ssize_t a_valitity)
{
    ::ev::auth::route::Gatekeeper::DeflectorResult rv = {
        /* ttr_            */ ( a_ttr > 0 ? a_ttr : job_.ttr() ),
        /* validity_       */ ( a_valitity > 0 ? a_valitity : job_.expires_in() ),
        /* status_code_    */ NGX_HTTP_INTERNAL_SERVER_ERROR,
        /* status_message_ */ ""
    };
    const ngx_int_t post_rv = PostJob(ctx_.ngx_ptr_->method, uri_, nullptr != ctx_.request_.body_ ? ctx_.request_.body_ : "",
                                      a_tube,
                                      static_cast<ssize_t>(rv.ttr_),
                                      static_cast<ssize_t>(rv.validity_)
    );
    // ... set status properties ...
    if ( NGX_OK == post_rv ) {
        rv.status_code_    = NGX_HTTP_OK;
        rv.status_message_ = "job deflected!";
    } else {
        rv.status_code_    = ctx_.response_.status_code_;
        rv.status_message_ = "failed to post job!";
    }
    // ... we're done ...
    return rv;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Asynchronously, send a job to beanstalk and wait for response.
 *
 * @param a_method
 * @param a_urn
 * @param a_body
 * @param a_tube
 * @param a_ttr
 * @param a_validity
 */
ngx_int_t ngx::casper::broker::jobify::Module::PostJob (const ngx_int_t a_method,
                                                        const std::string& a_urn, const std::string& a_body,
                                                        const std::string& a_tube, const ssize_t a_ttr, const ssize_t a_validity)
{
    Json::Value object = Json::Value(Json::ValueType::objectValue);
    object["id"]              = "";
    object["tube"]            = a_tube;
    object["ttr"]             = static_cast<Json::UInt64>(a_ttr);
    object["validity"]        = static_cast<Json::UInt64>(a_validity);
    object["payload"]         = Json::Value(Json::ValueType::objectValue);
    object["payload"]["tube"] = a_tube;
    // ... set request METHOD ...
    object["payload"]["method"] = ctx_.request_.method_;
    // ... set request URN ...
    object["payload"]["urn"]  = a_urn;
    // ... set request BODY ...
    if ( a_body.length() > 0 ) {
        const bool is_json = ( nullptr != strcasestr(ctx_.request_.content_type_.c_str(), "application/json") ) ;
        if ( true == is_json ) {
            if ( false == json_reader_.parse(a_body, object["payload"]["body"]) ) {
                // ... an error occurred ...
                const auto errors = json_reader_.getStructuredErrors();
                if ( errors.size() > 0 ) {
                    throw Json::Exception("An error occurred while parsing request body as JSON: " +  errors[0].message + "!" );
                } else {
                    throw Json::Exception("An error occurred while parsing request body as JSON!");
                }
            }
        } else {
            object["payload"]["body"] = a_body;
        }
    }
    // ... set request HEADERS ...
    casper::broker::jobify::Module::GetUntouchedHeaders(ctx_.ngx_ptr_, object["payload"]["headers"]);
    // ... we're done  ...
    return job_.Submit(object);
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
ngx_int_t ngx::casper::broker::jobify::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_jobify_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_jobify_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_jobify_module);
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
        /* supported_content_types_ */ {
            "application/json", "application/json; charset=UTF-8",
            "text/plain", "text/plain; charset=UTF-8",
            "application/x-www-form-urlencoded"        
        }
    };
    
    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_jobify_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::jobify::Errors configuration_errors(params.locale_);
    
    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_jobify_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_jobify_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::jobify::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::jobify::Module::CleanupHandler,
        /* rx_content_type_       */ "text/plain; charset=utf-8",
        /* tx_content_type_       */ "application/json",
        /* log_token_             */ std::string(reinterpret_cast<const char*>(loc_conf->log_token.data), loc_conf->log_token.len),
        /* errors_factory_        */
        [] (const std::string& a_locale) {
            return new ngx::casper::broker::jobify::Errors(a_locale);
        },
        /* executor_factory_      */ nullptr,
        /* landing_page_url_      */ "",
        /* error_page_url_        */ "",
        /* serialize_errors_      */ true,
        /* at_rewrite_handler_    */ a_at_rewrite_handler
    };
    
    return ::ngx::casper::broker::Module::Initialize(config, params,
                                                     [&config, &params, &broker_conf, &loc_conf] () -> ::ngx::casper::broker::Module* {
                                                         return new ::ngx::casper::broker::jobify::Module(config, params, *broker_conf, *loc_conf);
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
void ngx::casper::broker::jobify::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_jobify_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::jobify::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_jobify_module, a_data);
}
