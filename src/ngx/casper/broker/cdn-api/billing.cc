/**
 * @file billing.h
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

#include "ngx/casper/broker/cdn-api/billing.h"

#include "ev/ngx/includes.h"

#include "ngx/casper/broker/cdn-common/exception.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref Loggable data reference.
 * @param a_callbacks         Functions to call when asynchronous request is completed.
 */
ngx::casper::broker::cdn::api::Billing::Billing (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                 ngx::casper::broker::cdn::api::Billing::Callbacks a_callbacks)
    : ngx::casper::broker::cdn::api::Route(a_loggable_data_ref),
      db_(loggable_data_ref_)
{
    wrapped_callbacks_.success_ = nullptr;
    wrapped_callbacks_.failure_ = nullptr;
    owner_callbacks_.success_   = a_callbacks.success_;
    owner_callbacks_.failure_   = a_callbacks.failure_;
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::api::Billing::~Billing ()
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
ngx_int_t ngx::casper::broker::cdn::api::Billing::Process (const uint64_t& a_id,
                                                           const ngx_uint_t& a_method, const std::map<std::string, std::string>& /* a_headers */,
                                                           const std::map<std::string, std::set<std::string>>& a_params,
                                                           const char* const a_body)
{
    try {
        // ... check if body is required ...
        if ( NGX_HTTP_GET != a_method ) {
            // ... it is, and if it's not valid ...
            if ( nullptr == a_body || 0 == strlen(a_body) ) {
                throw cdn::BadRequest("Missing or invalid body!");
            }
            // ... else parse body ...
            Json::Reader reader;
            if ( false == reader.parse(a_body, tmp_value_) ) {
                const auto errors = reader.getStructuredErrors();
                if ( errors.size() > 0 ) {
                    throw cdn::BadRequest("Invalid body: %s!", errors[0].message.c_str());
                } else {
                    throw cdn::BadRequest("Invalid body!");
                }
            }
        }

        // ... setup billing request ...
        db_.SetIncludeDetails(a_params.end() != a_params.find("include"));
        
        const uint64_t id = ( NGX_HTTP_POST == a_method ? tmp_value_["data"]["id"].asUInt64() : a_id );
        
        // ... intercept success callback ...
        wrapped_callbacks_.success_ = [this, a_method, id, a_params](const uint16_t a_status, const Json::Value& a_value) {
            if ( 200 != a_status ) {
                owner_callbacks_.success_(a_status, a_value);
            } else {
                if ( NGX_HTTP_GET == a_method || NGX_HTTP_PATCH == a_method ) {
                    owner_callbacks_.success_(a_status, TranslateTableToJSONAPI(a_method, id, a_params, a_value));
                } else /* if ( NGX_HTTP_POST == a_method )  */ {
                    owner_callbacks_.success_(204, Json::Value::null);
                }
            }
        };
        wrapped_callbacks_.failure_ = owner_callbacks_.failure_;
        
        // ... perform HTTP request ...
        switch (a_method) {
            case NGX_HTTP_GET:
                db_.Get(id, wrapped_callbacks_);
                break;
            case NGX_HTTP_POST:
                db_.Create(id, tmp_value_, wrapped_callbacks_);
                break;
            case NGX_HTTP_PATCH:
                db_.Update(id, tmp_value_, wrapped_callbacks_);
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
const Json::Value& ngx::casper::broker::cdn::api::Billing::TranslateTableToJSONAPI (const ngx_uint_t& a_method, const uint64_t& a_id, const std::map<std::string, std::set<std::string>>& a_params,
                                                                                    const Json::Value& a_value)
{
    // ... translate JSON to JSONAPI ...
    tmp_value_                       = Json::Value(Json::ValueType::objectValue);
    tmp_value_["data"]               = Json::Value(Json::ValueType::objectValue);
    tmp_value_["data"]["type"]       = "billing";
    tmp_value_["data"]["id"]         = a_id;
    tmp_value_["data"]["attributes"] = Json::Value(Json::ValueType::objectValue);

    // ... if it's NOT a get?
    if ( NGX_HTTP_GET != a_method ) {
        // ... copy 'accounted_space_limit' attribute only ...
        tmp_value_["data"]["attributes"]["accounted_space_limit"] = a_value["accounted_space_limit"];
        // ... and we're done ...
        return tmp_value_;
    }

    // ... GET - add all info about this resource ....
    for ( auto member : a_value.getMemberNames() ) {
        tmp_value_["data"]["attributes"][member] = a_value[member];
    }
    tmp_value_["data"]["relationships"] = Json::Value(Json::ValueType::objectValue);
    
    const Json::Value unaccounted_types = tmp_value_["data"]["attributes"].removeMember("unaccounted_types");
    const Json::Value accounted_types   = tmp_value_["data"]["attributes"].removeMember("accounted_types");
    const Json::Value details           = tmp_value_["data"]["attributes"].removeMember("details");

    std::map<std::string, Json::ArrayIndex> unaccounted_types_map;
    Json::Value                             unaccounted_items = Json::Value(Json::ValueType::arrayValue);
    
    //
    // ... relationships must be always included ...
    //
    // 1 ) 'unaccounted' ...
    //
    if ( false == unaccounted_types.isNull() && unaccounted_types.size() > 0 ) {
        for ( Json::ArrayIndex idx = 0 ; idx < unaccounted_types.size() ; ++idx ) {
            unaccounted_types_map[unaccounted_types[idx].asString()] = idx;
            Json::Value& item = unaccounted_items.append(Json::Value(Json::ValueType::objectValue));
            item["type"]      = "unaccounted";
            item["id"]        = unaccounted_types[idx];
        }
        tmp_value_["data"]["relationships"]["unaccounted"]["data"] = unaccounted_items;
    }
    
    //
    // 2 ) 'accounted' ...
    //
    Json::Value accounted_items = Json::Value(Json::ValueType::arrayValue);
    if ( false == accounted_types.isNull() && accounted_types.size() > 0 ) {
        for ( Json::ArrayIndex idx = 0 ; idx < accounted_types.size() ; ++idx ) {
            Json::Value& item = accounted_items.append(Json::Value(Json::ValueType::objectValue));
            item["type"]      = "accounted";
            item["id"]        = accounted_types[idx];
        }
        tmp_value_["data"]["relationships"]["accounted"]["data"] = accounted_items;
    }
    
    const auto include_param_pair = a_params.find("include");
    const auto includes           = a_params.end() != include_param_pair ? &include_param_pair->second : nullptr;
    if ( nullptr == includes ) {
        return tmp_value_;
    }

    // ... NOT including relationships objects or no relationships to include?
    if ( nullptr == includes || 0 == tmp_value_["data"]["relationships"].getMemberNames().size() ) {
        // ... we're done ...
        return tmp_value_;
    }

    //
    // ... included ...
    //
    const bool include_unaccounted = ( includes->end() != includes->find("unaccounted") );
    const bool include_accounted   = ( includes->end() != includes->find("accounted") );
    if ( false == include_accounted && false == include_unaccounted ) {
        // ... nothing to include, we're done ...
        return tmp_value_;
    }

    // ... if it reached here, there is at least one 'relationships' present and one 'included' to be created ...
    tmp_value_["included"] = Json::Value(Json::ValueType::arrayValue);

    // ... unaccounted types ...
    if ( true == include_unaccounted && false == unaccounted_types.isNull() && unaccounted_types.size() > 0 ) {
        // ... zero fill - types are defined but details might not be present ...
        for ( Json::ArrayIndex idx = 0 ; idx < unaccounted_types.size() ; ++idx ) {
            Json::Value& item = tmp_value_["included"].append(Json::Value(Json::ValueType::objectValue));
            item["type"]                             = "unaccounted";
            item["id"]                               = unaccounted_types[idx].asString();
            item["attributes"]["used_space"]         = 0;
            item["attributes"]["number_of_archives"] = 0;
        }
        // ... fill with real data - if available ...
        if ( false == details.isNull() && details.size() > 0 ) {
            for ( Json::ArrayIndex idx = 0 ; idx < details.size() ; ++idx ) {
                const Json::Value& detail = details[idx];
                const auto it = unaccounted_types_map.find(detail["billing_type"].asString());
                if ( unaccounted_types_map.end() == it ) {
                    continue;
                }
                Json::Value& item = tmp_value_["included"][idx];
                item["attributes"]["used_space"]         = detail["used_space"];
                item["attributes"]["number_of_archives"] = detail["number_of_archives"];
            }
        }
    }
    
    // ... accounted types ...
    if ( true == include_accounted && false == details.isNull() && details.size() > 0 ) {
        // ... fill with real data - if any
        for ( Json::ArrayIndex idx = 0 ; idx < details.size() ; ++idx ) {
            const Json::Value& detail = details[idx];
            const auto it = unaccounted_types_map.find(detail["billing_type"].asString());
            if ( unaccounted_types_map.end() != it ) {
                continue;
            }
            Json::Value& item = tmp_value_["included"].append(Json::Value(Json::ValueType::objectValue));
            item["type"]                             = "accounted";
            item["id"]                               = detail["billing_type"];
            item["attributes"]["used_space"]         = detail["used_space"];
            item["attributes"]["number_of_archives"] = detail["number_of_archives"];
        }
    }

    // ... we're done ...
    return tmp_value_;
}
