/**
 * @file nb-xattr.cc
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
 * casper-nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-nginx-broker. If not, see <http://www.gnu.org/licenses/>.
 */

#include "xattr/version.h"
#include "xattr/archive.h"

#include <string.h> // strlen
#include <unistd.h> // getopt
#include <string>
#include <map>
#include <set>

#include "cc/exception.h"
#include "cc/fs/dir.h"

#include "json/json.h"

enum class XAttrAction : uint8_t {
    Set = 0x00,
    Get,
    Remove,
    List,
    Verify,
    Create,
    Update
};

typedef struct _KVArg {
    std::string name_;
    std::string value_;
    std::string expression_;
} KVArg;

/**
 * @brief Show version.
 *
 * @param a_name Tool name.
 */
void show_version (const char* a_name)
{
    fprintf(stderr, "%s\n", NRS_CASPER_NGINX_BROKER_XATTR_INFO);
}

/**
 * @brief Show help.
 *
 * @param a_name Tool name.
 */
void show_help (const char* a_name)
{
    fprintf(stderr, "Usage: %s -f <file> <action> [option, ...]\n", a_name);
    fprintf(stderr, " action:\n");
    fprintf(stderr, "       -%c: %s\n", 'c' , "create an empty file.");
    fprintf(stderr, "       -%c: %s\n", 's' , "set an extended attribute.");
    fprintf(stderr, "       -%c: %s\n", 'g' , "get an extended attribute.");
    fprintf(stderr, "       -%c: %s\n", 'r' , "remove an extended attribute.");
    fprintf(stderr, "       -%c: %s\n", 'l' , "list extended attributes.");
    fprintf(stderr, "       -%c: %s\n", 'z' , "verify file integrity.");
    fprintf(stderr, "       -%c: %s\n", 'u' , "update extended attributes related to file integrity check.");
    fprintf(stderr, "       -%c: %s\n", 'h' , "show help.");
    fprintf(stderr, " options:\n");
    fprintf(stderr, "       -%c: %s\n", 'n' , "extended attribute name ( for set, get or remove actions ) or human readable name ( for create action )");
    fprintf(stderr, "       -%c: %s\n", 'v' , "extended attribute value ( for set action )");
    fprintf(stderr, "       -%c: %s\n", 'i' , "archive id ( for create action )");
    fprintf(stderr, "       -%c: %s\n", 'H' , "simulate HTTP headers ( for create action )");
    fprintf(stderr, "       -%c: %s\n", 'd' , "base directory where files are stored ( for create action )");
    fprintf(stderr, "       -%c: %s\n", 'j' , "JSON format ( for list action )");
    fprintf(stderr, "       -%c: %s\n", 'e' , "regext ECMAScript ( for get, remove or list action )");
}

void serialize_response (const char* const a_title, const std::map<std::string, std::string>& a_map, const bool a_as_json)
{
    if ( true == a_as_json ) {
        Json::Value payload = Json::Value(Json::ValueType::objectValue);
        for ( auto it : a_map ) {
            payload[it.first] = it.second;
        }
        Json::StyledWriter writer;
        fprintf(stdout, "%s", writer.write(payload).c_str());
    } else if ( 0 == a_map.size() ) {
        fprintf(stdout, "No attributes set.\n");
    } else {
        size_t mkl = 0;
        for ( auto it : a_map ) {
            if ( it.first.length() > mkl ) {
                mkl = it.first.length();
            }
        }
        fprintf(stdout, "%s:\n", a_title);
        for ( auto it : a_map ) {
            fprintf(stdout, "\t%*.*s: %s\n", static_cast<int>(mkl), static_cast<int>(mkl), it.first.c_str(), it.second.c_str());
        }
    }
}

/**
 * @brief Parse arguments.
 *
 * param a_argc
 * param a_argv
 * param o_config_file_uri
 * param o_action
 * param o_open_args
 * param a_create_args
 * param o_validate_args
 * param o_update_args
 * param o_kv_arg
 *
 * @return 0 on success, < 0 on error.
 */
int parse_args (int a_argc, char** a_argv, XAttrAction& o_action,
                nb::Archive::OpenArgs& o_open_args, nb::Archive::CreateArgs& o_create_args, nb::Archive::ValidateArgs& o_validate_args, nb::Archive::UpdateArgs& o_update_args,
                KVArg& o_kv_arg)
{
   
    
#ifdef __APPLE__
    o_create_args.config_file_uri_ = "/usr/local/etc/nb-xattr/conf.json";
#else
    o_create_args.config_file_uri_ = "/etc/nb-xattr/conf.json";
#endif
    o_open_args.config_file_uri_ = o_create_args.config_file_uri_;
    
    if ( a_argc < 1 ) {
        // ... show error ...
        fprintf(stderr, "invalid number of arguments: got %d, expected at least %d!\n", a_argc, 1);
        // ... and help ...
        show_help(a_argv[0]);
        // ... can't proceed ...
        return -1;
    }
    
    std::string file = "";
    
    // ... parse arguments ...
    char opt;
    while ( -1 != ( opt = getopt(a_argc, a_argv, "C:H:hcsgrlzujd:i:f:n:v:e:") ) ) {
        switch (opt) {
            case 'h':
                show_help(a_argv[0]);
                return 0;
            case 'C':
                if ( nullptr != optarg && strlen(optarg) > 0 ) {
                    o_create_args.config_file_uri_ = optarg;
                    o_open_args.config_file_uri_   = optarg;
                }
                break;
            case 'H':
                if ( nullptr != optarg && strlen(optarg) > 0 ) {
                    o_create_args.headers_.insert(optarg);
                }
                break;
            case 'f':
                file = ( nullptr != optarg ? optarg : "" );
                break;
            case 's':
                o_action = XAttrAction::Set;
                break;
            case 'g':
                o_action = XAttrAction::Get;
                break;
            case 'r':
                o_action = XAttrAction::Remove;
                break;
            case 'l':
                o_action = XAttrAction::List;
                break;
            case 'z':
                o_action = XAttrAction::Verify;
                break;
            case 'u':
                o_action = XAttrAction::Update;
                break;
            case 'c':
                o_action = XAttrAction::Create;
                break;
            case 'd':
                o_create_args.dir_prefix_ = ( nullptr != optarg ? optarg : "" );
                break;
            case 'i':
                o_create_args.reserved_id_ = ( nullptr != optarg ? optarg : "" );
                break;
            case 'n':
                o_kv_arg.name_ = ( nullptr != optarg ? optarg : "" );
                break;
            case 'e':
                o_kv_arg.expression_ = ( nullptr != optarg ? optarg : "" );
                break;
            case 'j':
                o_open_args.as_json_ = true;
                break;
            case 'v':
                o_kv_arg.value_ = ( nullptr != optarg ? optarg : "" );
                break;
            default:
                fprintf(stderr, "Ilegal option -- %c %s:\n", opt, ( nullptr != optarg ? optarg : "" ));
                show_help(a_argv[0]);
                return -1;
        }
    }
    
    const char* const pch = strstr(file.c_str(), "file://");
    if ( nullptr != pch ) {
        std::string tmp = std::string(pch + sizeof(char) * 7);
        file = tmp;
    }
    
    if ( XAttrAction::Verify == o_action ) {
        o_validate_args.uri_ = file;
    } else if ( XAttrAction::Update == o_action ) {
        o_update_args.uri_ = file;
    } else {
        o_open_args.uri_ = file;
    }
    
    // ... validate options ...
    switch(o_action) {
        case XAttrAction::Create:
        {
            if ( 0 == o_create_args.dir_prefix_.length() ) {
                fprintf(stderr, "Invalid or missing -d option value '%s'\n", o_create_args.dir_prefix_.c_str());
                show_help(a_argv[0]);
                return -1;
            }
            o_create_args.dir_prefix_ = ::cc::fs::Dir::Normalize(o_create_args.dir_prefix_);
        }
            break;
        case XAttrAction::Set:
            // ... value must be preset ...
            if ( 0 == o_kv_arg.value_.length() ) {
                fprintf(stderr, "Invalid or missing -v option value '%s'\n", o_kv_arg.value_.c_str());
                show_help(a_argv[0]);
                return -1;
            }
            if ( false == o_open_args.as_json_ ) {
                // ... fallthrough to test o_kv_arg.name_ ...
            } else {
                break;
            }
        case XAttrAction::Get:
        case XAttrAction::Remove:
            if ( ( XAttrAction::Set == o_action || XAttrAction::Remove == o_action ) && 0 == o_kv_arg.name_.length() ) {
                fprintf(stderr, "Invalid or missing -k option value\n");
                show_help(a_argv[0]);
                return -1;
            } else if ( XAttrAction::Set != o_action &&  0 == o_kv_arg.name_.length() && 0 == o_kv_arg.expression_.length() ) {
                fprintf(stderr, "Invalid or missing -k or -e option value\n");
                show_help(a_argv[0]);
                return -1;
            }
            break;
        default:
            break;
    }

    // ... we're done here ...
    return 0;
}

/**
 * brief
 *
 * param argc
 * param argv
 */
int main(int argc, char ** argv)
{
    
    std::string uri;
    XAttrAction action;
    KVArg       kv;
    std::map<std::string, std::string> attrs;
    
    nb::Archive::OpenArgs     open_args({ "", "", /* as_json_ */ false, /* config_file_uri_*/ "", /* edition_ */ false });
    nb::Archive::CreateArgs   create_args;
    nb::Archive::ValidateArgs validate_args;
    nb::Archive::UpdateArgs   update_args;

    int rc = parse_args(argc, argv, action, open_args, create_args, validate_args, update_args, kv);
    if ( 0 != rc ) {
        return rc;
    }
  
    const char* action_c_str = "Initializing";
    
    try {
        
        nb::Archive archive(NRS_CASPER_NGINX_BROKER_XATTR_INFO, NRS_CASPER_NGINX_BROKER_XATTR_INFO);
        
        if ( XAttrAction::Create == action ) {
            action_c_str = "Create";
            archive.Create(create_args);
            fprintf(stdout, "%s: OK\n", action_c_str);
        } else if ( XAttrAction::Verify == action ) {
            action_c_str = "Integrity";
            archive.Validate(validate_args);
            fprintf(stdout, "%s: OK\n", action_c_str);
        } else if ( XAttrAction::Update == action ) {
            action_c_str = "Update";
            archive.Update(update_args);
            fprintf(stdout, "%s: OK\n", action_c_str);
        } else {
            open_args.edition_ = ( XAttrAction::Set == action || XAttrAction::Remove == action );
            archive.Open(open_args);
            switch (action) {
                case XAttrAction::Set:
                {
                    action_c_str = "Set";
                    if ( true == open_args.as_json_ ) {
                        Json::Reader reader;
                        Json::Value object = Json::Value::null;
                        if ( false == reader.parse(kv.value_, object) ) {
                            const auto errors = reader.getStructuredErrors();
                             if ( errors.size() > 0 ) {
                                 throw ::cc::Exception("An error occurred while parsing JSON payload (%s): %s!" ,
                                                       kv.value_.c_str(),
                                                       errors[0].message.c_str()
                                 );
                             } else {
                                 throw ::cc::Exception("An error occurred while parsing JSON JSON payload (%s)!",
                                                       kv.value_.c_str()
                                 );
                             }
                        }
                        for ( auto member : object.getMemberNames() ) {
                            attrs[member] = object[member].asString();
                        }
                        archive.SetXAttrs(attrs);
                    } else {
                        archive.SetXAttr(kv.name_, kv.value_);
                        attrs[kv.name_] = kv.value_;
                    }
                    serialize_response("Set", attrs, open_args.as_json_);
                }
                    break;
                case XAttrAction::Get:
                {
                    action_c_str = "Get";
                    const std::string& k = ( 0 != kv.expression_.length() ? kv.expression_ : kv.name_ );
                    archive.GetXAttr(std::regex(k, std::regex_constants::ECMAScript), attrs);
                    serialize_response("Attributes", attrs, open_args.as_json_);
                }
                    break;
                case XAttrAction::Remove:
                {
                    action_c_str = "Remove";
                    const std::string& k = ( 0 != kv.expression_.length() ? kv.expression_ : kv.name_ );
                    archive.RemoveXAttr(std::regex(k, std::regex_constants::ECMAScript), attrs);
                    serialize_response("Removed", attrs, open_args.as_json_);
                }
                    break;
                case XAttrAction::List:
                {
                    action_c_str = "List";
                    archive.ListXAttrs([&attrs](const char* const a_name, const char* const a_value) {
                        attrs[a_name] = a_value;
                    });
                    serialize_response("Attributes", attrs, open_args.as_json_);
                }
                    break;
                default:
                    fprintf(stderr, "Dont know how to process requested action!\n");
                    rc = -1;
                    break;
            }
        }        
        
    } catch (const cc::Exception& a_cc_exception) {
        fprintf(stderr, "%s: %s\n", action_c_str, a_cc_exception.what());
        rc = -1;
    } catch (...) {
        try {
            std::rethrow_exception(std::current_exception());
        } catch(const std::exception& e) {
            fprintf(stderr, "%s: %s\n", action_c_str, e.what());
            rc = -1;
        }
    }
    
    return rc;
}
