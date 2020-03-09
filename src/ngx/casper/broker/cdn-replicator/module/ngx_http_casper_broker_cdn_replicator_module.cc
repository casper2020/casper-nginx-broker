/**
 * @file ngx_http_casper_broker_cdn_replicator_module.cc
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

#include "ngx/casper/broker/cdn-replicator/module/ngx_http_casper_broker_cdn_replicator_module.h"

#include "ngx/casper/broker/cdn-replicator/module.h"

#include <sys/stat.h>

#ifndef __APPLE__ // backtrace
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#else
#include <execinfo.h>
#include <stdio.h>
#endif

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Forward declarations
#pragma mark -
#endif

static void*     ngx_http_casper_broker_cdn_replicator_module_create_loc_conf (ngx_conf_t* a_cf);
static char*     ngx_http_casper_broker_cdn_replicator_module_merge_loc_conf  (ngx_conf_t* a_cf, void* a_parent, void* a_child);
static ngx_int_t ngx_http_casper_broker_cdn_replicator_module_filter_init     (ngx_conf_t* a_cf);

extern ngx_int_t ngx_http_casper_broker_cdn_replicator_module_content_handler (ngx_http_request_t* a_r);
extern ngx_int_t ngx_http_casper_broker_cdn_replicator_module_rewrite_handler (ngx_http_request_t* a_r);

#ifdef __APPLE__
#pragma mark -
#pragma mark - Data && Data Types
#pragma mark -
#endif

NGX_BROKER_MODULE_DECLARE_MODULE_ENABLER;

/**
 * @brief This struct defines the configuration command handlers
 */
static ngx_command_t ngx_http_casper_broker_cdn_replicator_module_commands[] = {
    {
        ngx_string("nginx_casper_broker_cdn_replicator"),                          /* directive name */
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,                                         /* legal on location context and takes a boolean ("on" or "off") */
        ngx_conf_set_flag_slot,                                                    /* translates "on" or "off" to 1 or 0 */
        NGX_HTTP_LOC_CONF_OFFSET,                                                  /* value saved on the location struct configuration ... */
        offsetof(ngx_http_casper_broker_cdn_replicator_module_loc_conf_t, enable), /* ... on the 'enable' element */
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_replicator_log_token"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_cdn_replicator_module_loc_conf_t, log_token),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_replicator_directory_prefix"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_cdn_replicator_module_loc_conf_t, directory_prefix),
        NULL
    },
    ngx_null_command
};

/**
 * @brief The casper-nginx-broker 'af' module context setup data.
 */
static ngx_http_module_t ngx_http_casper_broker_cdn_replicator_module_ctx = {
    NULL,                                                         /* preconfiguration              */
    ngx_http_casper_broker_cdn_replicator_module_filter_init,     /* postconfiguration             */
    NULL,                                                         /* create main configuration     */
    NULL,                                                         /* init main configuration       */
    NULL,                                                         /* create server configuration   */
    NULL,                                                         /* merge server configuration    */
    ngx_http_casper_broker_cdn_replicator_module_create_loc_conf, /* create location configuration */
    ngx_http_casper_broker_cdn_replicator_module_merge_loc_conf   /* merge location configuration  */
};

/**
 * @brief The casper-nginx-broker 'af' module setup data.
 */
ngx_module_t ngx_http_casper_broker_cdn_replicator_module = {
    NGX_MODULE_V1,
    &ngx_http_casper_broker_cdn_replicator_module_ctx,     /* module context    */
    ngx_http_casper_broker_cdn_replicator_module_commands, /* module directives */
    NGX_HTTP_MODULE,                                       /* module type       */
    NULL,                                                  /* init master       */
    NULL,                                                  /* init module       */
    NULL,                                                  /* init process      */
    NULL,                                                  /* init thread       */
    NULL,                                                  /* exit thread       */
    NULL,                                                  /* exit process      */
    NULL,                                                  /* exit master       */
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
static void* ngx_http_casper_broker_cdn_replicator_module_create_loc_conf (ngx_conf_t* a_cf)
{
    ngx_http_casper_broker_cdn_replicator_module_loc_conf_t* conf =
    (ngx_http_casper_broker_cdn_replicator_module_loc_conf_t*) ngx_pcalloc(a_cf->pool, sizeof(ngx_http_casper_broker_cdn_replicator_module_loc_conf_t));
    if ( NULL == conf ) {
        return NGX_CONF_ERROR;
    }

    conf->enable           = NGX_CONF_UNSET;
    conf->log_token        = ngx_null_string;
    conf->directory_prefix = ngx_null_string;

    return conf;
}

/**
 * @brief The merge conf callback.
 *
 * @param a_cf
 * @param a_parent
 * @param a_child
 */
static char* ngx_http_casper_broker_cdn_replicator_module_merge_loc_conf (ngx_conf_t* a_cf, void* a_parent, void* a_child)
{
    ngx_http_casper_broker_cdn_replicator_module_loc_conf_t* prev = (ngx_http_casper_broker_cdn_replicator_module_loc_conf_t*) a_parent;
    ngx_http_casper_broker_cdn_replicator_module_loc_conf_t* conf = (ngx_http_casper_broker_cdn_replicator_module_loc_conf_t*) a_child;

    ngx_conf_merge_value     (conf->enable          , prev->enable          ,                0 ); /* 0 - disabled */
    ngx_conf_merge_str_value (conf->log_token       , prev->log_token       , "cdn_replicator" );
    ngx_conf_merge_str_value (conf->directory_prefix, prev->directory_prefix,               "" );
    
    NGX_BROKER_MODULE_LOC_CONF_MERGED();

    return (char*) NGX_CONF_OK;
}


/**
 * @brief Filter module boiler plate installation
 *
 * @param a_cf
 */
static ngx_int_t ngx_http_casper_broker_cdn_replicator_module_filter_init (ngx_conf_t* a_cf)
{
    /*
     * Install the rewrite handler
     */
    const ngx_int_t rv = NGX_BROKER_MODULE_INSTALL_REWRITE_HANDLER(ngx_http_casper_broker_cdn_replicator_module_rewrite_handler);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    /**
     * Install content handler.
     */
    return NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER(ngx_http_casper_broker_cdn_replicator_module_content_handler);
}

/**
 * @brief Content phase handler, sends the stashed response or if does not exist passes to next handler
 *
 * @param  a_r The http request
 *
 * @return @li NGX_DECLINED if the content is not produced here, pass to next
 *         @li the return of the content sender function
 */
ngx_int_t ngx_http_casper_broker_cdn_replicator_module_content_handler (ngx_http_request_t* a_r)
{
    /*
     * Check if module is enabled and the request can be handled here.
     */
    NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER(a_r, ngx_http_casper_broker_cdn_replicator_module, ngx_http_casper_broker_cdn_replicator_module_loc_conf_t,
                                              "cdn_replicator_module");
    /*
     * This module is enabled.
     */
    ngx::casper::broker::cdn::replicator::Module* module = (ngx::casper::broker::cdn::replicator::Module*) ngx_http_get_module_ctx(a_r, ngx_http_casper_broker_cdn_replicator_module);
    if ( nullptr == module ) {
        // ... wtf?
        return NGX_ERROR;
    }
    
    module->SetAtContentHandler();
    
    // ... if not ready yet ...
    if ( false == module->SynchronousResponse() ) {
        return NGX_DECLINED;
    }
    
    if ( true == module->InternalRedirect() ) {
        return NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(module);
    } else if ( true == module->NoContentResponse() ) {
        return NGX_HTTP_NO_CONTENT;
    }
    
    // ... wtf - this module shouldn't reach here ! ...
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

/**
 * @brief
 *
 * @param a_r
 */
ngx_int_t ngx_http_casper_broker_cdn_replicator_module_rewrite_handler (ngx_http_request_t* a_r)
{
    /*
     * Check if module is enabled and the request can be handled here.
     */
    NGX_BROKER_MODULE_REWRITE_HANDLER_BARRIER(a_r, ngx_http_casper_broker_cdn_replicator_module, ngx_http_casper_broker_cdn_replicator_module_loc_conf_t,
                                              "cdn_replicator_module");
    
    const auto tackle_response = [] (ngx_http_request_t* a_r, ngx::casper::broker::cdn::replicator::Module* a_module) -> ngx_int_t {
        //
        if ( true == a_module->SynchronousResponse() ) {
            // ... synchronous response ...
            if ( true == a_module->InternalRedirect() ) {
                NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_cdn_replicator_module, a_r, "cdn_replicator_module",
                                            "RW", "LEAVING",
                                            "%s", "synchronous response - internal redirect!"
                );
                // ... internal redirect and we're done ...
                return NGX_BROKER_MODULE_FINALIZE_REQUEST_WITH_INTERNAL_REDIRECT(a_module);
            } else {
                NGX_BROKER_MODULE_DEBUG_LOG(ngx_http_casper_broker_cdn_replicator_module, a_r, "cdn_replicator_module",
                                            "RW", "LEAVING",
                                            "%s", "synchronous response - not handled where!"
                );
                // ... so, next phase ( content ) by returning ...
                return NGX_DECLINED;
            }
        } else {
            // ... asynchronous response: stall rewrite phase ...
            return NGX_DONE;
        }
    };
    
    //
    // NOTICE: any return code rather han NGX_DECLINED or NGX_DONE will finalize request ...
    //
    ngx_int_t rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
    
    ngx::casper::broker::cdn::replicator::Module* module = (ngx::casper::broker::cdn::replicator::Module*) ngx_http_get_module_ctx(a_r, ngx_http_casper_broker_cdn_replicator_module);
    if ( nullptr == module ) {
        // ... first time this handler is called, it's time to create this request module's context ...
        rv = ngx::casper::broker::cdn::replicator::Module::Factory(a_r, /* a_at_rewrite_handler */true);
        if ( NGX_OK == rv ) {
            rv = tackle_response(a_r, (ngx::casper::broker::cdn::replicator::Module*) ngx_http_get_module_ctx(a_r, ngx_http_casper_broker_cdn_replicator_module));
        } else if ( NGX_DONE == rv ) {
            ngx_http_finalize_request(a_r, NGX_DONE);
            rv = NGX_DECLINED;
        }
    } else {
        module->SetAtRewriteHandler();
        // ... returning from a previous stalled rewrite phase ...
        rv = tackle_response(a_r, module);
    }
    
    // ... we're done ...
    return rv;
}
