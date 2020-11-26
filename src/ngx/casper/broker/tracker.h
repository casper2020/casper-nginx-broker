/**
 * @file tracker.h
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

#pragma once
#ifndef NRS_NGX_CASPER_BROKER_TRACKER_H_
#define NRS_NGX_CASPER_BROKER_TRACKER_H_

#include <string> // std::string
#include <map>    // std::map

#include "ev/exception.h"

#include "ev/ngx/includes.h"

#include "osal/osal_singleton.h"

#include "ngx/casper/broker/errors.h"
#include "ngx/casper/broker/exception.h"

#include "unicode/fmtable.h" // U_ICU_NAMESPACE::Formattable

namespace ngx
{

    namespace casper
    {

        namespace broker
        {

            // ---- //
            class Tracker;
            class TrackerInitializer final : public ::osal::Initializer<Tracker>
            {
                
            public: // Constructor(s) / Destructor
                
                TrackerInitializer (Tracker& a_instance)
                    : ::osal::Initializer<Tracker>(a_instance)
                {
                    /* empty */
                }
                virtual ~TrackerInitializer ()
                {
                    /* empty */
                }
                
            }; // end of class 'TrackerInitializer'
            
            // ---- //
            class Tracker final : public osal::Singleton<Tracker, TrackerInitializer>
            {
                
            public: // Data Type(s)
                
                typedef std::function<void(const std::string& /* a_what */, const std::string& /* a_message */)> Logger;
            
            private: // Data Type(s)
                
                typedef struct {
                    Logger  logger_;
                    Errors* errors_;
                } Entry;

            private: // Data
                
                std::map<ngx_http_request_t*, Entry*> map_;
                
            public: // One-shot Call Method(s) / Function(s)
                
                void Startup  ();
                void Shutdown ();
                
            public: // Method(s) / Function(s)
                
                void              Register       (ngx_http_request_t* a_r, Errors* a_errors);
                void              Bind           (ngx_http_request_t* a_r, Logger a_logger);
                void              Unregister     (ngx_http_request_t* a_r);
                bool              IsRegistered   (ngx_http_request_t* a_r) const;
                Errors*           errors_ptr     (ngx_http_request_t* a_r);
                void              Log            (ngx_http_request_t* a_r, const std::string& a_what, const std::string& a_message) const;
                size_t            Count          () const;
                size_t            Count          (ngx_http_request_t* a_r) const;
                bool              ContainsErrors (ngx_http_request_t* a_r) const;
                
            }; // end of class 'Tracker'

            /**
             * @brief One-shot initializer.
             */
            inline void Tracker::Startup ()
            {
                /* empty */
            }
            
            /**
             * @brief Dealloc previously allocated memory ( if any ).
             */
            inline void Tracker::Shutdown ()
            {
                for ( auto it : map_ ) {
                    delete it.second->errors_;
                    delete it.second;
                }
                map_.clear();
            }
            
            /**
             * @brief Keep track of a specific NGX HTTP request.
             *
             * @param a_r
             * @param a_errors
             */
            inline void Tracker::Register (ngx_http_request_t* a_r, ngx::casper::broker::Errors* a_errors)
            {
                const auto it = map_.find(a_r);
                if ( map_.end() != it ) {
                    return;
                }
                map_[a_r] = new Tracker::Entry({ /* a_logger */ nullptr, a_errors });
            }
        
           /**
            * @brief Set a previously tracked NGX HTTP request tracker logger..
            *
            * @param a_r
            * @param a_logger
            */
            inline void Tracker::Bind (ngx_http_request_t* a_r, Logger a_logger)
            {
                const auto it = map_.find(a_r);
                if ( map_.end() == it ) {
                    return;
                }
                it->second->logger_ = a_logger;
            }
            
            /**
             * @brief Unregister a specific NGX HTTP request.
             *
             * @param a_r
             */
            inline void Tracker::Unregister (ngx_http_request_t* a_r)
            {
                const auto it = map_.find(a_r);
                if ( map_.end() == it ) {
                    return;
                }
                delete it->second->errors_;
                delete it->second;
                map_.erase(it);
            }
            
            /**
             * @brief Check if a specific NGX HTTP request is registered.
             *
             * @param a_r
             *
             * @return True if so, false otherwise.
             */
            inline bool Tracker::IsRegistered (ngx_http_request_t* a_r) const
            {
                return map_.end() != map_.find(a_r);
            }

            /**
             * @return The number of tracked requests.
             */
            inline size_t Tracker::Count () const
            {
                return map_.size();
            }
            
            /**
             * @return The number of tracked errors for a specific requests.
             *
             * @param a_r
             */
            inline size_t Tracker::Count (ngx_http_request_t* a_r) const
            {
                const auto it = map_.find(a_r);
                if ( map_.end() == it ) {
                    return 0;
                }
                return static_cast<size_t>(it->second->errors_->Count());
            }
            
            /**
             * @return True if there are errors set.
             *
             * @param a_r
             */
            inline bool Tracker::ContainsErrors (ngx_http_request_t* a_r) const
            {
                const auto it = map_.find(a_r);
                if ( map_.end() == it ) {
                    return false;
                }                
                return it->second->errors_->Count() > 0;
            }

            /**
             * @brief Log a message for a previously tracked NGX HTTP request..
             *
             * @param a_r
             * @param a_what
             * @param a_message
             */
             inline void Tracker::Log (ngx_http_request_t* a_r, const std::string& a_what, const std::string& a_message) const
             {
                 const auto it = map_.find(a_r);
                 if ( map_.end() == it || nullptr == it->second->logger_ ) {
                     return;
                 }
                 it->second->logger_(a_what, a_message);
             }

            /**
             * @return RW access to errors for a specific request.
             *
             * @param a_r
             */
            inline Errors* Tracker::errors_ptr (ngx_http_request_t* a_r)
            {
                const auto it = map_.find(a_r);
                if ( map_.end() == it ) {
                    return nullptr;
                }
                return it->second->errors_;
            }

        } // end of namespace 'broker'
        
    } // end of namespace 'casper'

}  // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_TRACKER_H_
