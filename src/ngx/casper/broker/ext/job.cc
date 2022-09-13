/**
 * @file job.cc
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

#include "ngx/casper/broker/ext/job.h"

#include "cc/b64.h"

#include "ev/beanstalk/producer.h"
#include "ev/ngx/bridge.h"

#include "ngx/casper/ev/glue.h"

#include "cc/b64.h"
#include "cc/auth/jwt.h"
#include "cc/auth/exception.h"
#include "cc/utc_time.h"

#include "ngx/version.h"

/**
 * @brief Default constructor.
 *
 * @param a_ctx
 * @param a_module
 */
ngx::casper::broker::ext::Job::Job (ngx::casper::broker::Module::CTX& a_ctx, ngx::casper::broker::Module* a_module,
                                    const ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf)
    : ngx::casper::broker::ext::Base(a_ctx, a_module)
{
    //
    // ... scheduler is required ...
    //
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
    
    // ... the following variables should ensured by called ...
    jwt_rsa_public_key_uri_ = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_rsa_public_key_uri.data), a_ngx_loc_conf.jwt_rsa_public_key_uri.len);

    // ... copy beanstalkd config ...
    beanstalk_config_ = ::ngx::casper::ev::Glue::GetInstance().BeanstalkdConfig();
    beanstalk_config_.tubes_.clear();

    // ... submit related ...
    job_id_key_                = ::ngx::casper::ev::Glue::GetInstance().JobIDKey();
    job_object_                = Json::Value::null;
    job_patch_object_          = Json::Value::null;
    job_status_                = Json::Value::null;
    job_ttr_                   = 60;
    job_validity_              = 120;
    job_expires_in_            = ctx_.request_.connection_validity_;

    // ... won't be used in this context, but need to initialize variables ...
    primitive_protocol_[0]     = { nullptr, nullptr , 0 }; /* status code @ unsigned_value */
    primitive_protocol_[1]     = { nullptr, nullptr , 0 }; /* content-type */
    primitive_protocol_[2]     = { nullptr, nullptr , 0 }; /* body */
    
    // ... set JSON writer settings ...
    json_writer_.omitEndingLineFeed();
}

/**
 * @brief Default constructor.
 *
 * @param a_ctx
 * @param a_module
 * @param a_params
 * @param a_ngx_loc_conf
 */
ngx::casper::broker::ext::Job::Job (ngx::casper::broker::Module::CTX& a_ctx, ngx::casper::broker::Module* a_module,
                                    const ngx::casper::broker::Module::Params& a_params, const ngx_http_casper_broker_module_loc_conf_t& a_ngx_loc_conf)
    : ngx::casper::broker::ext::Base(a_ctx, a_module)
{
    //
    // ... scheduler is required ...
    //
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
    
    // ... the following variables should ensured by called ...
    jwt_rsa_public_key_uri_ = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_rsa_public_key_uri.data), a_ngx_loc_conf.jwt_rsa_public_key_uri.len);
    
    // ... copy beanstalkd config ...
    beanstalk_config_ = ::ngx::casper::ev::Glue::GetInstance().BeanstalkdConfig();
    beanstalk_config_.tubes_.clear();
    
    // ... submit related ...
    job_id_key_                = ::ngx::casper::ev::Glue::GetInstance().JobIDKey();
    job_object_                = Json::Value::null;
    job_patch_object_          = Json::Value::null;
    job_status_                = Json::Value::null;
    job_ttr_                   = 60;
    job_validity_              = 120;
    job_expires_in_            = ctx_.request_.connection_validity_;
    
    // ... can be used in this context ...
    primitive_protocol_[0]     = { nullptr, nullptr , 0 }; /* status code @ unsigned_value */
    primitive_protocol_[1]     = { nullptr, nullptr , 0 }; /* content-type */
    primitive_protocol_[2]     = { nullptr, nullptr , 0 }; /* body */
    
    // ... if no error set ...
    if ( NGX_OK == ctx_.response_.return_code_ ) {
        // ... handle JSON directives ...
        try {
            if ( false == json_reader_.parse(std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_iss.data), a_ngx_loc_conf.jwt_iss.len), jwt_allowed_iss_) ) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Unable to parse 'jwt_iss' directive data!");
            } else if ( true == jwt_allowed_iss_.isNull() || Json::ValueType::arrayValue != jwt_allowed_iss_.type() || 0 == jwt_allowed_iss_.size() ) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Invalid 'jwt_iss' directive data format!");
            }
        } catch (const Json::Exception& a_json_exception) {
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_json_exception);
        }
    }
    
    // ... if no error set ...
    if ( NGX_OK == ctx_.response_.return_code_ ) {
        // ... casper-extra-job-params ...
        const auto casper_extra_job_params_it = a_params.in_headers_.find("casper-extra-job-params");
        if ( a_params.in_headers_.end() != casper_extra_job_params_it ) {
            // ... decode and parse 'casper-extra-job-params' data ...
            std::string payload;
            try {
                payload = ::cc::base64_url_unpadded::decode<std::string>(casper_extra_job_params_it->second);
            } catch (...) {
                try {
                    payload = ::cc::base64_rfc4648::decode<std::string>(casper_extra_job_params_it->second);
                } catch (const std::domain_error& a_d_e) {
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_,
                            ( std::string("An error occurred while decoding 'casper-extra-job-params': ") +  a_d_e.what() ).c_str()
                    );
                } catch (...) {
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_,
                            "An error occurred while decoding 'casper-extra-job-params'!"
                    );
                }
            }
            // ... if no decoding error is set ...
            if ( NGX_OK == ctx_.response_.return_code_ ) {
                // ... parse 'casper-extra-job-params' ...
                if ( false == json_reader_.parse(payload, job_patch_object_) ) {
                    // ... an error occurred ...
                    const auto errors = json_reader_.getStructuredErrors();
                    if ( errors.size() > 0 ) {
                        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_,
                                                                 ( "An error occurred while parsing JSON 'casper-extra-job-params' payload: " +  errors[0].message + "!" ).c_str()
                        );
                    } else {
                        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, "An error occurred while parsing JSON 'casper-extra-job-params' payload!");
                    }
                } else if ( Json::ValueType::objectValue != job_patch_object_.type() ) {
                    NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_,
                                                            ( "Invalid JSON object type for 'casper-extra-job-params': expecting " +
                                                               std::to_string(Json::ValueType::objectValue) +
                                                               " got " +
                                                               std::to_string(job_patch_object_.type())
                                                             ).c_str()
                    );
                }
            }
            // ... if no parsing error is set ...
            if ( NGX_OK == ctx_.response_.return_code_ ) {
                // ... set job allowed patch objects ...
                if ( a_ngx_loc_conf.jwt_allowed_patch_members_set.len > 0 ) {
                    const std::string payload = std::string(reinterpret_cast<const char*>(a_ngx_loc_conf.jwt_allowed_patch_members_set.data), a_ngx_loc_conf.jwt_allowed_patch_members_set.len);
                    Json::Value allowed_patch_members;
                    if ( false == json_reader_.parse(payload, allowed_patch_members) ) {
                        // ... an error occurred ...
                        const auto errors = json_reader_.getStructuredErrors();
                        if ( errors.size() > 0 ) {
                            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                        ( "An error occurred while parsing JSON 'job_allowed_patch_members_set': " +
                                                                         errors[0].message + "!"
                                                                         ).c_str()
                            );
                        } else {
                            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while parsing JSON 'job_allowed_patch_members_set'!");
                        }
                    } else if ( Json::ValueType::arrayValue != allowed_patch_members.type() ) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                    ( "Invalid JSON object type for 'job_allowed_patch_members_set': expecting " +
                                                                     std::to_string(Json::ValueType::arrayValue) +
                                                                     " got " +
                                                                     std::to_string(allowed_patch_members.type())
                                                                     ).c_str()
                        );
                    }
                    for ( Json::ArrayIndex idx = 0 ; idx < allowed_patch_members.size() ; ++idx ) {
                        if ( false == allowed_patch_members[idx].isString() ) {
                            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                        ( "An error occurred while parsing JSON 'job_allowed_patch_members_set' at index " +
                                                                         std::to_string(idx) + " string value is expected!"
                                                                         ).c_str()
                            );
                            break;
                        }
                        job_allowed_patch_members_set_.insert(allowed_patch_members[idx].asString());
                    }
                }
            }
        }
    }

    // ... set JSON writer settings ...
    json_writer_.omitEndingLineFeed();
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::ext::Job::~Job ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
    ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Submit a job request.
 *
 * @param a_object Job definition.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::ext::Job::Submit (const Json::Value& a_object)
{
    return Submit(a_object, /* a_success_callback */ nullptr, /* a_failure_callback */ nullptr);
}

/**
 * @brief Decode and submit a job request.
 *
 * @param a_object Job definition.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::ext::Job::Submit (const std::string& a_jwt)
{
    // ... start with a 'bad request' ...
    ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
    ctx_.response_.return_code_ = NGX_ERROR;
    
    const std::string jwt_b64 = cc::auth::JWT::MakeBrowsersUnhappy(a_jwt);
        
    // ... decode and send job to beanstalk queue ...
    const int64_t now = cc::UTCTime::Now();
        
    try {
        
        for ( Json::ArrayIndex idx = 0; idx < jwt_allowed_iss_.size() ; ++idx ) {
            
            ::cc::auth::JWT jwt(jwt_allowed_iss_[idx].asCString());
            
            // .. try to decode JWT w/ current 'ISS'...
            
            try {
                
                jwt.Decode(jwt_b64, jwt_rsa_public_key_uri_);
                
                const Json::Value action = jwt.GetUnregisteredClaim("action");
                if ( true == action.isNull() || false == action.isString() ) {
                    NGX_BROKER_MODULE_THROW_EXCEPTION(ctx_, "JWT unregistered claim '%s': not set!", "action");
                }
                
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
                
                if ( 0 == strcasecmp(action.asCString(), "redirect") ) {
                    
                    return HandleRedirect(jwt.GetUnregisteredClaim("redirect"));
                    
                } else if ( 0 == strcasecmp(action.asCString(), "job") ) {
                    
                    return Submit(jwt.GetUnregisteredClaim("job"));
                    
                } else {
                    // ... no other action is expected here! ...
                    return NGX_BROKER_MODULE_SET_ACTION_NOT_IMPLEMENTED(ctx_, action.asCString());
                }
                
            }  catch (const ::cc::auth::Exception& a_cc_auth_exception) {
                // ... track exception ...
                NGX_BROKER_MODULE_TRACK_EXCEPTION(ctx_, a_cc_auth_exception);
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
    
    // ... we're done ...
    return ctx_.response_.return_code_;
}

/**
 * @brief Submit a job request.
 *
 * @param a_object Job definition.
 * @param a_success_callback
 * @param a_failure_callback
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::ext::Job::Submit (const Json::Value& a_object,
                                                 const std::function<void()> a_success_callback,
                                                 const std::function<void(const ::ev::Exception&)> a_failure_callback)
{
    // ... if not controlled externally ...
    if ( nullptr == a_success_callback ) {
        // ... start with a 'bad request' ...
        ctx_.response_.status_code_ = NGX_HTTP_BAD_REQUEST;
        ctx_.response_.return_code_ = NGX_ERROR;
    } else {
        // ... external control must be ready to deal with asynchronous response ...
        if ( false == ctx_.response_.asynchronous_ ) {
            throw ::ev::Exception("Called must be ready to deal with asynchronous response!");
        }
    }
    
    // ... track job object ...
    job_object_ = a_object;
    // ...
    if ( false == job_patch_object_.isNull() ) {
        for ( auto member : job_patch_object_.getMemberNames() ) {
            // ... if already set ...
            if ( true == job_object_.isMember(member) ) {
                // ... ignore it ...
                continue;
            }
            // ... if is not allowed ...
            if ( job_allowed_patch_members_set_.end() == job_allowed_patch_members_set_.find(member) ) {
                // ... ignore it ...
                continue;
            }
            // ... patch object payload with patch object member ...
            job_object_["payload"][member] = job_patch_object_[member];
        }
    }
    
    // ... set ttr && expires in ...
    job_ttr_         = static_cast<size_t>(job_object_.get("ttr", static_cast<Json::Value::UInt64>(job_ttr_)).asUInt64());             // in seconds
    job_validity_    = static_cast<size_t>(job_object_.get("validity", static_cast<Json::Value::UInt64>(job_expires_in_)).asUInt64()); // in seconds
    job_expires_in_  = job_validity_ + job_ttr_;
    job_tube_        = job_object_["tube"].asString();

    // ... yes ... run a task to reserve it ...
    module_ptr_->NewTask([this] () -> ::ev::Object* {
                         
        // ...  get new job id ...
        return new ::ev::redis::Request(ctx_.loggable_data_ref_, "INCR", {
            /* key   */ job_id_key_
        });
                         
     })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // INCR:
        //
        // - An integer reply is expected:
        //
        //  - the value of key after the increment
        //
        const ::ev::redis::Value& value = ::ev::redis::Reply::EnsureIntegerReply(a_object);
        
        job_object_["id"] = std::to_string(value.Integer());
        
        // ... set job key ...
        job_key_     = ( module_ptr_->service_id_ + ":jobs:" + job_object_["tube"].asString() + ':' + job_object_["id"].asString() );
        job_channel_ = ( module_ptr_->service_id_ + ':' + job_object_["tube"].asString() + ':' + job_object_["id"].asString() );
        
        // ... first, set queued status ...
        return new ::ev::redis::Request(ctx_.loggable_data_ref_, "HSET", {
            /* key   */ job_key_,
            /* field */ "status", "{\"status\":\"queued\"}"
        });
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HSET:
        //
        // - An integer reply is expected:
        //
        //  - 1 if field is a new field in the hash and value was set.
        //  - 0 if field already exists in the hash and the value was updated.
        //
        (void)::ev::redis::Reply::EnsureIntegerReply(a_object);
        
        return new ::ev::redis::Request(ctx_.loggable_data_ref_, "EXPIRE", { job_key_, std::to_string(job_expires_in_) });
        
    })->Finally([this, a_success_callback] (::ev::Object* a_object) {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ::ev::redis::Reply::EnsureIntegerReply(a_object, 1);

        // ... patch job id and tube ...
        job_object_["payload"]["id"] = job_object_["id"];
        if ( false == job_object_["payload"].isMember("tube") ) {
            job_object_["payload"]["tube"] = job_tube_;
        }

        if ( nullptr == a_success_callback ) {
            // ...ADD the job to beanstalkd and wait for reply ...
            ScheduleJob(job_channel_);
        } else {
            // ... JUST add job to beanstalkd, not waiting for reply ...
            // ... connect and sent job ...
            try {
                
                ::ev::beanstalk::Producer producer(beanstalk_config_, job_tube_);
                
                const int64_t status = producer.Put(json_writer_.write(job_object_["payload"]),
                                                    /* a_priority = 0 */ 0,
                                                    /* a_delay = 0 */ 0,
                                                    static_cast<uint32_t>(job_ttr_)
                );
                
                if ( status < 0 ) {
                    throw ::ev::Exception("Beanstalk client returned with error code " + std::to_string(status) + ", while adding job '"
                                            +
                                           job_object_["id"].asString() + "' to '" + job_tube_ + "' tube!"
                    );
                }
                
            } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
                throw ::ev::Exception("BEANSTALK CONNECTOR: %s", a_btc_exception.what());
            }
            // ... notify caller ...
            a_success_callback();
        }
        
    })->Catch([this, a_failure_callback] (const ::ev::Exception& a_ev_exception) {
        
        if ( nullptr == a_failure_callback ) {
            // ... and error occurred while trying to subscribe a REDIS channel ...
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_ev_exception);
            NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
        } else {
            // ... notify caller ...
            a_failure_callback(a_ev_exception);
        }
        
    });
    
    // ... if not controlled externally ...
    if ( nullptr == a_success_callback ) {
        // ... job id reservation is an asynchronous operation ...
        // ... so this module response is also asynchronous ...
        ctx_.response_.asynchronous_ = true;
        ctx_.response_.return_code_ = NGX_OK;
        // ... we're done ...
        return ctx_.response_.return_code_;
    } else {
        // ... we're done ...
        return NGX_OK;
    }
    
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Process a redirect request.
 *
 * @param a_object Redirect definition.
 *
 * @return NGINX return code.
 */
ngx_int_t ngx::casper::broker::ext::Job::HandleRedirect (const Json::Value& a_object)
{
    if ( true == a_object.isNull() || false == a_object.isObject() ) {
        return NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_,
                                                       ( "Invalid response object type: expected " +
                                                        std::to_string(Json::ValueType::objectValue) + ", got " + std::to_string(a_object.type())  + "!"
                                                       ).c_str()
        );
    }
    
    const Json::Value protocol = a_object.get("protocol", Json::Value::null);
    if ( true == protocol.isNull() || false == protocol.isString() || 0 == protocol.asString().length() ) {
        return NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(ctx_, "protocol");
    }
    
    const Json::Value host = a_object.get("host", Json::Value::null);
    if ( true == host.isNull() || false == host.isString() || 0 == host.asString().length() ) {
        return NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(ctx_, "host");
    }
    
    const Json::Value port = a_object.get("port", Json::Value::null);
    if ( false == port.isNull() && false == port.isUInt() ) {
        return NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(ctx_, "port");
    }
    
    const Json::Value path = a_object.get("path", Json::Value::null);
    if ( false == path.isNull() && ( false == path.isString() || 0 == path.asString().length() ) ) {
        return NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(ctx_, "path");
    }
    
    const Json::Value file = a_object.get("file", Json::Value::null);
    if ( true == file.isNull() || false == file.isString() || 0 == file.asString().length() ) {
        return NGX_BROKER_MODULE_SET_INVALID_OR_MISSING_JSON_OBJECT_FIELD_ERROR(ctx_, "file");
    }
    
    ctx_.response_.redirect_.protocol_     = protocol.asString();
    ctx_.response_.redirect_.host_         = host.asString();
    ctx_.response_.redirect_.port_         = port.asUInt();
    ctx_.response_.redirect_.path_         = path.asString();
    ctx_.response_.redirect_.file_         = file.asString();
    ctx_.response_.redirect_.internal_     = true;
    ctx_.response_.redirect_.asynchronous_ = true;

     // /<location>/<protocol>/<host>/<port>/<file>    
    ctx_.response_.redirect_.uri_ = "";
    if ( '/' != ctx_.response_.redirect_.path_.c_str()[0] ) {
        ctx_.response_.redirect_.uri_ += '/';
    }
    ctx_.response_.redirect_.uri_ += ctx_.response_.redirect_.path_;
    
    if ( '/' != ctx_.response_.redirect_.uri_.c_str()[ctx_.response_.redirect_.uri_.length() - 1] ) {
        ctx_.response_.redirect_.uri_ += '/';
    }
    ctx_.response_.redirect_.uri_ += ctx_.response_.redirect_.protocol_ + '/' + ctx_.response_.redirect_.host_ + '/' + std::to_string(ctx_.response_.redirect_.port_);
    
    if ( '/' != ctx_.response_.redirect_.uri_.c_str()[ctx_.response_.redirect_.uri_.length() - 1] ) {
        ctx_.response_.redirect_.uri_ += '/';
    }
    ctx_.response_.redirect_.uri_ += ctx_.response_.redirect_.file_;
    
    ctx_.response_.status_code_        = NGX_HTTP_OK;
    ctx_.response_.return_code_        = NGX_OK;
    
    return NGX_OK;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Schedule a job.
 *
 * @param a_name
 *
 * @return
 */
ngx_int_t ngx::casper::broker::ext::Job::ScheduleJob (const std::string& a_name)
{
    try {
        // ... first a REDIS channel must be subscribed ....
        ::ev::redis::subscriptions::Manager::GetInstance().SubscribeChannels(/* a_channels */
                                                                             { a_name },
                                                                             /* a_status_callback */
                                                                             std::bind(&ngx::casper::broker::ext::Job::JobSubscriptionCallback, this, std::placeholders::_1, std::placeholders::_2),
                                                                             /* a_data_callback */
                                                                             std::bind(&ngx::casper::broker::ext::Job::JobMessageCallback, this, std::placeholders::_1, std::placeholders::_2),
                                                                             /* a_client */
                                                                             this
        );
        
        // ... REDIS channel subscription is an asynchronous operation ...
        // ... so this module response ...
        ctx_.response_.asynchronous_ = true ;
        ctx_.response_.return_code_  = NGX_OK;
        
        return ctx_.response_.return_code_;
        
    } catch (const ::ev::Exception& a_ev_exception) {
        // ... and error occurred while trying to subscribe a REDIS channel ...
        return NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_ev_exception);
    }
    
    return ctx_.response_.return_code_;
}

/**
 * @brief This method will be called when the status of a REDIS subscription ( for a specific a job ) has changed.
 *
 * @param a_name
 * @param a_status
 *
 * @return
 */
EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK ngx::casper::broker::ext::Job::JobSubscriptionCallback (const std::string& /* a_name */,
                                                                                                            const ::ev::redis::subscriptions::Manager::Status& a_status)
{
    switch (a_status) {
            
        case ::ev::redis::subscriptions::Manager::Status::Subscribing:
            break;
            
        case ::ev::redis::subscriptions::Manager::Status::Subscribed:
        {
            bool accepted = true;

            // ... set new connection to a specific 'tube' ...
            const std::string tube = job_object_["tube"].asString();
            
            // ... connect and sent job ...
            try {
                                
                ::ev::beanstalk::Producer producer(beanstalk_config_, tube);
                
                const std::string payload = json_writer_.write(job_object_["payload"]);
                
                // ... log ...
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s ~> %s",
                                                  "JB : ID",
                                                  job_id_key_.c_str(), job_object_["payload"]["id"].asCString()
                );
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s",
                                                  "JB : TUBE",
                                                  job_object_["payload"]["tube"].asCString()
                );
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", " SIZET_FMT " second(s)",
                                                  "JB : TTR",
                                                  job_ttr_
                );
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", " SIZET_FMT " second(s)",
                                                  "JB : VALIDITY",
                                                  job_validity_
                );
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s",
                                                  "JB : KEY",
                                                  job_key_.c_str()
                );
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s",
                                                  "JB : CHANNEL",
                                                  job_channel_.c_str()
                );
                
                // ... check for max timeout enforcement ...
                ngx_http_casper_broker_module_loc_conf_t* broker_conf = (ngx_http_casper_broker_module_loc_conf_t*)ngx_http_get_module_loc_conf(ctx_.ngx_ptr_, ngx_http_casper_broker_module);
                if ( NULL == broker_conf ) {
                    throw ::ev::Exception("%s", "Unable to access local config!");
                }
                
                // ... log ...
                ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                  NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s",
                                                  "JB : PAYLOAD",
                                                  ( 1 == broker_conf->jobs.log.payload ? payload.c_str() : "<redacted>" ) 
                );
                
                // ... timeout above limit?
                if ( job_expires_in_ > broker_conf->jobs.timeouts.max ) {
                    // ... yes, enforce?
                    if ( 1 == broker_conf->jobs.timeouts.enforce ) {
                        // ... yes, log ...
                        ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                      NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT
                                                      ", ':no_entry: `%s` - %s - *timeout value above " SIZET_FMT " second(s)*! "
                                                      "```{\"timeout\":" SIZET_FMT ",\"ttr\":" SIZET_FMT ",\"validity\":" SIZET_FMT ",\"tube\":\"%s\"}```"
                                                      " Job timeout value, " SIZET_FMT " second(s), must be below " SIZET_FMT " second(s).'",
                                                      "JB : ALERT",
                                                      NGX_NAME, job_key_.c_str(), static_cast<size_t>(broker_conf->jobs.timeouts.max),
                                                      job_expires_in_, job_ttr_, job_validity_, job_tube_.c_str(),
                                                      job_expires_in_, static_cast<size_t>(broker_conf->jobs.timeouts.max)
                        );
                        // ... and set error ...
                        NGX_BROKER_MODULE_SET_BAD_REQUEST_ERROR(ctx_, ( "Job timeout value, " + std::to_string(job_expires_in_) + " second(s), must be below " + std::to_string(broker_conf->jobs.timeouts.max) + " second(s)!").c_str());
                        // ... not acceptable ...
                        accepted = false;
                    } else {
                        // ... no, just log a warning ...
                        ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT
                                                          ", ':warning: `%s` - %s - *timeout value above " SIZET_FMT " second(s)*! "
                                                          "```{\"timeout\":" SIZET_FMT ",\"ttr\":" SIZET_FMT ",\"validity\":" SIZET_FMT ",\"tube\":\"%s\"}```"
                                                          " Job timeout value, " SIZET_FMT " second(s), should be below " SIZET_FMT " second(s).'",
                                                          "JB : ALERT",
                                                          NGX_NAME, job_key_.c_str(), static_cast<size_t>(broker_conf->jobs.timeouts.max),
                                                          job_expires_in_, job_ttr_, job_validity_, job_tube_.c_str(),
                                                          job_expires_in_, static_cast<size_t>(broker_conf->jobs.timeouts.max)
                        );
                    }
                }
                
                // ... accepted?
                if ( true == accepted ) {

                    const int64_t status = producer.Put(payload,
                                                        /* a_priority = 0 */ 0,
                                                        /* a_delay = 0 */ 0,
                                                        static_cast<uint32_t>(job_ttr_)
                    );
                    if ( status < 0 ) {
                        // ... an error must be set ...
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                    ( "Beanstalk client returned with error code " + std::to_string(status) + ", while adding job '" +
                                                                     job_object_["id"].asString() + "' to '" + tube + "' tube!"
                                                                    ).c_str()
                        );
                        // ... not acceptable ...
                        accepted = false;
                    }
                    
                    // ... still acceptable?
                    if ( true == accepted ) {
                        // ... yes, log ...
                        ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                                          NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", " SIZET_FMT " second(s)",
                                                          "JB : TIMEOUT IN",
                                                          job_expires_in_
                        );
                        // ... and schedule timeout ...
                        ::ev::scheduler::Scheduler::GetInstance().SetClientTimeout(
                                                                                   /* a_client */
                                                                                   this,
                                                                                   /* a_ms */
                                                                                   static_cast<uint64_t>(job_expires_in_ * 1000),
                                                                                   /* a_callback */
                                                                                   [this] () {
                                                                                       NGX_BROKER_MODULE_SET_GATEWAY_TIMEOUT_ERROR(ctx_,
                                                                                                                                   ( "Job waiting timed out after " + std::to_string(job_expires_in_) + " second(s)" ).c_str()
                                                                                       );
                                                                                       ctx_.response_.serialize_errors_ = true;
                                                                                       NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
                                                                                   }
                        );
                    }

                }
                
            } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
                // ... we're done ...
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, ::ev::Exception("BEANSTALK CONNECTOR: %s", a_btc_exception.what()));
                // ... not acceptable ...
                accepted = false;
            } catch (const ::ev::Exception& a_ev_exception) {
                // ... we're done ...
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_ev_exception);
                // ... not acceptable ...
                accepted = false;
            }
            
            // ... if an error is set ...
            if ( false == accepted || NGX_HTTP_INTERNAL_SERVER_ERROR == ctx_.response_.status_code_ ) {
                // ... we must finalize request with errors serialization ...
                return [this] () {
                    // ... and we're done ...
                    ctx_.response_.serialize_errors_ = true;
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
                };
            }
            
            break;
        }
            
        default:
            break;
            
    }
    
    // ... we're done ...
    return nullptr;
}

/**
 * @brief This method will be called when a new message from a specific a job is ready.
 *
 * @param a_name
 * @param a_message
 *
 * @return
 */
EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK ngx::casper::broker::ext::Job::JobMessageCallback (const std::string& /* a_name */, const std::string & a_message)
{
    try {
        
        //
        // *<status-code-int-value>,<content-type-length-in-bytes>,<content-type-string-value>,<body-length-bytes>,<body>
        //
        // or
        //
        // {
        //    "action" : "" - [progress|redirect|response] - default [redirect]
        //    ...
        // }
        
        job_status_ = Json::Value::null;
        
        // ... cancel scheduled timeout ( if any ) ...
        ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
        
        // ... invalid, primitive or JSON protocol?
        if ( 0 == a_message.length() ) {
            
            // ... not enough bytes ...
            NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                        "Invalid JOB response message - no data received!"
            );
            
            // ... stop accepting messages from this job ...
            ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
            
            // ... we must finalize request with errors serialization ...
            return [this] () {
                // ... and we're done ...
                NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
            };
        
        } else if ( '*' == a_message.c_str()[0] || '!' == a_message.c_str()[0] ) {
            
            // ... stop accepting messages from this job ...
            ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);

            // ... straight response ...
            
            const char* const c_str = a_message.c_str();
            
            // expecting: *<status-code-int-value>,<content-type-length-in-bytes>,<content-type-string-value>,<body-length-bytes>,<body>
            // expecting: !<status-code-int-value>,<content-type-length-in-bytes>,<content-type-string-value>,<body-length-bytes>,<body>,<headers-length-bytes>,<headers>,...
            
            primitive_protocol_[0] = { c_str + 1, strchr(c_str + 1, ','), 0 }; /* status code @ unsigned_value */
            primitive_protocol_[1] = { nullptr  , nullptr               , 0 }; /* content-type */
            primitive_protocol_[2] = { nullptr  , nullptr               , 0 }; /* body */

            // ... read status code ....
            if ( nullptr == primitive_protocol_[0].end_ || 1 != sscanf(primitive_protocol_[0].start_, "%u", &primitive_protocol_[0].unsigned_value_) ) {
                NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Invalid message format near field #0");
            } else {
                // ... read all other components ...
                for ( size_t idx = 1 ; idx < ( sizeof(primitive_protocol_) / sizeof(primitive_protocol_[0]) ) ; ++idx ) {
                    if ( 1 != sscanf(primitive_protocol_[idx-1].end_ + sizeof(char), "%u", &primitive_protocol_[idx].unsigned_value_) ) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                    (
                                                                     "Invalid message format near field #" + std::to_string(idx) ).c_str()
                                                                    );
                        break;
                    }
                    primitive_protocol_[idx].start_ = strchr(primitive_protocol_[idx-1].end_ + sizeof(char), ',');
                    if ( nullptr == primitive_protocol_[idx].start_ ) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                    (
                                                                     "Invalid message format near field #" + std::to_string(idx) ).c_str()
                                                                    );
                        break;
                    }
                    primitive_protocol_[idx].start_ += sizeof(char);
                    primitive_protocol_[idx].end_    = primitive_protocol_[idx].start_ + primitive_protocol_[idx].unsigned_value_;
                }
                // ... extended?
                if ( '!' == a_message.c_str()[0] ) {
                    size_t idx = 3;
                    const char* ptr = primitive_protocol_[2].start_ + primitive_protocol_[2].unsigned_value_;
                    while ( '\0' != ptr[0] && ',' == ptr[0] ) {
                        ptr += sizeof(char);
                        unsigned len = 0;
                        if ( 1 != sscanf(ptr, "%u,", &len) ) {
                            break;
                        }
                        ptr = strchr(ptr, ',');
                        if ( nullptr == ptr ) {
                            break;
                        }
                        ptr += sizeof(char);
                        {
                            const char* v = strchr(ptr, ':');
                            if ( nullptr == v ) {
                                break;
                            }
                            const char* kv = ptr; const size_t kl = ( v - ptr );
                            v+= sizeof(char);
                            if ( ' ' == v[0] ) {
                                v += sizeof(char);
                            }
                            const size_t vl = len - ( v - ptr );
                            ctx_.response_.headers_[std::string(kv, kl)] = std::string(v, vl);
                        }
                        ++idx;
                        ptr += static_cast<size_t>(len);
                    }
                    // ... failed?
                    if ( not ( '\0' == ptr[0] ) ) {
                        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                    ( "Invalid message format near field #" + std::to_string(idx) ).c_str()
                        );
                    }
                }
            }

            // ... if an error is set ...
            if ( NGX_HTTP_INTERNAL_SERVER_ERROR == ctx_.response_.status_code_ ) {
                // ... reset ...
                primitive_protocol_[0] = { nullptr, nullptr , 0 };
                primitive_protocol_[1] = { nullptr, nullptr , 0 };
                primitive_protocol_[2] = { nullptr, nullptr , 0 };
                // ... we must finalize request with errors serialization ...
                return [this] () {
                    // ... and we're done ...
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
                };
            } else {
                // ... good to go ...
                ctx_.response_.status_code_ = static_cast<uint16_t>(primitive_protocol_[0].unsigned_value_);

                // ... if it's an unauthorized response ...
                if ( '!' != a_message.c_str()[0] && NGX_HTTP_UNAUTHORIZED == ctx_.response_.status_code_ ) {
                    ctx_.response_.headers_["WWW-Authenticate"] =
                        "Bearer realm=\"api\", error=\"invalid_token\", error_description=\"The access token provided is expired, revoked, malformed, or invalid for other reasons.\"";
                    // ... we must finalize request with errors serialization ...
                    return [this] () {
                        // ... and we're done ...
                        module_ptr_->ctx_.response_.binary_ = false;
                        NGX_BROKER_MODULE_SET_ERROR_I18N(ctx_, NGX_HTTP_UNAUTHORIZED, "AUTH_INVALID_SESSION_ID");
                        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
                    };
                }

                // ... we're done ...
                return [this] () {
                    
                    const std::string content_type = std::string(primitive_protocol_[1].start_, primitive_protocol_[1].unsigned_value_);
                    
                    const bool is_text = (
                         nullptr != strstr(content_type.c_str(), "application/text") ||
                         nullptr != strstr(content_type.c_str(), "application/xml")  ||
                         nullptr != strstr(content_type.c_str(), "application/json") ||
                         nullptr != strstr(content_type.c_str(), "application/vnd.api+json")
                    );
                    
                    // ... by finalizing request with an 'text' response ....
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE_AND_LENGTH(module_ptr_,
                                                                     /* status-code  */ ctx_.response_.status_code_,
                                                                     /* content-type */ content_type,
                                                                     /* body         */ std::string(primitive_protocol_[2].start_, primitive_protocol_[2].unsigned_value_),
                                                                     /* length       */ primitive_protocol_[2].end_ - primitive_protocol_[2].start_,
                                                                     /* binary       */  ( false == is_text )
                    );
                };
            }
            
        } else { // ... expecting a 'JSON' response ...
            
            ::ev::LoggerV2::GetInstance().Log(module_ptr_->logger_client_, "cc-modules",
                                              NRS_NGX_CASPER_BROKER_MODULE_LOGGER_KEY_FMT ", %s",
                                              "JB : INCOMING",
                                              a_message.c_str()
            );
            
            // ... parse received data ...
            if ( false == json_reader_.parse(a_message, job_status_) ) {
                // ... an error occurred ...
                const auto errors = json_reader_.getStructuredErrors();
                if ( errors.size() > 0 ) {
                    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_,
                                                                ( "An error occurred while parsing JSON subscription message: " +  errors[0].message + "!" ).c_str()
                                                                );
                } else {
                    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "An error occurred while parsing JSON subscription message!");
                }
            }
            
            // ... if an error is set ...
            if ( NGX_HTTP_INTERNAL_SERVER_ERROR == ctx_.response_.status_code_ ) {
                // ... stop accepting messages from this job ...
                ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
                // ... we must finalize request with errors serialization ...
                return [this] () {
                    // ... and we're done ...
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
                };
            }
            
            //
            // 'progress', 'redirect' or 'response' are the expected actions
            //
            
            const Json::Value action = job_status_.get("action", "progress");
            
            if ( 0 == strcasecmp(action.asCString(), "progress") ) {
                
                // ... 'progress' action ...
                
                // ... we're done, by ignoring it ...
                return nullptr;
                
            } else if ( 0 == strcasecmp(action.asCString(), "redirect") ) {
                
                // ... 'redirect' action ...
                
                //              {
                //                  "file": "",
                //                  "host": "",
                //                  "port": 0,
                //                  "protocol": ""
                //              }
                if ( NGX_OK != HandleRedirect(job_status_.get("redirect", Json::Value::null)) ) {
                    
                    // ... errors already set, fall-through ...
                    
                } else {
                    
                    // ... stop accepting messages from this job ...
                    ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);

                    // ... response codes already set ...
                    
                    // ... we're done ...
                    return [this] () {
                        // ... by finalizing request with an internal redirect ...
                        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(module_ptr_);
                    };
                    
                }
                
            } else if ( 0 == strcasecmp(action.asCString(), "response") ) {
                
                // ... stop accepting messages from this job ...
                ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);

                // ... 'response' action ...
                
                //              {
                //                  "content_type: "",
                //                  "action" : "response",
                //                  "response": null,
                //                  "status_code" : 200
                //              }
                
                // ... good to go ...
                ctx_.response_.status_code_ = static_cast<uint16_t>(job_status_.get("status_code",  NGX_HTTP_OK).asUInt());
                
                // ... we're done ...
                return [this] () {
                    // ... by finalizing request with an 'text' response ....
                    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_RESPONSE(module_ptr_, ctx_.response_.status_code_, job_status_["content_type"].asString(), json_writer_.write(job_status_["response"]));
                };
                
            } else {
                // ... this action is NOT implemented ...
                ctx_.response_.status_code_ = NGX_HTTP_NOT_IMPLEMENTED;
                // ... no other action is expected here! ...
                NGX_BROKER_MODULE_SET_ACTION_NOT_IMPLEMENTED(ctx_, action.asCString());
            }
            
        }
        
    } catch (const Json::Exception& a_json_exception) {
        // ... track exception and fall-through ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_json_exception);
    } catch (const ngx::casper::broker::Exception& a_broker_exception) {
        // ... track exception and fall-through ...
        NGX_BROKER_MODULE_SET_INTERNAL_SERVER_EXCEPTION(ctx_, a_broker_exception);
    }
    
    // ... stop accepting messages from this job ...
    ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);

    // ... if we've reached here ( at least on error should be already set ) ...
    return [this] () {
        // ... and we're done by finalizing request with errors serialization response ...
        NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
    };
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief This method will be called when REDIS connection was lost.
 */
void ngx::casper::broker::ext::Job::OnREDISConnectionLost ()
{
    NGX_BROKER_MODULE_SET_INTERNAL_SERVER_ERROR(ctx_, "Unable to connect to REDIS!");
    NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_ERRORS_SERIALIZATION_RESPONSE(module_ptr_);
}
