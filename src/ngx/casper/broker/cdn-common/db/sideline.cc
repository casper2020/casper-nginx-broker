/**
 * @file sideline.cc
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

#include "ngx/casper/broker/cdn-common/db/sideline.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

/**
 * @brief Default constructor
 *
 * param a_loggable_data_ref
 * @param a_settings See \link db::Sideline::Settings \link.
 * @param a_schema Schema name.
 * @param a_table Table name.
 */
ngx::casper::broker::cdn::common::db::Sideline::Sideline (const ::ev::Loggable::Data& a_loggable_data_ref,
                                                          const ngx::casper::broker::cdn::common::db::Sideline::Settings& a_settings,
                                                          const std::string& a_schema, const std::string& a_table)
 : ngx::casper::broker::cdn::common::db::Object(a_loggable_data_ref, &a_settings, a_schema, a_table)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
ngx::casper::broker::cdn::common::db::Sideline::~Sideline ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

/**
 * @brief Create a sideline entry.
 *
 * @param a_activity One of \link Activity \link.
 * @param a_billing  Billing info.
 * @param a_payload  JSON object.
 * @param a_callback Functions to call accordantly to success of failure.
 */
void ngx::casper::broker::cdn::common::db::Sideline::Register (const ngx::casper::broker::cdn::common::db::Sideline::Activity& a_activity,
                                                               const ngx::casper::broker::cdn::common::db::Sideline::Billing& a_billing,
                                                               const Json::Value& a_payload,
                                                               ngx::casper::broker::cdn::common::db::Sideline::JSONCallbacks a_callbacks)
{
    std::stringstream ss;
    // ... ensure activity is set ...
    if ( ngx::casper::broker::cdn::common::db::Sideline::Activity::NotSet == a_activity ) {
        ss << a_activity;
        throw ::cc::Exception("Don't know how to handle sideline activity '" + ss.str() + "'!");
    }

    Json::FastWriter json_writer; json_writer.omitEndingLineFeed();
    
    Json::Value slaves = Json::Value(settings_->slaves_);
    for ( Json::ArrayIndex idx = 0 ; idx < slaves.size() ; ++idx ) {
        slaves[idx]["status"] = "Pending";
    }

    ss.clear(); ss.str("");
    ss << "INSERT INTO " << schema_ << '.' << table_ << "(activity,payload,slaves,tts,status,billing_id,billing_type)";
    ss << " VALUES (";
    ss <<     "'" << a_activity << "'";
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(json_writer.write(a_payload)) << "'";
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(json_writer.write(slaves))   << "'";
    ss <<     ","  << "(NOW() AT TIME ZONE 'UTC') + ( '" << ( settings_->ttr_ + settings_->validity_ ) << "' || 'second' )::interval";
    ss <<     ",'" << Sideline::Status::Pending << "'";
    ss <<     "," << a_billing.id_;
    ss <<     ",'" << ::ev::postgresql::Request::SQLEscape(a_billing.type_) << "'";
    ss << " ) RETURNING id, to_json(activity) AS activity, to_json(status) AS status;";

    AsyncQueryAndSerializeToJSON(ss, a_callbacks);
}

