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
#pragma mark - CDN API MODULE
#endif

#include "ngx/casper/broker/cdn-api/module.h"

#include "ngx/casper/broker/cdn-api/module/version.h"

#include "ngx/casper/broker/cdn-common/errors.h"

#include "ngx/version.h"

#include "ngx/casper/broker/cdn-common/exception.h"

#include <regex>   // std::regex

const char* const ngx::casper::broker::cdn::api::Module::sk_rx_content_type_ = "application/vnd.api+json;charset=utf-8";
const char* const ngx::casper::broker::cdn::api::Module::sk_tx_content_type_ = "application/vnd.api+json;charset=utf-8";

ngx::casper::broker::cdn::common::db::Sideline::Settings ngx::casper::broker::cdn::api::Module::s_sideline_settings_ = {
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
 * param a_config
 * param a_params
 * param a_ngx_cdn_api_loc_conf
 * param a_ngx_cdn_api_loc_conf
 */
ngx::casper::broker::cdn::api::Module::Module (const ngx::casper::broker::Module::Config& a_config,
                                               const ngx::casper::broker::Module::Params& a_params,
                                               ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf, ngx_http_casper_broker_cdn_api_module_loc_conf_t& /* a_ngx_cdn_api_loc_conf */)
    : ngx::casper::broker::Module("cdn_api", a_config, a_params),
      router_(ctx_.loggable_data_ref_),
      job_(ctx_, this, a_ngx_loc_conf)
{
    json_writer_.omitEndingLineFeed();
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::api::Module::~Module ()
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
ngx_int_t ngx::casper::broker::cdn::api::Module::Setup ()
{
    const ngx_int_t crv = ngx::casper::broker::Module::Setup();
    if ( NGX_OK != crv ) {
        return crv;
    }
    
    // ... grab local configuration ...
    ngx_http_casper_broker_cdn_api_module_loc_conf_t* loc_conf =
        (ngx_http_casper_broker_cdn_api_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_cdn_api_module);
    if ( NULL == loc_conf ) {
        // ... set error
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                    "An error occurred while fetching this module configuration - nullptr!"
        );
        // ... nothing else to do ...
        return ctx_.response_.return_code_;
    }
    
    // ... one shot initialization ...
    if ( true == s_sideline_settings_.slaves_.isNull() ) {
        try {
            const char* const start_ptr = reinterpret_cast<const char* const>(loc_conf->sideline.slaves.data);
            const char* const end_ptr   = start_ptr + loc_conf->sideline.slaves.len;
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
            s_sideline_settings_.slaves_ = Json::Value(Json::ValueType::arrayValue);
            for ( Json::ArrayIndex idx = 0 ; idx < hosts.size() ; ++idx ) {
                Json::Value& slave = s_sideline_settings_.slaves_.append(Json::Value(Json::ValueType::objectValue));
                slave["host"] = hosts[idx];
            }
            s_sideline_settings_.tube_     = std::string(reinterpret_cast<const char*>(loc_conf->sideline.tube.data), loc_conf->sideline.tube.len);
            s_sideline_settings_.ttr_      = static_cast<uint64_t>(loc_conf->sideline.ttr);
            s_sideline_settings_.validity_ = static_cast<uint64_t>(loc_conf->sideline.validity);
        } catch (const Json::Exception& a_json_exception) {
            s_sideline_settings_.slaves_ = Json::Value::null;
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_json_exception.what());
        } catch (const ::cc::Exception& a_cc_exception) {
            s_sideline_settings_.slaves_ = Json::Value::null;
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
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
ngx_int_t ngx::casper::broker::cdn::api::Module::Run ()
{
    // ... if an error occurred ...
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        // ... we're done ...
        return ctx_.response_.return_code_;
    }
    // ... start as bad request ...
    ctx_.response_.status_code_  = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_  = NGX_ERROR;
    ctx_.response_.asynchronous_ = false;
    // ... process request ...
    try {
        router_.Enable(std::bind(&ngx::casper::broker::cdn::api::Module::RouteFactory, this, std::placeholders::_1, std::placeholders::_2));
        router_.Process(ctx_.ngx_ptr_->method, ctx_.request_.uri_,
                        /* a_body */ ( NGX_HTTP_GET != ctx_.ngx_ptr_->method ? ctx_.request_.body_ : nullptr ),
                        ctx_.request_.headers_,
                        type_, id_, params_,
                        ctx_.response_.asynchronous_
        );
        ctx_.response_.return_code_ = NGX_OK;
    } catch (const ngx::casper::broker::cdn::NotImplemented& /* a_not_implemented */) {
        NGX_BROKER_MODULE_SET_HTTP_METHOD_NOT_IMPLEMENTED(ctx_);
    } catch (const ngx::casper::broker::cdn::BadRequest& a_bad_request) {
        NGX_BROKER_MODULE_SET_BAD_REQUEST(ctx_, a_bad_request.what());
    } catch (const ngx::casper::broker::cdn::InternalServerError& a_internal_server_error) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_internal_server_error.what());
    } catch (const ngx::casper::broker::cdn::Exception& a_cdn_exception) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cdn_exception.what());
    } catch (const ::cc::Exception& a_ev_exception) {
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_ev_exception.what());
    } catch (...) {
        try {
            CC_EXCEPTION_RETHROW(/* a_unhandled */ true);
        } catch (const ::cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, a_cc_exception.what());
        }
    }
    
    // ... reset asynchronous flag?
    if ( NGX_OK != ctx_.response_.return_code_ ) {
        ctx_.response_.asynchronous_ = false;
    }
    
    // ... we're done
    return ctx_.response_.return_code_;
}

/**
 * @brief Called when a router request succeeded.
 *
 * @param a_status HTTP Status Code.
 * @param a_value Response, JSON object.
 */
void ngx::casper::broker::cdn::api::Module::OnRouterRequestSucceeded (const uint16_t a_status, const Json::Value& a_value)
{
    if ( 200 == a_status ) {
        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(this, a_status, ctx_.response_.content_type_, json_writer_.write(a_value).c_str());
    } else if ( 204 == a_status ) {
        NGX_BROKER_MODULE_SET_NO_CONTENT(ctx_);
        NGX_BROKER_MODULE_FINALIZE_REQUEST(this);
    } else {
        if ( 404 == a_status ) {
            NGX_BROKER_MODULE_SET_NOT_FOUND_ERROR(ctx_, "");
        } else {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, ( "Unexpected internal status code: " + std::to_string(a_status) + "!" ).c_str());
        }
        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
    }
}

/**
 * @brief Called when a router request faile.
 *
 * @param a_status HTTP Status Code.
 * @param a_value Response, JSON object.
 */
void ngx::casper::broker::cdn::api::Module::OnRouterRequestFailed (const uint16_t /* a_status */, const ::ev::Exception& a_value)
{
    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_value);
    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

ngx::casper::broker::cdn::api::Route* ngx::casper::broker::cdn::api::Module::RouteFactory (const char* const a_name, const ::ev::Loggable::Data& a_loggable_data_ref)
{
    if ( 0 == strcasecmp("billing", a_name) ) {
        return BillingFactory(a_loggable_data_ref);
    } else if ( 0 == strcasecmp("sideline", a_name) ) {
        return SidelineFactory(a_loggable_data_ref);
    }
    throw ::cc::Exception("Dont know how to create a '%s' API instance!", a_name);
}

/**
 * @brief Create a new instance of 'Billing' api.
 *
 * param a_loggable_data_ref
 *
 * @return A new instance of \link api::Billing \link, caller must delete object.
 */
ngx::casper::broker::cdn::api::Billing* ngx::casper::broker::cdn::api::Module::BillingFactory (const ::ev::Loggable::Data& a_loggable_data_ref)
{
    return new ngx::casper::broker::cdn::api::Billing(a_loggable_data_ref,
        /* a_callbacks */ {
            /* success_ */
            std::bind(&ngx::casper::broker::cdn::api::Module::OnRouterRequestSucceeded, this, std::placeholders::_1, std::placeholders::_2),
            /* failure_ */
            std::bind(&ngx::casper::broker::cdn::api::Module::OnRouterRequestFailed, this, std::placeholders::_1, std::placeholders::_2)
        }
    );
}

/**
 * @brief Create a new instance of 'Sideline' api.
 *
 * param a_loggable_data_ref
 *
 * @return A new instance of \link api::Sideline \link, caller must delete object.
 */
ngx::casper::broker::cdn::api::Sideline* ngx::casper::broker::cdn::api::Module::SidelineFactory (const ::ev::Loggable::Data& a_loggable_data_ref)
{
    return new ngx::casper::broker::cdn::api::Sideline(a_loggable_data_ref, s_sideline_settings_,
           /* a_callbacks */ {
               /* success_ */
               [this] (const uint16_t a_status, const Json::Value& a_value) {
                   // ... if tube ...
                   if ( 0 != s_sideline_settings_.tube_.length() ) {
                       // ... is set, submit a job ...
                       Json::Value job = Json::Value(Json::ValueType::objectValue);
                       job["tube"]               = s_sideline_settings_.tube_;
                       job["ttr"]                = s_sideline_settings_.ttr_;
                       job["validity"]           = s_sideline_settings_.validity_;
                       job["payload"]            = Json::Value(Json::ValueType::objectValue);
                       job["payload"]["sync_id"] = a_value["data"]["id"].asUInt64();
                       TrySubmitJob(job,
                                    [this, a_status, a_value](bool /* a_submitted */){
                                        ngx::casper::broker::cdn::api::Module::OnRouterRequestSucceeded(a_status, a_value);
                                    }
                       );
                   } else {
                       // ... is NOT set, nothing else to do ...
                       ngx::casper::broker::cdn::api::Module::OnRouterRequestSucceeded(a_status, a_value);
                   }
               },
               /* failure_ */
               std::bind(&ngx::casper::broker::cdn::api::Module::OnRouterRequestFailed, this, std::placeholders::_1, std::placeholders::_2)
           }
    );
}


#ifdef __APPLE__
#pragma mark - HELPER METHODS - Follow-up job related.
#endif

/**
 * @brief Try to submit a beanstalk job ( any error will be ignored ) and finalize request.
 *
 * @param a_payload  Job payload.
 * @param a_callback Function to call when done.
 */
void ngx::casper::broker::cdn::api::Module::TrySubmitJob (const Json::Value& a_payload,
                                                          std::function<void(bool a_submitted)> a_callback)
{
    // ... sanity check ...
    if ( false == ctx_.response_.asynchronous_ ) {
        throw ::cc::Exception("%s can't be called in a non asynchronous context!", __FUNCTION__);
    }

    // ... if no tube is set ...
    if ( 0 == s_sideline_settings_.tube_.length() || true == a_payload.isNull() ) {
        // ... no job will be submitted, set timeout to emulate async response ...
        ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(/* a_client */ this, /* a_ms */ 10,
                                                                    /* a_callback */
                                                                    [a_callback] () {
                                                                        a_callback(/* a_submitted*/ false);
                                                                    }
        );
    } else {
        // ... try to submit a job ...
        try {
            (void)job_.Submit(a_payload,
                              /* a_success_callback */
                              [a_callback] () {
                                    // ... finalize request ...
                                    a_callback(/* a_submitted*/ true);
                               },
                              /* a_failure_callback */
                              [a_callback] (const ev::Exception& /* a_ev_exception */) {
                                    // ... finalize request ...
                                    a_callback(/* a_submitted*/ false);
                              }
            );
        } catch (const ::ev::Exception& /* a_ev_exception */) {
            // ... no problem, set timeout to emulate async response ...
            ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(/* a_client */ this, /* a_ms */ 10,
                                                                        /* a_callback */
                                                                        [a_callback] () {
                                                                            a_callback(/* a_submitted*/ false);
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
ngx_int_t ngx::casper::broker::cdn::api::Module::Factory (ngx_http_request_t* a_r, bool a_at_rewrite_handler)
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
    ngx_http_casper_broker_cdn_api_module_loc_conf_t* loc_conf =
    (ngx_http_casper_broker_cdn_api_module_loc_conf_t*)ngx_http_get_module_loc_conf(a_r, ngx_http_casper_broker_cdn_api_module);
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
        /* supported_content_types_ */ { sk_rx_content_type_ }
    };

    ngx_int_t rv = ngx::casper::broker::Module::WarmUp(ngx_http_casper_broker_cdn_api_module, a_r, loc_conf->log_token,
                                                       params);
    if ( NGX_OK != rv ) {
        return rv;
    }

    //
    // 'MODULE' SPECIFIC CONFIG
    //
    ngx::casper::broker::cdn::common::Errors configuration_errors(params.locale_);

    rv = ngx::casper::broker::Module::EnsureDirectives(ngx_http_casper_broker_cdn_api_module, a_r, loc_conf->log_token,
                                                       {
                                                       },
                                                       &configuration_errors
    );
    if ( NGX_OK != rv ) {
        return rv;
    }

    const ngx::casper::broker::Module::Config config = {
        /* ngx_module_            */ ngx_http_casper_broker_cdn_api_module,
        /* ngx_ptr_               */ a_r,
        /* ngx_body_read_handler_ */ ngx::casper::broker::cdn::api::Module::ReadBodyHandler,
        /* ngx_cleanup_handler_   */ ngx::casper::broker::cdn::api::Module::CleanupHandler,
        /* rx_content_type_       */ ngx::casper::broker::cdn::api::Module::sk_rx_content_type_,
        /* tx_content_type_       */ ngx::casper::broker::cdn::api::Module::sk_tx_content_type_,
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
                                                         return new ::ngx::casper::broker::cdn::api::Module(config, params, *broker_conf, *loc_conf );
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
void ngx::casper::broker::cdn::api::Module::ReadBodyHandler (ngx_http_request_t* a_r)
{
    ngx::casper::broker::Module::ReadBody(ngx_http_casper_broker_cdn_api_module, a_r);
}

/**
 * @brief This method will be called when nginx is about to finalize a connection.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
void ngx::casper::broker::cdn::api::Module::CleanupHandler (void* a_data)
{
    ngx::casper::broker::Module::Cleanup(ngx_http_casper_broker_cdn_api_module, a_data);
}
