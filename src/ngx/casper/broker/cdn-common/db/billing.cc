/**
 * @file billing.cc
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

#include "ngx/casper/broker/cdn-common/db/billing.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

/**
 * @brief Default constructor
 *
 * param a_loggable_data_ref
 * @param a_schema Schema name.
 * @param a_table Table name.
 */
ngx::casper::broker::cdn::common::db::Billing::Billing (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                        const std::string& a_schema, const std::string& a_table)
 : ngx::casper::broker::cdn::common::db::Object(a_loggable_data_ref, a_schema, a_table),
   include_details_(false)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::db::Billing::~Billing ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

/**
 * @brief Create or Update a billing entry.
 *
 * @param a_payload  JSON, expected {"data":{"type":"billing","attributes":{"accounted_space_limit":<uint64_t>}}}
 * @param a_callback Functions to call accordantly to success of failure.
 */
void ngx::casper::broker::cdn::common::db::Billing::Create (const uint64_t& a_id, const Json::Value& a_payload,
                                                            ngx::casper::broker::cdn::common::db::Billing::JSONCallbacks a_callbacks)
{
    Update(a_id, a_payload, a_callbacks);
}

/**
 * @brief Create or Update a billing entry.
 *
 * @param a_payload        JSON, expected {"data":{"type":"billing","attributes":{"accounted_space_limit":<uint64_t>}}}
 * @param a_callback       Functions to call accordantly to success of failure.
 */
void ngx::casper::broker::cdn::common::db::Billing::Update (const uint64_t& a_id, const Json::Value& a_payload,
                                                            ngx::casper::broker::cdn::common::db::Billing::JSONCallbacks a_callbacks)
{
    const uint64_t limit = static_cast<uint64_t>(a_payload["data"]["attributes"]["accounted_space_limit"].asUInt64());
    std::stringstream ss;
    ss << "INSERT INTO fs.billing(billing_id,accounted_space_limit) VALUES("
          << a_id << "," << limit << ")"
          << " ON CONFLICT (billing_id) WHERE billing_id=" << a_id << " DO UPDATE SET accounted_space_limit=" << limit
       << " RETURNING "
          << "fs.billing.unaccounted_number_of_archives,"
          << "fs.billing.unaccounted_used_space,"
          << "fs.billing.accounted_number_of_archives,"
          << "fs.billing.accounted_used_space, fs.billing.accounted_space_limit"
          << ",to_json(fs.billing.unaccounted_types) AS unaccounted_types"
          << ",to_json("
               << "ARRAY(SELECT e FROM unnest(ARRAY( SELECT DISTINCT(fs.billing_stats_details.billing_type) FROM fs.billing_stats_details WHERE billing_id =  fs.billing.billing_id)) e WHERE e <> ALL (fs.billing.unaccounted_types))"
          << ") AS accounted_types";
    if ( true == include_details_ ) {
       ss << ",( SELECT array_to_json(array_agg(row_to_json(t))) AS details "
          <<    "FROM ( select billing_type, number_of_archives, used_space FROM fs.billing_stats_details WHERE billing_id = fs.billing.billing_id ) AS t"
          << ")";
    }
    AsyncQueryAndSerializeToJSON(ss, a_callbacks);
}


/**
 * @brief Retrieve billing information.
 *
 * @param a_id       Billing id.
 * @param a_callback Functions to call accordantly to success of failure.
 */
void ngx::casper::broker::cdn::common::db::Billing::Get (const uint64_t& a_id,
                                                         ngx::casper::broker::cdn::common::db::Billing::JSONCallbacks a_callbacks)
{
    std::stringstream ss;
    ss << "SELECT b.unaccounted_number_of_archives, b.unaccounted_used_space, b.accounted_number_of_archives, b.accounted_used_space, b.accounted_space_limit"
       << ",to_json(b.unaccounted_types) AS unaccounted_types"
       << ",to_json("
              << "ARRAY(SELECT e FROM unnest(ARRAY( SELECT DISTINCT(fs.billing_stats_details.billing_type) FROM fs.billing_stats_details WHERE billing_id =  b.billing_id)) e WHERE e <> ALL (b.unaccounted_types))"
       << ") AS accounted_types";
    if ( true == include_details_ ) {
        ss <<  ",( SELECT array_to_json(array_agg(row_to_json(t))) AS details "
           <<  "  FROM ( select billing_type, number_of_archives, used_space FROM fs.billing_stats_details WHERE billing_id = b.billing_id ) AS t"
           <<  ")";
    }
    ss << " FROM fs.billing AS b WHERE b.billing_id=" << a_id << ";";
    AsyncQueryAndSerializeToJSON(ss, a_callbacks);
}

/**
 * @brief Retrieve billing information for a specific billing type.
 *
 * @param a_id       Billing id.
 * @param a_type     Billing type.
 * @param a_callback Functions to call accordantly to success of failure.
 */
void ngx::casper::broker::cdn::common::db::Billing::Get (const uint64_t& a_id, const std::string& a_type,
                                                        ngx::casper::broker::cdn::common::db::Billing::JSONCallbacks a_callbacks)
{
    std::stringstream ss;
    ss << "SELECT array_to_json(unaccounted_types) AS unaccounted_types, accounted_used_space, accounted_space_limit FROM "
         << schema_ << '.' << table_
       << " WHERE billing_id = " << a_id << ';'
    ;
    AsyncQuery(ss,
        /* a_callbacks */
        {
            /* success_ */
            [a_callbacks, a_type] (const uint16_t /* a_status */, const ::ev::postgresql::Value& a_value) {
                // ... for this specific requrest, we're expecting only one result ...
                if ( 0 == a_value.rows_count() ) {
                    // ... not found -..
                    a_callbacks.success_(/* a_status */ 404, Json::Value::null);
                } else if ( a_value.rows_count() > 1 ) {
                    // ... more than one ...
                    throw ::ev::Exception("Multiple results found, expecting only one!") ;
                } else {
                    // ... just one, serialize to json ...
                    Json::Value record = Json::Value(Json::ValueType::objectValue);
                    try {
                        Json::Reader reader;
                        for ( int column = 0 ; column < a_value.columns_count() ; ++column ) {
                            if ( false == reader.parse(a_value.raw_value(/* a_row */ 0, /* a_column */ column), record[a_value.column_name(column)]) ) {
                                const auto errors = reader.getStructuredErrors();
                                if ( errors.size() > 0 ) {
                                    throw ::cc::Exception("Billing column value is not a valid JSON!: " +  errors[0].message + "!" );
                                } else {
                                    throw ::cc::Exception("Billing column value is not a valid JSON!");
                                }
                            }
                        }
                        // ... check if is unaccounted type ...
                        const Json::Value& unaccounted_types = record["unaccounted_types"];
                        for ( Json::ArrayIndex idx = 0 ; idx < unaccounted_types.size() ; ++idx ) {
                            if ( 0 == strcmp(a_type.c_str(), unaccounted_types[idx].asCString()) ) {
                                record["unaccounted"] = true;
                                break;
                            }
                        }
                    } catch (const Json::Exception& a_json_exception) {
                        throw ::ev::Exception("%s", a_json_exception.what());
                    }
                    // ... deliver result ...
                    a_callbacks.success_(/* a_status */ 200, record);
                }
            },
            /* error_ */
            [a_callbacks] (const uint16_t a_status, const ::ev::postgresql::Error& a_error) {
                // ... notify error ...
                a_callbacks.failure_(a_status, ::ev::Exception("%s", a_error.message().c_str()));
            },
            /* failure_ */
            [a_callbacks] (const uint16_t a_status, const ::ev::Exception& a_ev_exception) {
                // ... notify exception ...
                a_callbacks.failure_(a_status, a_ev_exception);
            }
        }
    );
}

