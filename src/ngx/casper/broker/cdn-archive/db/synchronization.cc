/**
 * @file synchronization.cc
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

#include "ngx/casper/broker/cdn-archive/db/synchronization.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

#include "cc/utc_time.h"

#include <sstream>
#include <iomanip>   // std::setw, std::setfill
#include <algorithm> // std::remove

/**
 * @brief Default constructor.
 *
 * param a_loggable_data_ref
 * @param a_settings Writer settings.
 * @param a_callback Set of callbacks to report status.
 * @param a_schema Schema name.
 * @param a_table Table name.
 */
ngx::casper::broker::cdn::archive::db::Synchronization::Synchronization (const ev::Loggable::Data& a_loggable_data_ref,
                                                                         const ngx::casper::broker::cdn::archive::db::Synchronization::Settings& a_settings,
                                                                         const Callbacks a_callbacks,
                                                                         const std::string& a_schema, const std::string& a_table)
: ngx::casper::broker::cdn::common::db::Object(a_loggable_data_ref, a_schema, a_table),
  settings_(a_settings), callbacks_(a_callbacks)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::archive::db::Synchronization::~Synchronization ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Asynchronously, execute a operation into database table.
 *
 * @param a_operation One of \link Operation \link.
 * @param a_data Operation data.
 */
void ngx::casper::broker::cdn::archive::db::Synchronization::Register (const ngx::casper::broker::cdn::archive::db::Synchronization::Operation& a_operation,
                                                                       const ngx::casper::broker::cdn::archive::db::Synchronization::Data& a_data)
{
    //
    // ... set payload ...
    //
    
    Json::FastWriter json_writer;

    json_writer.omitEndingLineFeed();

    Json::Value payload                    = Json::Value(Json::ValueType::objectValue);
    payload["archive"]                     = Json::Value(Json::ValueType::objectValue);
    payload["archive"]["id"]               = a_data.new_.id_;
    payload["archive"]["headers"]          = Json::Value(Json::ValueType::objectValue);
    payload["archive"]["xattrs"]           = Json::Value(Json::ValueType::objectValue);
    payload["archive"]["origin"]           = Json::Value(Json::ValueType::objectValue);
    payload["archive"]["origin"]["ip"]     = loggable_data_ref_.ip_addr();
    payload["archive"]["origin"]["module"] = loggable_data_ref_.module();
    
    Json::Value& headers = payload["archive"]["headers"];
    for ( auto it : a_data.headers_ ) {
        headers[it.first] = it.second;
    }
    
    Json::Value& xattrs = payload["archive"]["xattrs"];
    for ( auto it : a_data.xattrs_ ) {
        xattrs[it.first] = it.second;
    }
    
    std::stringstream ss;
    
    ss << "" << a_operation << "";
    
    const std::string operation_str = ss.str();
    
    Json::Value& tracking = payload["archive"]["tracking"];
    
    if ( not ( ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Delete == a_operation ) ) {
        tracking["new"]         = Json::Value(Json::ValueType::objectValue);
        tracking["new"]["id"]   = a_data.new_.id_;
        tracking["new"]["size"] = a_data.new_.size_;
    }
    
    if ( not ( ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Create == a_operation ) ) {
        tracking["old"]         = Json::Value(Json::ValueType::objectValue);
        tracking["old"]["id"]   = a_data.old_.id_;
        tracking["old"]["size"] = a_data.old_.size_;
    }

    switch(a_operation) {
        case ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Create:
            tracking["delta"] = tracking["new"]["size"];
            break;
        case ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Update:
        case ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Patch:
            tracking["delta"] = static_cast<int64_t>(tracking["new"]["size"].asUInt64()) - static_cast<int64_t>(tracking["old"]["size"].asUInt64());
            break;
        case ngx::casper::broker::cdn::archive::db::Synchronization::Operation::Delete:
            tracking["delta"] = -1 * static_cast<int64_t>(tracking["old"]["size"].asUInt64());
            break;
        default:
            throw ::cc::Exception("Don't know how to calculate delta for operation '%s'!", operation_str.c_str());
    }
    
    tracking["unaccounted"] = a_data.billing_.unaccounted_;
            
    Json::Value slaves = Json::Value(settings_.slaves_);
    for ( Json::ArrayIndex idx = 0 ; idx < slaves.size() ; ++idx ) {
        slaves[idx]["status"] = "Pending";
    }
    
    //
    // ... set query ...
    //
    ss.clear(); ss.str("");
    ss << "INSERT INTO " << schema_ << '.' << table_ << "(operation,payload,slaves,tts,status,billing_id,billing_type)";
    ss << " VALUES (";
    ss <<     "'" << operation_str << "'";
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(json_writer.write(payload)) << "'";
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(json_writer.write(slaves))   << "'";
    ss <<     ","  << "(NOW() AT TIME ZONE 'UTC') + ( '" << ( settings_.ttr_ + settings_.validity_ ) << "' || 'second' )::interval";
    ss <<     ",'" << Synchronization::Status::Pending << "'";
    ss <<     "," << a_data.billing_.id_;
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(a_data.billing_.type_) << "'";
    ss << " ) RETURNING id;";
    
    //
    // ... execute query ...
    //
    AsyncQuery(ss,
               {
                   /* success_ */
                   [this, a_operation] (const uint16_t a_status, const ::ev::postgresql::Value& a_value) {
                       if ( 200 == a_status ) {
			 const Json::Value id = static_cast<Json::UInt64>(std::stoull(a_value.raw_value(/* a_row */ 0, /* a_column */ 0)));
			 callbacks_.success_(a_operation, a_status, id);
                       } else {
                           callbacks_.success_(a_operation, a_status, Json::Value::null);
                       }
                   },
                   /* error_ */
                   [this, a_operation] (const uint16_t a_status, const ::ev::postgresql::Error& a_error) {
                       callbacks_.failure_(a_operation, a_status, ::ev::Exception("%s", a_error.message().c_str()));
                   },
                   /* failure_ */
                   [this, a_operation] (const uint16_t a_status, const ::ev::Exception& a_ev_exception) {
                       callbacks_.failure_(a_operation, a_status, a_ev_exception);
                   }
               }
    );
}
