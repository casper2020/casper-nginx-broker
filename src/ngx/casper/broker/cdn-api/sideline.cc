/**
 * @file sideline.h
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
#pragma mark - CONSTRUCTOR(S) / DESTRUCTOR
#endif

#include "ngx/casper/broker/cdn-api/sideline.h"

#include "ev/ngx/includes.h"

#include "ngx/casper/broker/cdn-common/exception.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref Loggable data reference.
 * @param a_settings See \link Settings \link.
 * @param a_data See \link Data \link.
 * @param a_callback Set of callbacks to call ( accordingly with state ) when async request is completed.
 */
ngx::casper::broker::cdn::api::Sideline::Sideline (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                   const ngx::casper::broker::cdn::api::Sideline::Settings& a_settings,
                                                   ngx::casper::broker::cdn::api::Sideline::Callbacks a_callbacks)
    : ngx::casper::broker::cdn::api::Route(a_loggable_data_ref),
      db_(loggable_data_ref_, a_settings),
      activity_data_{
          /* activity_     */ common::db::Sideline::Activity::NotSet,
          /* payload_      */ Json::Value::null,
          /* id_           */ ("X-NUMERIC-ID")
#ifdef __APPLE__
          ,
          /* billing_id_   */ 0,
          /* billing_type_ */ ""
#endif
      }
{
    wrapped_callbacks_.success_ = nullptr;
    wrapped_callbacks_.failure_ = nullptr;
    owner_callbacks_.success_   = a_callbacks.success_;
    owner_callbacks_.failure_   = a_callbacks.failure_;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::api::Sideline::~Sideline ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark - PUBLIC METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Process a request.
 *
 * @param a_id     Resource ID std::numeric_limits<uint64_t>::max() if not required.
 * @param a_method Request HTTP METHOD.
 * @param a_header HTTP headers.
 * @param a_params Addicional params.
 * @param a_body   Requet body ( if any )
 */
ngx_int_t ngx::casper::broker::cdn::api::Sideline::Process (const uint64_t& a_id,
                                                            const ngx_uint_t& a_method, const std::map<std::string, std::string>& a_headers,
                                                            const std::map<std::string, std::set<std::string>>& a_params,
                                                            const char* const a_body)
{
    try {
        
        const ActivityData& activitity_data = GetActivityData(a_id, a_method, a_headers, a_params, a_body);

        // ... intercept success callback ...
        wrapped_callbacks_.success_ = [this, a_method, a_id, a_params](const uint16_t a_status, const Json::Value& a_value) {
            if ( 200 != a_status ) {
                owner_callbacks_.success_(a_status, a_value);
            } else {
                if ( NGX_HTTP_POST == a_method ) {
                    owner_callbacks_.success_(a_status, TranslateTableToJSONAPI(a_method, a_id, a_params, a_value));
                } else /* if ( NGX_HTTP_POST == a_method )  */ {
                    owner_callbacks_.success_(204, Json::Value::null);
                }
            }
        };
        wrapped_callbacks_.failure_ = owner_callbacks_.failure_;
                
        const ngx::casper::broker::cdn::common::db::Sideline::Billing billing = {
            /* id_   */ (const uint64_t&)activitity_data.billing_id_,
            /* type_ */ (const std::string&)activitity_data.billing_type_
        };
                
        // ... perform HTTP request ...
        switch (a_method) {
            case NGX_HTTP_POST:
                db_.Register(activitity_data.activity_, billing, activitity_data.payload_, wrapped_callbacks_);
                break;
            default:
                throw cdn::NotImplemented();
        }
    } catch (const Json::Exception& a_json_exception) {
        throw cdn::BadRequest("Invalid body: %s!", a_json_exception.what());
    }
    // ... done ...
    return NGX_OK;
}

const ngx::casper::broker::cdn::api::Sideline::ActivityData& ngx::casper::broker::cdn::api::Sideline::GetActivityData (const uint64_t& /* a_id */,
                                                                                                                       const ngx_uint_t& a_method,
                                                                                                                       const std::map<std::string, std::string>& a_headers,
                                                                                                                       const std::map<std::string, std::set<std::string>>& /* a_params */,
                                                                                                                       const char* const a_body)
{
    // ... ensure ownership and billing info ...
    activity_data_.activity_ = common::db::Sideline::Activity::NotSet;
    activity_data_.id_.Set({"X-CASPER-ENTITY-ID", "X-CASPER-USER-ID"}, a_headers);
    activity_data_.billing_id_.Set(a_headers);
    activity_data_.billing_type_.Set(a_headers);
    activity_data_.payload_  = Json::Value(Json::ValueType::objectValue);
    
    Json::Value body = Json::Value::null;
               
    // ... check if body is required ...
    if ( NGX_HTTP_POST == a_method ) {
        // ... it is, and if it's not valid ...
        if ( nullptr == a_body || 0 == strlen(a_body) ) {
            throw cdn::BadRequest("Missing or invalid body!");
        }
        // ... else parse body ...
        Json::Reader reader;
        if ( false == reader.parse(a_body, body) ) {
            const auto errors = reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw cdn::BadRequest("Invalid body: %s!", errors[0].message.c_str());
            } else {
                throw cdn::BadRequest("Invalid body!");
            }
        }
    }

    //
    // FROM API CALL ( BODY )
    //
    //        {
    //           "data": {
    //               "type": "sideline",
    //               "attributes": {
    //                   "activity": "Copy",
    //                   "file": "logo_template.png",
    //                    "path_suffix": "public"
    //               }
    //           }
    //        }
    //
    //
    // TO JOB
    //
    //        {
    //            "X-CASPER-ENTITY|USER-ID": 0,
    //            "copy": {
    //                "file": "logo_template.png",
    //                "path_suffix": "<path_suffix>"
    //            }
    //        }
    
    // ... pick activity ...
    const std::string activity_str = body["data"]["attributes"].get("activity", "").asString();
    
    // ... for now 'copy' activity is the only one recognized ...
    if ( 0 == strcasecmp(activity_str.c_str(), "copy") ) {
        // ... copy file ...
        const std::string filename = body["data"]["attributes"].get("filename", "").asString();
        if ( 0 == filename.length() ) {
            throw ::cc::Exception("Invalid or missing 'filename' attribute: " + filename + "!");
        }
        activity_data_.activity_ = common::db::Sideline::Activity::Copy;
        activity_data_.payload_["copy"]["file"]        = filename;
        activity_data_.payload_["copy"]["path_suffix"] = body["data"]["attributes"]["path_suffix"];
        activity_data_.payload_[activity_data_.id_.Name()] = (const uint64_t&)activity_data_.id_;
    }
    
    // ... if
    if ( common::db::Sideline::Activity::Copy != activity_data_.activity_ ) {
        throw cdn::NotImplemented(("Don't know how to handle sideline activity '" + activity_str + "'!").c_str());
    }
    
    return activity_data_;
}

#ifdef __APPLE__
#pragma mark - PRIVATE METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Translate a 'JSON table' TO a JSONAPI object.
 *
 * @param a_method  Request HTTP METHOD.
 * @param a_id      Resource ID std::numeric_limits<uint64_t>::max() if not required.
 * @param a_params  Addicional params.
 * @param a_value  'JSON table'.
 */
const Json::Value& ngx::casper::broker::cdn::api::Sideline::TranslateTableToJSONAPI (const ngx_uint_t& /* a_method */, const uint64_t& /* a_id */, const std::map<std::string, std::set<std::string>>& /* a_params */,
                                                                                     const Json::Value& a_value)
{
    // ... translate JSON to JSONAPI ...
    tmp_value_                       = Json::Value(Json::ValueType::objectValue);
    tmp_value_["data"]               = Json::Value(Json::ValueType::objectValue);
    tmp_value_["data"]["type"]       = "sideline";
    tmp_value_["data"]["id"]         = a_value["id"];

    // ... GET - add all info about this resource ....
    tmp_value_["data"]["attributes"] = Json::Value(Json::ValueType::objectValue);
    for ( auto member : a_value.getMemberNames() ) {
        tmp_value_["data"]["attributes"][member] = a_value[member];
    }
    tmp_value_["data"]["attributes"].removeMember("id");

    // ... we're done ...
    return tmp_value_;
}

