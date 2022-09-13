/**
 * @file object.cc
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

#include "ngx/casper/broker/cdn-common/db/object.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"

/**
 * @brief Default constructor
 *
 * param a_loggable_data_ref
 * @param a_schema Schema name.
 * @param a_table Table name.
 */
ngx::casper::broker::cdn::common::db::Object::Object (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                       const std::string& a_schema, const std::string& a_table)
 : loggable_data_ref_(a_loggable_data_ref), settings_(nullptr), schema_(a_schema), table_(a_table)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Default constructor
 *
 * param a_loggable_data_ref
 * @param a_settings See \link Settings \link.
 * @param a_schema Schema name.
 * @param a_table Table name.
 */
ngx::casper::broker::cdn::common::db::Object::Object (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                      const ngx::casper::broker::cdn::common::db::Object::Settings* a_settings,
                                                       const std::string& a_schema, const std::string& a_table)
 : loggable_data_ref_(a_loggable_data_ref), settings_(a_settings), schema_(a_schema), table_(a_table)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}


/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::db::Object::~Object ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

/**
 * @brief Execute a query asynchronously.
 *
 * @param a_ss        Query to execute.
 * @param a_callbacks Functions to call as soon as this async request returns.
 */
void ngx::casper::broker::cdn::common::db::Object::AsyncQuery (const std::stringstream& a_ss,
                                                               ngx::casper::broker::cdn::common::db::Object::Callbacks a_callbacks)
{
    
    const std::string query = a_ss.str();
    
    NewTask([this, query] () -> ::ev::Object* {
        
        return new ::ev::postgresql::Request(loggable_data_ref_, query);

    })->Finally([query, a_callbacks] (::ev::Object* a_object) {
        
        ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
        if ( nullptr == result ) {
            throw ::ev::Exception("Unexpected PostgreSQL result object: nullptr!");
        }
        
        const ::ev::postgresql::Reply* reply = dynamic_cast<const ::ev::postgresql::Reply*>(result->DataObject());
        if ( nullptr == reply ) {
            const ::ev::postgresql::Error* error = dynamic_cast<const ::ev::postgresql::Error*>(result->DataObject());
            if ( nullptr != error ) {
                a_callbacks.error_(/* a_status */ 500, *error);
            } else {
                throw ::ev::Exception("Unexpected PostgreSQL data object!");
            }
        } else {
            if ( reply->value().is_error() ) {
                a_callbacks.success_(/* a_status */ 500, /* a_value */ reply->value());
            } else {
                const uint16_t status = (
                                          ExecStatusType::PGRES_COMMAND_OK == reply->value().status()
                                            ? 204 // ... a query command that doesn't return anything was executed properly by the backend ...
                                            :
                                          ExecStatusType::PGRES_TUPLES_OK == reply->value().status()
                                            ? 200 // ... a query command that returns tuples was executed properly by the backend, PGresult contains the result tuples ...
                                            : 500 // .. error ...
                );
                // ... query performed, result must be evaluated by this request owner ...
                a_callbacks.success_(/* a_status */ status, /* a_value */ reply->value());
            }
        }
        
    })->Catch([query, a_callbacks] (const ::ev::Exception& a_ev_exception) {
        a_callbacks.failure_(/* a_status */ 500, /* a_data */ a_ev_exception);
    });
}

/**
 * @brief Execute a query asynchronously and serialize result to a JSON object.
 *
 * @param a_ss        Query to execute.
 * @param a_callbacks Functions to call as soon as this async request returns.
 */
void ngx::casper::broker::cdn::common::db::Object::AsyncQueryAndSerializeToJSON (const std::stringstream& a_ss,
                                                                                 ngx::casper::broker::cdn::common::db::Object::JSONCallbacks a_callbacks)
{
    AsyncQuery(a_ss,
        /* a_callbacks */
        {
            /* success_ */
            [a_callbacks] (const uint16_t /* a_status */, const ::ev::postgresql::Value& a_value) {
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
                            const char* const value = a_value.raw_value(/* a_row */ 0, /* a_column */ column);
                            if ( nullptr == value || 0 == strlen(value) ) {
                                record[a_value.column_name(column)] = Json::Value::null;
                            } else if ( false == reader.parse(value, record[a_value.column_name(column)]) ) {
                                const auto errors = reader.getStructuredErrors();
                                if ( errors.size() > 0 ) {
                                    throw ::cc::Exception("Sideline column value is not a valid JSON!: " +  errors[0].message + "!" );
                                } else {
                                    throw ::cc::Exception("Sideline column value is not a valid JSON!");
                                }
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

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ::ngx::casper::broker::cdn::common::db::Object::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}
