/**
 * @file ngx_http_casper_broker_ul_module.cc
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

#include "ngx/casper/broker/ul/module/ngx_http_casper_broker_ul_module.h"

#include "ngx/casper/broker/ul/module/version.h"

#include "ngx/casper/broker/module.h"
#include "ngx/casper/broker/i18.h"

#include "cc/fs/file.h" // XAttr, Writer
#include "cc/utc_time.h"

#include <sys/stat.h>

#include "curl/curl.h"

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Forward declarations
#pragma mark -
#endif

static void*     ngx_http_casper_broker_ul_module_create_loc_conf  (ngx_conf_t* a_cf);
static char*     ngx_http_casper_broker_ul_module_merge_loc_conf   (ngx_conf_t* a_cf, void* a_parent, void* a_child);
static ngx_int_t ngx_http_casper_broker_ul_module_filter_init      (ngx_conf_t* a_cf);

static void      ngx_http_casper_broker_ul_module_read_body_callback (ngx_http_request_t* a_r);
static void      ngx_http_casper_broker_ul_module_cleanup_handler    (void*);

static ngx_int_t ngx_http_casper_broker_ul_ensure_output             (ngx_http_casper_broker_ul_module_context_t* a_context, std::string& o_uri);

static const ngx_str_t  NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_ORIGIN  = ngx_string("Access-Control-Allow-Origin");
static const ngx_str_t  NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_METHODS = ngx_string("Access-Control-Allow-Methods");
static const ngx_str_t  NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_HEADERS = ngx_string("Access-Control-Allow-Headers");

static const ngx_str_t  NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOWED_METHODS = ngx_string("POST"); // , PUT
static const ngx_str_t  NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOWED_HEADERS = ngx_string("Content-Length, Content-Type, Content-Disposition");

#ifdef DEBUG
//    #define NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_WRITE_BUFFER_LIMIT 1 // bytes
//    #define NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT  1 // bytes
#else
    #undef NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_WRITE_BUFFER_LIMIT
    #undef NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT
#endif

#ifdef __APPLE__
    #ifdef NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT
        #define NGX_HTTP_CASPER_BROKER_UL_MODULE_BODY_BUFFER_SIZE NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT
    #else
        #define NGX_HTTP_CASPER_BROKER_UL_MODULE_BODY_BUFFER_SIZE 1024
    #endif
#else 
    // 1MB buffer
    #define NGX_HTTP_CASPER_BROKER_UL_MODULE_BODY_BUFFER_SIZE 1048576
#endif

#ifdef __APPLE__
#pragma mark -
#pragma mark - Data && Data Types
#pragma mark -
#endif

NGX_BROKER_MODULE_DECLARE_MODULE_ENABLER;

/**
 * @brief This struct defines the configuration command handlers
 */
static ngx_command_t ngx_http_casper_broker_ul_module_commands[] = {
    {
        ngx_string("nginx_casper_broker_ul"),                           /* directive name */
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,                              /* legal on location context and takes a boolean ("on" or "off") */
        ngx_conf_set_flag_slot,                                         /* translates "on" or "off" to 1 or 0 */
        NGX_HTTP_LOC_CONF_OFFSET,                                       /* value saved on the location struct configuration ... */
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, enable),  /* ... on the 'enable' element */
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_log_token"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, log_token),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_allowed_origins"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, allowed_origins),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_output_dir"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, output_dir),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_expires_in"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, expires_in),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_accept_multipart_body"),
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, accept_multipart_body),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_max_content_length"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, max_content_length),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_authorization_url"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, authorization_url),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_authorization_ct"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, authorization_ct),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_ul_authorization_tt"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_ul_module_loc_conf_t, authorization_tt),
        NULL
    },
    ngx_null_command
};

/**
 * @brief The casper-nginx-broker 'ul' module context setup data.
 */
static ngx_http_module_t ngx_http_casper_broker_ul_module_ctx = {
    NULL,                                               /* preconfiguration              */
    ngx_http_casper_broker_ul_module_filter_init,       /* postconfiguration             */
    NULL,                                               /* create main configuration     */
    NULL,                                               /* init main configuration       */
    NULL,                                               /* create server configuration   */
    NULL,                                               /* merge server configuration    */
    ngx_http_casper_broker_ul_module_create_loc_conf,   /* create location configuration */
    ngx_http_casper_broker_ul_module_merge_loc_conf     /* merge location configuration  */
};

/**
 * @brief The casper-nginx-broker 'ul' module setup data.
 */
ngx_module_t ngx_http_casper_broker_ul_module = {
    NGX_MODULE_V1,
    &ngx_http_casper_broker_ul_module_ctx,      /* module context    */
    ngx_http_casper_broker_ul_module_commands,  /* module directives */
    NGX_HTTP_MODULE,                            /* module type       */
    NULL,                                       /* init master       */
    NULL,                                       /* init module       */
    NULL,                                       /* init process      */
    NULL,                                       /* init thread       */
    NULL,                                       /* exit thread       */
    NULL,                                       /* exit process      */
    NULL,                                       /* exit master       */
    NGX_MODULE_V1_PADDING
};

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Implementation
#pragma mark -
#endif

/**
 * @brief Alocate the module configuration structure.
 *
 * @param a_cf
 */
static void* ngx_http_casper_broker_ul_module_create_loc_conf (ngx_conf_t* a_cf)
{
    ngx_http_casper_broker_ul_module_loc_conf_t* conf =
        (ngx_http_casper_broker_ul_module_loc_conf_t*) ngx_pcalloc(a_cf->pool, sizeof(ngx_http_casper_broker_ul_module_loc_conf_t));
    if ( NULL == conf ) {
        return NGX_CONF_ERROR;
    }

    conf->enable                = NGX_CONF_UNSET;
    conf->log_token             = ngx_null_string;
    conf->allowed_origins       = ngx_null_string;
    conf->output_dir            = ngx_null_string;
    conf->expires_in            = NGX_CONF_UNSET;
    conf->accept_multipart_body = NGX_CONF_UNSET;
    conf->max_content_length    = NGX_CONF_UNSET;
    conf->authorization_url     = ngx_null_string;
    conf->authorization_ct      = NGX_CONF_UNSET;
    conf->authorization_tt      = NGX_CONF_UNSET;

    return conf;
}

/**
 * @brief The merge conf callback.
 *
 * @param a_cf
 * @param a_parent
 * @param a_child
 */
static char* ngx_http_casper_broker_ul_module_merge_loc_conf (ngx_conf_t* /* a_cf */, void* a_parent, void* a_child)
{
    ngx_http_casper_broker_ul_module_loc_conf_t* prev = (ngx_http_casper_broker_ul_module_loc_conf_t*) a_parent;
    ngx_http_casper_broker_ul_module_loc_conf_t* conf = (ngx_http_casper_broker_ul_module_loc_conf_t*) a_child;

    ngx_conf_merge_value     (conf->enable               , prev->enable               ,          0  ); /* 0 - disabled */
    ngx_conf_merge_str_value (conf->log_token            , prev->log_token            , "ul_module" );
    ngx_conf_merge_str_value (conf->allowed_origins      , prev->allowed_origins      ,          "" );
    ngx_conf_merge_str_value (conf->output_dir           , prev->output_dir           ,     "/tmp/" );
    ngx_conf_merge_uint_value(conf->expires_in           , prev->expires_in           ,      604800 ); /* 604800 seconds -> 7 days */
    ngx_conf_merge_value     (conf->accept_multipart_body, prev->accept_multipart_body,           0 ); /* 0 - no */
    ngx_conf_merge_value     (conf->max_content_length   , prev->max_content_length   ,           0 ); /* not set */
    ngx_conf_merge_str_value (conf->authorization_url    , prev->authorization_url    ,          "" ); /* not set */
    ngx_conf_merge_value     (conf->authorization_ct     , prev->authorization_ct     ,          30 ); /* 30 seconds */
    ngx_conf_merge_value     (conf->authorization_tt     , prev->authorization_tt     ,          60 ); /* 60 seconds */
    
    NGX_BROKER_MODULE_LOC_CONF_MERGED();    
    
    return (char*) NGX_CONF_OK;
}

/**
 * @brief Filter module boiler plate installation
 *
 * @param a_cf
 */
static ngx_int_t ngx_http_casper_broker_ul_module_filter_init (ngx_conf_t* a_cf)
{
    /**
     * Install content handler.
     */
    return NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER(ngx_http_casper_broker_ul_module_content_handler);
}

#ifdef __APPLE__
#pragma mark - Content Handler
#endif

/**
 * @brief Content phase handler, sends the stashed response or if does not exist passes to next handler
 *
 * @param  a_r The http request
 *
 * @return @li NGX_DECLINED if the content is not produced here, pass to next
 *         @li the return of the content sender function
 */
ngx_int_t ngx_http_casper_broker_ul_module_content_handler (ngx_http_request_t* a_r)
{
    /*
     * Check if module is enabled and the request can be handled here.
     */
    NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER(a_r, ngx_http_casper_broker_ul_module, ngx_http_casper_broker_ul_module_loc_conf_t,
                                              "ul_module");

    /*
     * This module is enabled.
     */
    
    // ... first, check if  allowed origins directive is present ...
    if ( 0 == loc_conf->allowed_origins.len ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "missing mandatory directive '%s'", "nginx_casper_broker_ul_allowed_origins"
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // ... now, parse it as a JSON array of strings ...
    Json::Value ao_obj = Json::Value::null;
    try {
        Json::Reader reader;
        if ( false == reader.parse(std::string(reinterpret_cast<const char*>(loc_conf->allowed_origins.data), loc_conf->allowed_origins.len), ao_obj) ) {
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "invalid directive '%s' value - unable to parse it as JSON", "nginx_casper_broker_ul_allowed_origins"
            );
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    } catch (const Json::Exception& a_json_exception) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "invalid directive '%s' value - %s", "nginx_casper_broker_ul_allowed_origins", a_json_exception.what()
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    bool valid_json = true;
    if ( false == ao_obj.isNull() && true == ao_obj.isArray() ) {
        for ( Json::ArrayIndex idx = 0 ; idx < ao_obj.size() ; ++idx ) {
            if ( false == ao_obj[idx].isString() || 0 == ao_obj[idx].asString().length() ) {
                valid_json = false;
                break;
            }
        }
    } else {
        valid_json = false;
    }
    
    if ( false == valid_json ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "invalid directive '%s' value - a JSON array of strings is expected", "nginx_casper_broker_ul_allowed_origins"
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // ... now collect headers ...
    ngx::utls::HeadersMap request_headers;
    ngx::utls::nrs_ngx_read_in_headers(a_r, request_headers);
    
    // ... validate 'authorization' OR 'origin' ....
    const auto authorization_it = request_headers.find("authorization");
    if ( request_headers.end() != authorization_it ) {
        // ... ensure it's enabled ...
        if ( 0 == loc_conf->authorization_url.len || NULL == loc_conf->authorization_url.data ) {
            // ... authorization validation not enabled ...
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "authorization validation not %s ...", "enabled"
            );
            return NGX_HTTP_NOT_ALLOWED;
        }
        const std::string authorization_url = std::string(reinterpret_cast<char const*>(loc_conf->authorization_url.data), loc_conf->authorization_url.len).c_str();
        // ... 'authorization' ...
        std::string type;
        std::string credentials;
        const ngx_int_t authorization_parsing = ngx::utls::nrs_ngx_parse_authorization((authorization_it->first + ": " + authorization_it->second), type, credentials);
        if ( NGX_OK == authorization_parsing ) {
            // ... valid, check type and credentials ...
            if ( 0 != strcasecmp(type.c_str(), "bearer") ) {
                // ... unsupported authorization header type ...
                NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                            "CH", "LEAVING",
                                            "unsupported authorization header type '%s'...", type.c_str()
                );
                return NGX_HTTP_FORBIDDEN;
            } else if ( 0 == credentials.length() ) {
                // ... invalid authorization header value ...
                NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                            "CH", "LEAVING",
                                            "invalid authorization header credentials '%s'...", credentials.c_str()
                );
                return NGX_HTTP_FORBIDDEN;
            } else {
                long  sc = (long)NGX_HTTP_INTERNAL_SERVER_ERROR;
                CURL* handle  = curl_easy_init();
                if ( NULL == handle ) {
                    // ... unable to initialize cURL handle ...
                    NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                "CH", "LEAVING",
                                                "unable to %s cURL handle...", "initialize"
                    );
                } else {
                    // ... initialize cURL ...
                    int ir  = curl_easy_setopt(handle, CURLOPT_URL           , authorization_url.c_str());
                    ir     += curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, loc_conf->authorization_ct);
                    ir     += curl_easy_setopt(handle, CURLOPT_TIMEOUT       , loc_conf->authorization_tt);
                    ir     += curl_easy_setopt(handle, CURLOPT_FORBID_REUSE  , 1L);
                    ir     += curl_easy_setopt(handle, CURLOPT_HTTPGET       , 1L);
                    if ( ir != CURLE_OK ) {
                        // ... unable to initialize cURL request ...
                        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                    "CH", "LEAVING",
                                                    "unable to %s cURL request...", "initialize"
                        );
                    } else {
                        struct curl_slist* headers = curl_slist_append(nullptr, ("Authorization: " + type + " " + credentials).c_str());
                        if ( nullptr == headers ) {
                            // ... unable to allocate cURL request headers ...
                            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                        "CH", "LEAVING",
                                                        "unable to %s cURL request headers...", "allocate"
                            );
                        } else {
                            if ( CURLE_OK != curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers) ) {
                                // ... unable to bind cURL request headers ...
                                NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                            "CH", "LEAVING",
                                                            "unable to %s cURL request headers...", "bind"
                                );
                            } else {
                                // ... perform request ...
                                const CURLcode rc = curl_easy_perform(handle);
                                if ( CURLE_OK != rc ) {
                                    // ... unable to perform cURL request ...
                                    NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                                "CH", "LEAVING",
                                                                "unable to perform cURL request - %s", curl_easy_strerror(rc)
                                    );
                                    // ... special case timeout ....
                                    if ( CURLE_OPERATION_TIMEDOUT == rc ) {
                                        sc = NGX_HTTP_REQUEST_TIME_OUT;
                                    }
                                } else {
                                    // ... succeeded, grab HTTP status code ...
                                    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &sc);
                                }
                            }
                            curl_slist_free_all(headers);
                        }
                    }
                    // ... cleanup ...
                    curl_easy_cleanup(handle);
                  }
                // ... header is valid, check if credentials are also valid ...
                if ( NGX_HTTP_OK != sc ) {
                    return (ngx_int_t)sc;
                }
            }
        } else {
            // ... invalid ...
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "invalid authorization header value '%s'...", ( authorization_it->first + ": " + authorization_it->second ).c_str()
            );
            return NGX_HTTP_FORBIDDEN;
        }
    } else { // ... 'origin' ...
        // ... ensure 'Origin' header is present ...
        const auto origin_it = request_headers.find("origin");
        if ( request_headers.end() == origin_it ) {
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "missing mandatory header '%s'...", "Origin"
            );
            return NGX_HTTP_FORBIDDEN;
        }
        
        // ... and it it's acceptable ...
        std::string acceptable_origin;
        for ( Json::ArrayIndex idx = 0 ; idx < ao_obj.size() ; ++idx ) {
            if ( 0 == strcasecmp(ao_obj[idx].asString().c_str(), origin_it->second.c_str() ) ) {
                acceptable_origin = origin_it->second;
                break;
            }
        }
        
        // ... NOT acceptable?
        if ( 0 == acceptable_origin.length() ) {
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "Origin: %s - not acceptable...", origin_it->second.c_str()
            );
            return NGX_HTTP_FORBIDDEN;
        }
        
        // ... origin is acceptable ...
        ngx_str_t* allowed_origin = ngx::utls::nrs_ngx_copy_string(a_r, acceptable_origin);

        // ... add 'Allow-*' headers ...
        ngx::utls::nrs_ngx_add_response_header(a_r, &NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_ORIGIN,  allowed_origin);
        ngx::utls::nrs_ngx_add_response_header(a_r, &NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_METHODS, &NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOWED_METHODS);
        ngx::utls::nrs_ngx_add_response_header(a_r, &NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOW_HEADERS, &NGX_HTTP_CASPER_BROKER_UL_MODULE_ALLOWED_HEADERS);
    }

    // ... only a pre-upload negotiation?
    if ( a_r->method & NGX_HTTP_OPTIONS ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                                  "CH", "LEAVING",
                                                  "Options: %s", "ok"
        );
        return ngx::utls::nrs_ngx_send_only_header_response(a_r, NGX_HTTP_OK);
    }
    
    // ... only 'POST' method is supported ...
    if ( NGX_HTTP_POST != a_r->method ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                           "CH", "LEAVING",
                                           "Method: %s - not allowed...", std::string(reinterpret_cast<char const*>(a_r->method_name.data), a_r->method_name.len).c_str()
        );
        return NGX_HTTP_NOT_ALLOWED;
    }
    
    // ... upload step ..
    
    // ... check for mandatory directive(s) ...
    if ( 0 == loc_conf->output_dir.len || NULL == loc_conf->output_dir.data ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "missing mandatory directive '%s'...", "nginx_casper_broker_ul_resources_dir"
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // ... create context ...
    ngx_http_casper_broker_ul_module_context_t* context = new ngx_http_casper_broker_ul_module_context_t();
    // ... exceptions are enabled... it will crash up if we ran of memory ...
    for ( auto it : request_headers ) {
        context->in_headers_[it.first] = it.second;
    }

	//
	// 'Content-Type' - https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
	//
    const auto content_type_it = context->in_headers_.find("content-type");
    if ( context->in_headers_.end() == content_type_it ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "missing mandatory header '%s'...", "Content-Type"
        );
        delete context;
        return NGX_HTTP_BAD_REQUEST;
    }

	if ( nullptr != strstr(content_type_it->second.c_str(), "multipart/") ) {
		if ( 0 == loc_conf->accept_multipart_body ) {
			NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "Content-Type : %s - not enabled...", content_type_it->second.c_str()
			);
			delete context;
			return NGX_HTTP_NOT_ALLOWED;
		} else {
			//
			// supported:
			//
			// Content-Type: multipart/form-data; boundary=something
			//
			// media-type - multipart/form-data
			// boundary   - For multipart entities the boundary directive is required!
			ngx::utls::nrs_ngx_multipart_header_info_t info;
			ngx_int_t mph_rv = ngx::utls::nrs_ngx_parse_multipart_content_type(
												(content_type_it->first + ": " + content_type_it->second), info);
			if ( NGX_OK != mph_rv ) {
				NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                            "CH", "LEAVING",
                                            "unable to decode '%s' header...", "Content-Type"
				);
				delete context;
				return NGX_HTTP_BAD_REQUEST;
			}
		}
	} else if ( nullptr == strstr(content_type_it->second.c_str(), "application/octet-stream") ) {
		NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "Content-Type: %s - not supported...", content_type_it->second.c_str()
		 );
		delete context;
		return NGX_HTTP_BAD_REQUEST;
	}

	//
	// 'Content-Disposition' & 'Content-Length'
	//
	if ( 0 == loc_conf->accept_multipart_body ) {
		// 'Content-Disposition'
		const auto content_disposition_it = context->in_headers_.find("content-disposition");
		if ( context->in_headers_.end() == content_disposition_it ) {
			NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "missing mandatory header '%s'...", "Content-Disposition"
			);
			delete context;
			return NGX_HTTP_BAD_REQUEST;
		}
		const std::string value = content_disposition_it->first + ": " + content_disposition_it->second;
		if ( NGX_OK != ngx::utls::nrs_ngx_parse_content_disposition(value,
																	context->content_disposition_,
																	context->field_name_,
																	context->file_.name_) ) {
			NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "Content-Disposition: %s - not supported...", content_disposition_it->second.c_str()
            );
			delete context;
			return NGX_HTTP_NOT_ALLOWED;
		}
		// 'Content-Length'
		const auto content_length_it = context->in_headers_.find("content-length");
		if ( context->in_headers_.end() == content_length_it ) {
			NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "missing mandatory header '%s'...", "Content-Length"
			);
			delete context;
			return NGX_HTTP_BAD_REQUEST;
		}
		context->content_length_ = a_r->headers_in.content_length_n;
        // ... check limit ( if any ) ...
        if ( 0 != loc_conf->max_content_length && context->content_length_ > loc_conf->max_content_length ) {
            NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "CH", "LEAVING",
                                        "entity too large - client intended to send " SIZET_FMT " byte(s), but this module is limited to %u byte(s)!",
                                        context->content_length_, loc_conf->max_content_length
            );
            delete context;
            return NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
        }
	}
    
    // ... nrs_ngx_multipart_body_t ...
    context->multipart_body_.boundary_.set_            = 0;
    context->multipart_body_.content_disposition_.set_ = 0;
    context->multipart_body_.content_type_.set_        = 0;
    context->multipart_body_.terminator_.set_          = 0;
    context->multipart_body_.offset_                   = 0;
    context->multipart_body_.done_                     = 0;
    context->multipart_body_.last_                     = 0;
    context->multipart_body_.enabled_                  = ( 1 == loc_conf->accept_multipart_body ) ? 1 : 0;
    context->file_.writer_                             = nullptr;
    context->file_.bytes_written_                      = 0;
    context->file_.expires_in_                         = static_cast<int64_t>(loc_conf->expires_in);
    context->file_.directory_                          = std::string(reinterpret_cast<const char*>(loc_conf->output_dir.data), loc_conf->output_dir.len);
    if ( '/' != context->file_.directory_.c_str()[context->file_.directory_.length() - 1] ) {
        context->file_.directory_ += '/';
    }
    context->error_tracker_                            = new ngx::casper::broker::ul::Errors("en_US");
    context->read_callback_again_                      = false;
    context->read_callback_count_                      = 0;
    
    ngx_http_set_ctx(a_r, context, ngx_http_casper_broker_ul_module);
    
    // ... set cleanup handler ...
    ngx_http_cleanup_t* cleanup = ngx_http_cleanup_add(a_r, 0);
    if ( NULL == cleanup ) {
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "%s", "unable to install cleanup handler..."
        );
        ngx_http_set_ctx(a_r, NULL, ngx_http_casper_broker_ul_module);
        delete context;
        return NGX_ERROR;
    }
    cleanup->handler = ngx_http_casper_broker_ul_module_cleanup_handler;
    cleanup->data    = a_r;
    
#ifdef DEBUG
    
#ifdef NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT
    {
        const int  buffer_size = NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_READ_BUFFER_LIMIT; // in bytes
        const bool success     = ( 0 == setsockopt(a_r->connection->fd, SOL_SOCKET, SO_RCVBUF, (const void *) &buffer_size, sizeof(int)) );
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "socket read buffer limit: %s to %d bytes", ( true == success ? "set" : "unable to set" ), buffer_size
        );
    }
#endif
    
#ifdef NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_WRITE_BUFFER_LIMIT
    {
        const int  buffer_size = NGX_HTTP_CASPER_BROKER_UL_MODULE_SOCKET_WRITE_BUFFER_LIMIT; // in bytes
        const bool success     = ( 0 == setsockopt(a_r->connection->fd, SOL_SOCKET, SO_SNDBUF, (const void *) &buffer_size, sizeof(int)) );
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "socket write buffer limit: %s to %d bytes", ( true == success ? "set" : "unable to set" ), buffer_size
        );
    }
#endif
    
#endif
    
    // ... if no multipart ...
    if ( 0 == context->multipart_body_.enabled_ ) {
        // ... notify nginx to keep temporary file so it can be moved or erased by this module later...
        a_r->request_body_in_persistent_file = 1;
        a_r->request_body_in_clean_file      = 0;
    }
    
    const ngx_int_t rc = ngx_http_read_client_request_body(a_r, ngx_http_casper_broker_ul_module_read_body_callback);
    
    if ( 0 == context->error_tracker_->Count() && ( NGX_OK /* 0 */ == rc || NGX_AGAIN /* -2 */ == rc ) ) {
        
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "CH", "LEAVING",
                                    "with no errors, rc is %d ...", rc
        );
        
        context->read_callback_again_ = ( NGX_AGAIN == rc );
        
        return ( NGX_AGAIN == rc ? NGX_DONE : rc );
    }

    // ... if no multipart ...
    if ( 0 == context->multipart_body_.enabled_ ) {
        // ... rollback ...
        a_r->request_body_in_persistent_file = 1;
        a_r->request_body_in_clean_file      = 0;
    }
    
    NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                "CH", "LEAVING",
                                "with nginx error code %d ...", rc
    );

    return NGX_ERROR;
}

#ifdef __APPLE__
#pragma mark - Body Handler
#endif

/**
 * @brief Read request body.
 *
 * @param a_r
 */
static void ngx_http_casper_broker_ul_module_read_body_callback (ngx_http_request_t* a_r)
{
    u_char            buffer [NGX_HTTP_CASPER_BROKER_UL_MODULE_BODY_BUFFER_SIZE];
    off_t             read_offset        = 0;
    ssize_t           bytes_read         = 0;
    ngx_uint_t        http_status_code   = NGX_HTTP_OK;
    bool              wtf_bailout        = false;
    bool              clean_exit         = false;
    bool              errors_set         = false;
    std::stringstream ss;
    std::string       msg;

    
    ngx_http_casper_broker_ul_module_context_t* context = NULL;
    ngx_chain_t*                                chain   = NULL;

    // ... no context?
    context = (ngx_http_casper_broker_ul_module_context_t*) ngx_http_get_module_ctx(a_r, ngx_http_casper_broker_ul_module);
    if ( NULL == context ) {
        // ... no context!
        NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "RB", "ENTER",
                                    "no context: %s!", "WTF"
        );
        http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        wtf_bailout      = true;
        goto bailout;
    }
    
    // ... track the number of times this callback was called ...
    context->read_callback_count_++;

    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                "RB", "ENTER",
                                "READING BODY ( %d )", context->read_callback_count_
    );

    // ... nothing to read?
    if ( NULL == a_r->request_body || NULL == a_r->request_body->bufs ) {
        // ... no body!
        http_status_code = NGX_HTTP_BAD_REQUEST;
        context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                       "Request's body is empty!"
        );
        goto bailout;
    }
    chain = a_r->request_body->bufs;
    
    //
    // NO MULTIPART BODY?
    //

    if ( 0 == context->multipart_body_.enabled_ ) {

		NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "RB", "NO-MULTIPART",
                                    "%s", ( 1 == chain->buf->in_file ? "from file" : "from memory buffers" )
		);

        // ... set ouput uri ...
        http_status_code = ngx_http_casper_broker_ul_ensure_output(context, context->file_.uri_);
        if ( NGX_HTTP_OK != http_status_code ) {
            // ... at least one error should already be set ...
            goto done;
        }

        NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "RB", "TO",
                                    "%s", ( "file://" + context->file_.uri_).c_str()
        );

        // ... from file or memory? ...
        if ( 1 == chain->buf->in_file ) {
            
            // ... file, just move it ...
            const std::string temp_file = std::string(reinterpret_cast<const char*>(chain->buf->file->name.data), chain->buf->file->name.len);

            const int rv = rename(temp_file.c_str(), context->file_.uri_.c_str());

			NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "RB", "READING",
                                        "move '%s' to '%s' ~> %d", temp_file.c_str(), context->file_.uri_.c_str(), rv
			);

            if ( 0 != rv ) {
                http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                               ( std::string("Error while moving file to final destination - ") + strerror(rv) ).c_str()
                );
            } else {
                context->file_.bytes_written_ = chain->buf->file_last - chain->buf->file_pos;
            }

            goto done;
            
        } else {

			NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "RB", "READING",
                                        "%s", "reading body from memory buffers..."
			);

            // ... memory, write to file ...
            context->file_.writer_ = new cc::fs::file::Writer();
            
            // ... open file now ...
            try {
                context->file_.writer_->Open(context->file_.uri_, cc::fs::file::Writer::Mode::Write);
                context->file_.md5_.Initialize();
            } catch (const cc::Exception& a_cc_exception) {
                ss.str("");
                ss << "Error while opening file '" << context->file_.uri_ << "': " << a_cc_exception.what();
                msg = ss.str();
                http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                               msg.c_str()
                );
                goto done;
            }

            // ... memory, write to file ...
            while ( ( 0 == context->error_tracker_->Count() ) && NULL != chain ) {
                
                // ... read fro memory ....
                u_char* buffer_ptr = chain->buf->pos;
                size_t  bytes_read = chain->buf->last - buffer_ptr;
                
                // ... write data to file ...
                try {
                    context->file_.bytes_written_ += context->file_.writer_->Write(buffer_ptr, bytes_read, /* a_flush */ true);
                    context->file_.md5_.Update(buffer_ptr, bytes_read);
                } catch (const cc::Exception& a_cc_exception) {
                    ss.str("");
                    ss << "Error while writing to file '" << context->file_.writer_->URI() << "': " << a_cc_exception.what();
                    msg = ss.str();
                    http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   msg.c_str()
                    );
                    break;
                }
                
                // next ...
                chain = chain->next;
            }
            
            goto done;
            
        }
        
    }
    
    //
    // !!! MULTIPART BODY !!!
    //
    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                "RB", "MULTIPART",
                                "%s", ( 1 == chain->buf->in_file ? "from file" : "from memory buffers" )
    );
    
    //
    // WARNING:
    //
    // !!! At the moment of this module development:
    //
    // 1 ) non-blocking I/O was already set by nginx
    // 2 ) relying on having NGX_HAVE_PREAD flag set to 1 ( at ngx_auto_config.h )
    // 3 ) when this flag is defined, ngx_read_file calls pread which will return
    //     EAGAIN - The file was marked for non-blocking I/O, and no data were ready to be read.
    //
    // !!! THIS info was confirmed to at macOS and Debian OS
    //
    // ... read data ...
    while ( ( 0 == context->error_tracker_->Count() ) ) {
        
        // ... no more data to read?
        if ( NULL == chain ) {
            // ... done ...
            break;
        }

        u_char* buffer_ptr;

        // ... from file or memory?
        if ( 1 == chain->buf->in_file ) {
            // ... read from file ...
            if ( ( bytes_read = ngx_read_file(chain->buf->file, buffer, NGX_HTTP_CASPER_BROKER_UL_MODULE_BODY_BUFFER_SIZE, read_offset) ) <= 0 ) {
                break;
            }
            buffer_ptr = buffer;
        } else {
            // ... read fro memory ....
            buffer_ptr = chain->buf->pos;
            bytes_read = chain->buf->last - buffer_ptr;
            // next ...
            chain = chain->next;
        }
        
        size_t write_offset = 0;
        
        if ( 0 == context->multipart_body_.done_ ) {
            
            size_t bytes_used = 0;
            
            ngx_int_t mpb_parse_rc = ngx::utls::nrs_ngx_parse_multipart_body(buffer_ptr, bytes_read, context->multipart_body_,
                                                                             bytes_used);
            if ( NGX_AGAIN == mpb_parse_rc ) {
                
                // ... still waiting for more data ...
                read_offset += bytes_read;
                continue;
                
            } else if ( NGX_DONE == mpb_parse_rc ) {
                
                //
                // 'Content-Disposition'
                //
                if ( NGX_OK != ngx::utls::nrs_ngx_parse_content_disposition(context->multipart_body_.content_disposition_.value_,
                                                                            context->content_disposition_,
                                                                            context->field_name_, context->file_.name_) ) {
                    http_status_code = NGX_HTTP_BAD_REQUEST;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   "Missing or invalid 'Content-Disposition' multipart body data!"
                    );
                    break;
                }
                
                // ... we're only expecting a 'form-data' ...
                if ( 0 != strcasecmp(context->content_disposition_.c_str(), "form-data") ) {
                    
                    ss.str("");
                    ss << "'Content-Disposition': " << context->content_disposition_.c_str();
                    ss << " is not supported, 'form-data' is expected!";
                    msg = ss.str().c_str();
                    
                    http_status_code = NGX_HTTP_BAD_REQUEST;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   msg.c_str()
                    );
                    break;
                }
                
                //
                // 'Content-Type'
                //
                if ( NGX_OK != ngx::utls::nrs_ngx_parse_content_type(context->multipart_body_.content_type_.value_, context->content_type_) ) {
                    http_status_code = NGX_HTTP_BAD_REQUEST;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   "Missing or invalid 'Content-Type' multipart body data!");
                    break;
                }
                // ... final adjustment ..
                context->content_length_ = a_r->headers_in.content_length_n - ( /* includes first boundary */
                                                                               context->multipart_body_.offset_ +
                                                                               /* last boundary */
                                                                               context->multipart_body_.boundary_.value_.length() + strlen(CRLF "--" CRLF)
                );
                
                //
                // Setup output file...
                //
                
                // ... sanity checkpoint ...
                if ( nullptr != context->file_.writer_ ) {
                    http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   "Unexpected file writer state, shouldn't be set yet!"
                    );
                    break;
                }
                
                // ... ensure output ...
                http_status_code = ngx_http_casper_broker_ul_ensure_output(context, context->file_.uri_);
                if ( NGX_HTTP_OK != http_status_code ) {
                    // ... at least one error should already be set ...
                    break;
                }
                
                // ... as is ...
                context->file_.writer_ = new cc::fs::file::Writer();
                
                // ... open file now ...
                try {
                    context->file_.writer_->Open(context->file_.uri_, cc::fs::file::Writer::Mode::Write);
                    context->file_.md5_.Initialize();
                } catch (const cc::Exception& a_cc_exception) {
                    ss.str("");
                    ss << "Error while opening file '" << context->file_.uri_ << "': " << a_cc_exception.what();
                    msg = ss.str();
                    http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   msg.c_str()
                    );
                    break;
                }
                
                if ( bytes_used > bytes_read ) {
                    // ... sanity check failed!
                    http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                    context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                                   "Error while parsing multipart body data - used more bytes than received!"
                    );
                    break;
                } else if ( bytes_used == bytes_read ) {
                    read_offset += bytes_used;
                    continue;
                } else {
                    write_offset = context->multipart_body_.offset_;
                    read_offset += bytes_read;
                }
                
            } else {
                http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                               "Error while parsing multipart body data!"
                );
                break;
            }
            
        } else {
            // ... next ...
            read_offset += bytes_read;
        }
        
        // ... adjust buffer pointer and amount of bytes to write ...
        size_t bytes_to_write = static_cast<size_t>(bytes_read) - write_offset;
        if ( ( context->file_.bytes_written_ + bytes_to_write ) > context->content_length_ ) {
            size_t diff = context->content_length_ - context->file_.bytes_written_;
            if ( 0 == diff ) {
                bytes_to_write = 0;
                break;
            }
            bytes_to_write = diff;
        }
        
        // ... write data to file ...
        try {
            context->file_.bytes_written_ += context->file_.writer_->Write(buffer_ptr + write_offset, bytes_to_write, /* a_flush */ true);
            context->file_.md5_.Update(buffer_ptr + write_offset, bytes_to_write);
        } catch (const cc::Exception& a_cc_exception) {
            ss.str("");
            ss << "Error while writing to file '" << context->file_.writer_->URI() << "': " << a_cc_exception.what();
            msg = ss.str();
            http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                           msg.c_str()
            );
            break;
        }

    }
    
done:
    
    // ... I/O ...
    if ( EAGAIN == bytes_read ) {
        NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "RB", "EAGAIN",
                                    "%s", ""
        );
        // .... this method should be called again by nginx when fd is ready ...
        return;
    }
    
    // ... done?
    clean_exit = ( 0 == ( context->content_length_ - context->file_.bytes_written_ ) );
    errors_set = ( context->error_tracker_->Count() > 0 || bytes_read < 0 );
    
    if ( true == clean_exit || true == errors_set  ) {
        
        // ... read error?
        if ( bytes_read < 0 ) {
            ss.str("");
            ss << "Error while reading request body near byte " << read_offset  << "!";
            msg = ss.str();
            http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                           msg.c_str()
            );
        }
        
        // ... not complete?
        if ( 0 != ( static_cast<ssize_t>(context->content_length_) - static_cast<ssize_t>(context->file_.bytes_written_) ) ) {
            ss.str("");
            ss << "Error while finalizing request - byte count mismatch:";
            ss << " expected to write " << context->content_length_ << " byte(s)";
            ss << " end up writing " << context->file_.bytes_written_ << " byte(s)";
            msg = ss.str();
            http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", http_status_code, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                           msg.c_str()
            );
        }

        // ... file can be closed now ...
        if ( nullptr != context->file_.writer_ ) {
            const std::string uri = context->file_.writer_->URI();
            try {
                
                // ... close file ...
                context->file_.writer_->Close();
                                
                // ... and write xattrs ...
                ::cc::fs::file::XAttr xattrs(uri);
                
                // ... upload id is same as file name ...
                std::string id;  ::cc::fs::File::Name(uri, id);

                // ... set upload id ...
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.id", id);

                // ... http info ...
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.protocol"   ,
                           std::string(reinterpret_cast<char const*>(a_r->http_protocol.data), a_r->http_protocol.len)
                );
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.method"   ,
                           std::string(reinterpret_cast<char const*>(a_r->method_name.data), a_r->method_name.len)
                );
                if ( nullptr != a_r->headers_in.user_agent && a_r->headers_in.user_agent->value.len > 0 ) {
                    xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.user-agent"    ,
                               std::string(reinterpret_cast<char const*>(a_r->headers_in.user_agent->value.data), a_r->headers_in.user_agent->value.len)
                   );
                }

                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.ip",
                           std::string(reinterpret_cast<char const*>(a_r->connection->addr_text.data), a_r->connection->addr_text.len)
                );
 
                for ( auto header : { "origin", "referer" } ) {
                    const auto it = context->in_headers_.find(header);
                    if ( context->in_headers_.end() == it ) {
                        continue;
                    }
                    xattrs.Set(std::string(XATTR_ARCHIVE_PREFIX "com.cldware.upload.") + header,
                               it->second
                    );
                }
                
                // ... module & timestamp info ...
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.created.by", NGX_CASPER_BROKER_UL_MODULE_INFO);
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.created.at", cc::UTCTime::NowISO8601WithTZ());
                
                // ... content info ...
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.uri", "file://" + uri);
                if ( nullptr != a_r->headers_in.content_type && a_r->headers_in.content_type->value.len > 0 ) {
                    xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.content-type",
                               std::string(reinterpret_cast<char const*>(a_r->headers_in.content_type->value.data), a_r->headers_in.content_type->value.len)
                    );
                }                
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.content-length", std::to_string(context->file_.bytes_written_));
                xattrs.Set(XATTR_ARCHIVE_PREFIX "com.cldware.upload.md5"           ,  context->file_.md5_.Finalize());
                xattrs.Seal(XATTR_ARCHIVE_PREFIX "com.cldware.upload.seal"         ,
                            reinterpret_cast<const unsigned char*>(NGX_CASPER_BROKER_UL_MODULE_INFO), strlen(NGX_CASPER_BROKER_UL_MODULE_INFO),
                            /* a_excluding_attrs */ nullptr
                );

            } catch (const cc::Exception& a_cc_exception) {
                ss.str("");
                ss << "Error while closing file '" << uri << "': " << a_cc_exception.what();
                msg = ss.str();
                http_status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                               msg.c_str()
                );
            }
        }
        
        errors_set = ( context->error_tracker_->Count() > 0 );
    }
    
bailout:
        
    errors_set = ( context->error_tracker_->Count() > 0 || bytes_read < 0 );

    if ( true == errors_set && 0 == context->multipart_body_.enabled_ && NULL != chain && 1 == chain->buf->in_file && chain->buf->file->name.len > 0 ) {
        try {
            const std::string tmp_file = std::string(reinterpret_cast<const char*>(chain->buf->file->name.data), chain->buf->file->name.len);
            if ( true == ::cc::fs::File::Exists(tmp_file) ) {
                ::cc::fs::File::Erase(tmp_file);
            }
        } catch (const cc::Exception& a_cc_exception) {
            NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "RB", "LEAVING",
                                        "Error while erasing temporary file: %s", a_cc_exception.what()
            );
        }
    }
    
    if ( true == wtf_bailout ) {
        
        NGX_BROKER_MODULE_ERROR_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                    "RB", "LEAVING",
                                    "%s", "WTF bailout!"
        );
        
        a_r->connection->read->eof = 1;
        ngx_http_finalize_request(a_r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        
    } else if ( true == clean_exit || true == errors_set )
    
        try {

            Json::FastWriter json_writer;

            const char* content_type;
            std::string content_value;
            
            if ( true == errors_set ) {
                
                content_type  = context->error_tracker_->content_type_.c_str();
                content_value = json_writer.write(context->error_tracker_->Serialize2JSON());
                
            } else { /* true == clean_exit */

                Json::Value response = Json::Value(Json::ValueType::objectValue);
                
                response["file"] = std::string(context->file_.uri_.c_str(),  context->file_.directory_.length(), context->file_.uri_.length() - context->file_.directory_.length());

                content_type  = "application/json";
                content_value = json_writer.write(response);

            }

            NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "RB", "LEAVING",
                                        "%s", "done"
            );

            ngx::casper::broker::Module::WriteResponse(ngx_http_casper_broker_ul_module, a_r,
                                                       /* a_headers      */ nullptr,
                                                       /* a_status_code  */ http_status_code,
                                                       /* a_content_type */ content_type,
                                                       /* a_data         */ content_value.c_str(),
                                                       /* a_length       */ content_value.length(),
                                                       /* a_binary       */ false,
                                                       /* a_finalize     */ ( true == context->read_callback_again_ ),
                                                       /* a_log_token    */ "ul_module",
                                                       /* a_plain_module */ true
            );

        } catch (const Json::Exception& a_json_exception) {

            NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, a_r, "ul_module",
                                        "RB", "LEAVING",
                                        "%s", a_json_exception.what()
            );
            
            a_r->connection->read->eof = 1;
            ngx_http_finalize_request(a_r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            
        }

}

#ifdef __APPLE__
#pragma mark - Cleanup
#endif

/**
 * @brief This method will be called when nginx is about to finalize the main request.
 *
 * @param a_data In this module, is the pointer to the request it self.
 */
static void ngx_http_casper_broker_ul_module_cleanup_handler (void* a_data)
{
    ngx_http_request_t*                         request = (ngx_http_request_t*) a_data;
    ngx_http_casper_broker_ul_module_context_t* context = (ngx_http_casper_broker_ul_module_context_t*) ngx_http_get_module_ctx(request, ngx_http_casper_broker_ul_module);
    
    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                "FH", "ENTER",
                                "%s", ""
    );

    if ( context != NULL ) {
        
        NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                    "FH", "DELETE",
                                    "%s", ""
        );

        if ( nullptr != context->file_.writer_ ) {
            delete context->file_.writer_;
        }
        
        if ( nullptr != context->error_tracker_ ) {
            delete context->error_tracker_;
        }
        
        if ( 0 == context->multipart_body_.enabled_ ) {
            ngx_chain_t* chain = request->request_body->bufs;
            if ( NULL != chain && 1 == chain->buf->in_file && chain->buf->file->name.len > 0 ) {
                const std::string tmp_file = std::string(reinterpret_cast<const char*>(chain->buf->file->name.data), chain->buf->file->name.len);
                try {
                    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                                "FH", "DELETE",
                                                "Checking : '%s'...", tmp_file.c_str()
                    );
                    if ( true == ::cc::fs::File::Exists(tmp_file) ) {
                        NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                                    "FH", "DELETE",
                                                    "Erasing temporary file: '%s'...", tmp_file.c_str()
                        );
                        ::cc::fs::File::Erase(tmp_file);
                    }
                } catch (const cc::Exception& a_cc_exception) {
                    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                                "FH", "DELETE",
                                                "Error while erasing temporary file: %s", a_cc_exception.what()
                    );
                }
            }
        }
        
        delete context;
        ngx_http_set_ctx(request, NULL, ngx_http_casper_broker_ul_module);
    }
    
    NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_ul_module, request, "ul_module",
                                "FH", "LEAVING",
                                "%s", "--- CLEANUP DONE ---"
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Ensure output can proceeed.
 *
 * @param a_context
 * @param o_uri
 */
static ngx_int_t ngx_http_casper_broker_ul_ensure_output (ngx_http_casper_broker_ul_module_context_t* a_context, std::string& o_uri)
{
    std::string tmp_uri = a_context->file_.directory_;
    
    // ... get time w/offset ...
    cc::UTCTime::HumanReadable now_hrt = cc::UTCTime::ToHumanReadable(cc::UTCTime::OffsetBy(a_context->file_.expires_in_));
    
    // ... setup new output directory ...
    char hrt_buffer[27] = { '\0' };
    const int w = snprintf(hrt_buffer, 26, "%04d-%02d-%02d",
                           static_cast<int>(now_hrt.year_), static_cast<int>(now_hrt.month_), static_cast<int>(now_hrt.day_)
    );
    if ( w < 0 || w > 26 ) {
        a_context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                         "Unable to change output directory - buffer write error!"
                                        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    tmp_uri += hrt_buffer;

    std::stringstream ss;
    std::string       tmp;

    // ... create directory structure ..
    const mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH;
    if ( mkdir(tmp_uri.c_str(), mode) == -1 ) {
        const int error_number = errno;
        if ( error_number != EEXIST ) {
            ss << "Unable to create output directory '" << tmp_uri << "': " << strerror(error_number) << "!";
            tmp = ss.str();
            a_context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                             tmp.c_str()
                                            );
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    
    // ... ensure there is enough space ...
    // ... ( but we're not reserving it, just preventing a dry-run ... )
    uint64_t available_space = 0;

    struct statfs stat_data;
    if ( -1 != statfs(tmp_uri.c_str(), &stat_data) ) {
        available_space = (uint64_t)((uint64_t)stat_data.f_bfree * (uint64_t)stat_data.f_bsize);
        if ( available_space <= static_cast<uint64_t>(a_context->content_length_) ) {
            ss << "Not enougth free space to write file to directory '" << tmp_uri << "':";
            ss << " required " << a_context->content_length_ << " bytes";
            ss << " but there are only " << available_space << " bytes available!";
            tmp = ss.str();
            a_context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                             tmp.c_str()
            );
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    } else {
        ss << "Unable to check if there is enough free space to write file to directory '" << tmp_uri << "': " << strerror(errno) << "!";
        tmp = ss.str();
        a_context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                         tmp.c_str()
                                         );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    tmp_uri += '/';
    
    // ... ensure a unique file ...
    try {
        
        std::string name;
        std::string extension;
        
        const char* ext_ptr = strrchr(a_context->file_.name_.c_str(), '.');
        if ( nullptr != ext_ptr ) {
            name      = std::string(a_context->file_.name_.c_str(), ext_ptr - a_context->file_.name_.c_str());
            extension = std::string(ext_ptr + 1);
        } else {
            name      = a_context->file_.name_;
            extension = "ul";
        }
        
        cc::fs::File::Unique(tmp_uri, name, extension, tmp_uri);
        
    } catch (const cc::Exception& a_cc_exception) {
        ss << "Unable to create a unique file name: " << a_cc_exception.what() << "!";
        tmp = ss.str();
        a_context->error_tracker_->Track("UL_AN_ERROR_OCCURRED_MESSAGE", NGX_HTTP_INTERNAL_SERVER_ERROR, "UL_AN_ERROR_OCCURRED_MESSAGE",
                                         tmp.c_str()
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    o_uri = tmp_uri;

    // ... we're good to go ...
    return NGX_HTTP_OK;
}
