/**
 * @file ngx_http_casper_broker_cdn_public_module.cc
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

#include "ngx/casper/broker/cdn-public/module/ngx_http_casper_broker_cdn_public_module.h"

#include "ngx/casper/broker/cdn-public/module.h"

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Forward declarations
#pragma mark -
#endif

static void*     ngx_http_casper_broker_cdn_public_module_create_loc_conf (ngx_conf_t* a_cf);
static char*     ngx_http_casper_broker_cdn_public_module_merge_loc_conf  (ngx_conf_t* a_cf, void* a_parent, void* a_child);
static ngx_int_t ngx_http_casper_broker_cdn_public_module_filter_init     (ngx_conf_t* a_cf);

extern ngx_int_t ngx_http_casper_broker_cdn_public_module_content_handler (ngx_http_request_t* a_r);
extern ngx_int_t ngx_http_casper_broker_cdn_public_module_rewrite_handler (ngx_http_request_t* a_r);

#ifdef __APPLE__
#pragma mark -
#pragma mark - Data && Data Types
#pragma mark -
#endif

NGX_BROKER_MODULE_DECLARE_MODULE_ENABLER;

/**
 * @brief This struct defines the configuration command handlers
 */
static ngx_command_t ngx_http_casper_broker_cdn_public_module_commands[] = {
    {
        ngx_string("nginx_casper_broker_cdn_public"),                          /* directive name */
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,                                     /* legal on location context and takes a boolean ("on" or "off") */
        ngx_conf_set_flag_slot,                                                /* translates "on" or "off" to 1 or 0 */
        NGX_HTTP_LOC_CONF_OFFSET,                                              /* value saved on the location struct configuration ... */
        offsetof(ngx_http_casper_broker_cdn_public_module_loc_conf_t, enable), /* ... on the 'enable' element */
        NULL
    },
    ngx_null_command
};

/**
 * @brief The casper-nginx-broker 'af' module context setup data.
 */
static ngx_http_module_t ngx_http_casper_broker_cdn_public_module_ctx = {
    NULL,                                                     /* preconfiguration              */
    ngx_http_casper_broker_cdn_public_module_filter_init,     /* postconfiguration             */
    NULL,                                                     /* create main configuration     */
    NULL,                                                     /* init main configuration       */
    NULL,                                                     /* create server configuration   */
    NULL,                                                     /* merge server configuration    */
    ngx_http_casper_broker_cdn_public_module_create_loc_conf, /* create location configuration */
    ngx_http_casper_broker_cdn_public_module_merge_loc_conf   /* merge location configuration  */
};

/**
 * @brief The casper-nginx-broker 'af' module setup data.
 */
ngx_module_t ngx_http_casper_broker_cdn_public_module = {
    NGX_MODULE_V1,
    &ngx_http_casper_broker_cdn_public_module_ctx,     /* module context    */
    ngx_http_casper_broker_cdn_public_module_commands, /* module directives */
    NGX_HTTP_MODULE,                                   /* module type       */
    NULL,                                              /* init master       */
    NULL,                                              /* init module       */
    NULL,                                              /* init process      */
    NULL,                                              /* init thread       */
    NULL,                                              /* exit thread       */
    NULL,                                              /* exit process      */
    NULL,                                              /* exit master       */
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
static void* ngx_http_casper_broker_cdn_public_module_create_loc_conf (ngx_conf_t* a_cf)
{
    ngx_http_casper_broker_cdn_public_module_loc_conf_t* conf =
    (ngx_http_casper_broker_cdn_public_module_loc_conf_t*) ngx_pcalloc(a_cf->pool, sizeof(ngx_http_casper_broker_cdn_public_module_loc_conf_t));
    if ( NULL == conf ) {
        return NGX_CONF_ERROR;
    }

    conf->enable    = NGX_CONF_UNSET;
    conf->log_token = ngx_null_string;

    return conf;
}

/**
 * @brief The merge conf callback.
 *
 * @param a_cf
 * @param a_parent
 * @param a_child
 */
static char* ngx_http_casper_broker_cdn_public_module_merge_loc_conf (ngx_conf_t* a_cf, void* a_parent, void* a_child)
{
    ngx_http_casper_broker_cdn_public_module_loc_conf_t* prev = (ngx_http_casper_broker_cdn_public_module_loc_conf_t*) a_parent;
    ngx_http_casper_broker_cdn_public_module_loc_conf_t* conf = (ngx_http_casper_broker_cdn_public_module_loc_conf_t*) a_child;

    ngx_conf_merge_value     (conf->enable   , prev->enable   ,           0 ); /* 0 - disabled */
    ngx_conf_merge_str_value (conf->log_token, prev->log_token, "cdn_public");
                              
    NGX_BROKER_MODULE_LOC_CONF_MERGED();

    return (char*) NGX_CONF_OK;
}


/**
 * @brief Filter module boiler plate installation
 *
 * @param a_cf
 */
static ngx_int_t ngx_http_casper_broker_cdn_public_module_filter_init (ngx_conf_t* a_cf)
{
    /*
     * Install the rewrite handler
     */
    const ngx_int_t rv = NGX_BROKER_MODULE_INSTALL_REWRITE_HANDLER(ngx_http_casper_broker_cdn_public_module_rewrite_handler);
    if ( NGX_OK != rv ) {
        return rv;
    }
    
    /**
     * Install content handler.
     */
    return NGX_BROKER_MODULE_INSTALL_CONTENT_HANDLER(ngx_http_casper_broker_cdn_public_module_content_handler);
}

/**
 * @brief Content phase handler, sends the stashed response or if does not exist passes to next handler
 *
 * @param  a_r The http request
 *
 * @return @li NGX_DECLINED if the content is not produced here, pass to next
 *         @li the return of the content sender function
 */
ngx_int_t ngx_http_casper_broker_cdn_public_module_content_handler (ngx_http_request_t* a_r)
{
    /*
     * Check if module is enabled and the request can be handled here.
     */
    NGX_BROKER_MODULE_CONTENT_HANDLER_BARRIER(a_r, ngx_http_casper_broker_cdn_public_module, ngx_http_casper_broker_cdn_public_module_loc_conf_t,
                                              "cdn_public_module");
    /*
     * This module is enabled, handle request.
     */
    return ngx::casper::broker::cdn::common::Module::ContentPhaseTackleResponse(a_r, ngx_http_casper_broker_cdn_public_module, "cdn_public_module");
}

/**
 * @brief
 *
 * @param a_r
 */
ngx_int_t ngx_http_casper_broker_cdn_public_module_rewrite_handler (ngx_http_request_t* a_r)
{
    /*
     * Check if module is enabled and the request can be handled here.
     */
    NGX_BROKER_MODULE_REWRITE_HANDLER_BARRIER(a_r, ngx_http_casper_broker_cdn_public_module, ngx_http_casper_broker_cdn_public_module_loc_conf_t,
                                              "cdn_public_module");

    /**
     * This module is enabled, handle request.
     */
    return ngx::casper::broker::cdn::common::Module::RewritePhaseTackleResponse(a_r, ngx_http_casper_broker_cdn_public_module,
                                                                              "cdn_public_module",
                                                                              [a_r] () -> ngx_int_t {
                                                                                return ngx::casper::broker::cdn::pub::Module::Factory(a_r, /* a_at_rewrite_handler */ true);
                                                                              }
    );
}