/**
 * @file synchronization.h
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
#ifndef NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_DB_SYNCHRONIZATION_H_
#define NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_DB_SYNCHRONIZATION_H_

#include "ngx/casper/broker/cdn-common/db/object.h"

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <vector>
#include <iostream> // std::ostream

#include "cc/fs/file.h" // Writer

#include "json/json.h"

namespace ngx
{
    
    namespace casper
    {
        
        namespace broker
        {
            
            namespace cdn
            {
                
                namespace archive
                {
                    
                    namespace db
                    {
                        
                        class Synchronization final : public ::cc::NonCopyable, public ::cc::NonMovable, private common::db::Object
                        {
                            
                        public: // Enum(s)
                            
                            enum class Operation : uint8_t {
                                NotSet = 0x00,
                                Create,
                                Update,
                                Patch,
                                Delete,
                                Move
                            };
                            
                            enum class Status : uint8_t {
                                NotSet = 0x00,
                                Pending,
                                Scheduled,
                                InProgress,
                                Done
                            };
                            
                            friend std::ostream& operator << (std::ostream& o_stream, const Operation& a_operation);
                            friend std::ostream& operator << (std::ostream& o_stream, const Status& a_status);
                            
                        public: // Data Type(s)
                            
                            typedef struct {
                                std::string tube_;
                                uint64_t    ttr_;
                                uint64_t    validity_;
                                Json::Value slaves_;
                            } Settings;
                            
                            typedef struct {
                                uint64_t    id_;
                                std::string type_;
                                bool        unaccounted_;
                            } Billing;
                            
                            typedef struct {
                                std::string id_;
                                std::string uri_;
                                uint64_t    size_;
                            } File;
                            
                            typedef struct {
                                File                               old_;
                                File                               new_;
                                std::map<std::string, std::string> headers_;
                                std::map<std::string, std::string> xattrs_;
                                Billing                            billing_;
                            } Data;
                            
                        typedef struct {
                            std::function<void(const Operation, const uint16_t, const Json::Value&)>     success_;
                            std::function<void(const Operation, const uint16_t, const ::ev::Exception&)> failure_;
                        } Callbacks;
                            
                        private: // Const Refs
                            
                            const Settings& settings_;
                            
                        private: // Ptrs to functions.
                            
                            const Callbacks callbacks_;
                            
                        public: // Construtor(s) / Destructor
                            
                            Synchronization () = delete;
                            Synchronization (const ::ev::Loggable::Data& a_loggable_data_ref,
                                      const Settings& a_settings,
                                      const Callbacks a_callbacks,
                                      const std::string& a_schema = "fs", const std::string& a_table = "synchronization");
                            virtual ~Synchronization ();
                        
                        public: // Method(s) / Function(s)

                            void Register (const Operation& operation, const Data& a_data);
                            
                        }; // end of class 'Registry'
                        
                        /**
                         * @brief Append a \link Synchronization::Operation \link value ( string representation ) to a stream.
                         *
                         * @param a_operation One of \link Synchronization::Operation \link.
                         */
                        inline std::ostream& operator << (std::ostream& o_stream, const Synchronization::Operation& a_operation)
                        {
                            switch (a_operation) {
                                case Synchronization::Operation::Create: o_stream << "Create"; break;
                                case Synchronization::Operation::Update: o_stream << "Update"; break;
                                case Synchronization::Operation::Patch : o_stream << "Patch" ; break;
                                case Synchronization::Operation::Delete: o_stream << "Delete"; break;
                                case Synchronization::Operation::Move  : o_stream << "Move"  ; break;
                                default                                : o_stream.setstate(std::ios_base::failbit); break;
                            }
                            return o_stream;
                        }
                        
                        /**
                         * @brief Append a \link Synchronization::Status \link value ( string representation ) to a stream.
                         *
                         * @param a_operation One of \link Synchronization::Status \link.
                         */
                        inline std::ostream& operator << (std::ostream& o_stream, const Synchronization::Status& a_status)
                        {
                            switch (a_status) {
                                case Synchronization::Status::Pending    : o_stream << "Pending"    ; break;
                                case Synchronization::Status::Scheduled  : o_stream << "Scheduled"  ; break;
                                case Synchronization::Status::InProgress : o_stream << "InProgress" ; break;
                                case Synchronization::Status::Done       : o_stream << "Done"       ; break;
                                default                                  : o_stream.setstate(std::ios_base::failbit); break;
                            }
                            return o_stream;
                        }
                        
                    } // end of namespace 'db'

                } // end of namespace 'archive'
                
            } // end of namespace 'cdn'
            
        } // end of namespace 'broker'
        
    } // end of namespace 'casper'
    
} // end of namespace 'ngx'

#endif // NRS_NGX_CASPER_BROKER_CDN_ARCHIVE_DB_SYNCHRONIZATION_H_
