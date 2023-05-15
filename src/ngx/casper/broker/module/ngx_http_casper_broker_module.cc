/**
 * @file ngx_http_casper_broker_module.cc
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

#include "ngx/casper/broker/module/ngx_http_casper_broker_module.h"

#include "ngx/casper/broker/initializer.h"

#include "ev/signals.h"

#include "ngx/version.h"

#include <sys/stat.h>

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Forward declarations
#pragma mark -
#endif

static void* ngx_http_casper_broker_module_create_main_conf(ngx_conf_t* a_cf);
static char* ngx_http_casper_broker_module_init_main_conf  (ngx_conf_t* a_cf, void* a_conf);

static void*     ngx_http_casper_broker_module_create_loc_conf (ngx_conf_t* a_cf);
static char*     ngx_http_casper_broker_module_merge_loc_conf  (ngx_conf_t* a_cf, void* a_parent, void* a_child);

static ngx_int_t ngx_http_casper_broker_module_init_process (ngx_cycle_t* a_cycle);
static void      ngx_http_casper_broker_module_exit_process (ngx_cycle_t* a_cycle);

#ifdef __APPLE__
#pragma mark -
#pragma mark - Data && Data Types
#pragma mark -
#endif

/**
 * @brief This struct defines the configuration command handlers
 */
static ngx_command_t ngx_http_casper_broker_module_commands[] = {
    /* service */
    {
        ngx_string("nginx_casper_broker_service_id"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, service_id),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_resources_dir"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, resources_dir),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_connection_validity"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, connection_validity),
        NULL
    },
    /* redis */
    {
        ngx_string("nginx_casper_broker_redis_ip_address"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, redis.ip_address),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_redis_port_number"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, redis.port_number),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_redis_database"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, redis.database),
        NULL
    },
    /* postgresql */
    {
        ngx_string("nginx_casper_broker_redis_max_conn_per_worker"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, redis.max_conn_per_worker),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_connection_string"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.conn_str),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_statement_timeout"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.statement_timeout),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_max_conn_per_worker"),\
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.max_conn_per_worker),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_post_connect_queries"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.post_connect_queries),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_min_queries_per_conn"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.min_queries_per_conn),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_max_queries_per_conn"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, postgresql.max_queries_per_conn),
        NULL
    },
    /* beanstalkd */
    {
        ngx_string("nginx_casper_broker_beanstalkd_host"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, beanstalkd.host),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_port"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, beanstalkd.port),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_timeout"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, beanstalkd.timeout),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_action_tubes"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, beanstalkd.tubes.action),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_sessionless_tubes"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, beanstalkd.tubes.sessionless),
        NULL
    },
    /* curl */
    {
        ngx_string("nginx_casper_broker_curl_max_conn_per_worker"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(nginx_broker_service_conf_t, curl.max_conn_per_worker),
        NULL
    },
    /* gatekeeper */
    {
       ngx_string("nginx_casper_broker_gatekeeper_config_file_uri"),
       NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
       ngx_conf_set_str_slot,
       NGX_HTTP_MAIN_CONF_OFFSET,
       offsetof(nginx_broker_service_conf_t, gatekeeper.config_file_uri),
       NULL
    },
    // --- //
    /* location */
    {
       ngx_string("nginx_casper_broker_location"), /* directive name */
       NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
       ngx_conf_set_str_slot,
       NGX_HTTP_LOC_CONF_OFFSET,
       offsetof(ngx_http_casper_broker_module_loc_conf_t, location),
       NULL
    },
    {
        ngx_string("nginx_casper_broker_supported_content_types"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, supported_content_types),
        NULL
    },
    /* session */
    {
        ngx_string("nginx_casper_broker_session_cookie_name"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session.cookie_name),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_session_cookie_domain"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session.cookie_domain),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_session_cookie_path"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session.cookie_path),
        NULL
    },
    /* jwt */
    {
        ngx_string("nginx_casper_broker_jwt_iss"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, jwt_iss),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_jwt_rsa_public_key_uri"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, jwt_rsa_public_key_uri),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_jwt_allowed_patch_members_set"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, jwt_allowed_patch_members_set),
        NULL
    },
    /* redirect config */
    {
        ngx_string("nginx_casper_broker_cdn_ast_config"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.ast),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_h2e"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.h2e_map),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_redirect_protocol"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.redirect.protocol),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_redirect_host"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.redirect.host),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_redirect_port"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.redirect.port),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_redirect_path"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.redirect.path),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_redirect_location"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.redirect.location),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_content_disposition"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.response.content_disposition),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_temporary_directory_prefix"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.directories.temporary_prefix),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_cdn_directory_prefix"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, cdn.directories.archive_prefix),
        NULL
    },
    /* cc log */
    {
         ngx_string("nginx_casper_broker_cc_log_set"),
         NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
         ngx_conf_set_flag_slot,
         NGX_HTTP_LOC_CONF_OFFSET,
         offsetof(ngx_http_casper_broker_module_loc_conf_t, cc_log.set),
         NULL
    },
    {
         ngx_string("nginx_casper_broker_cc_log_write_body"),
         NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
         ngx_conf_set_flag_slot,
         NGX_HTTP_LOC_CONF_OFFSET,
         offsetof(ngx_http_casper_broker_module_loc_conf_t, cc_log.write_body),
         NULL
     },
     /* jobs */
     {
         ngx_string("nginx_casper_broker_jobs_timeouts_max"),
         NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
         NGX_HTTP_LOC_CONF_OFFSET,
         offsetof(ngx_http_casper_broker_module_loc_conf_t, jobs.timeouts.max),
         NULL
     },
     {
         ngx_string("nginx_casper_broker_jobs_timeouts_enforce"),
         NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
         ngx_conf_set_flag_slot,
         NGX_HTTP_LOC_CONF_OFFSET,
         offsetof(ngx_http_casper_broker_module_loc_conf_t, jobs.timeouts.enforce),
         NULL
     },
     {
         ngx_string("nginx_casper_broker_jobs_log_payload"),
         NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
         ngx_conf_set_flag_slot,
         NGX_HTTP_LOC_CONF_OFFSET,
         offsetof(ngx_http_casper_broker_module_loc_conf_t, jobs.log.payload),
         NULL
     },
     /* ... */
     {
        ngx_string("nginx_casper_broker_errors_serialization_enable_ordered_json"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, awful_conf.ordered_errors_json_serialization_enabled),
        NULL
     },
    /* closure */
    ngx_null_command
};

/**
 * @brief The casper-nginx-broker 'jwt encoder' module context setup data.
 */
static ngx_http_module_t ngx_http_casper_broker_module_ctx = {
    NULL,                                            /* preconfiguration              */
    NULL,                                            /* postconfiguration             */
    ngx_http_casper_broker_module_create_main_conf,  /* create main configuration     */
    ngx_http_casper_broker_module_init_main_conf,    /* init main configuration       */
    NULL,                                            /* create server configuration   */
    NULL,                                            /* merge server configuration    */
    ngx_http_casper_broker_module_create_loc_conf,   /* create location configuration */
    ngx_http_casper_broker_module_merge_loc_conf     /* merge location configuration  */
};

/**
 * @brief The casper-nginx-broker 'jwt encoder' module setup data.
 */
ngx_module_t ngx_http_casper_broker_module = {
    NGX_MODULE_V1,
    &ngx_http_casper_broker_module_ctx,                /* module context    */
    ngx_http_casper_broker_module_commands,            /* module directives */
    NGX_HTTP_MODULE,                                   /* module type       */
    NULL,                                              /* init master       */
    NULL,                                              /* init module       */
    ngx_http_casper_broker_module_init_process,        /* init process      */
    NULL,                                              /* init thread       */
    NULL,                                              /* exit thread       */
    ngx_http_casper_broker_module_exit_process,        /* exit process      */
    NULL,                                              /* exit master       */
    NGX_MODULE_V1_PADDING
};

// MARK: - Module - Implementation

/**
 * @brief Allocate module 'main' config.
 *
 * param a_cf
 */
static void* ngx_http_casper_broker_module_create_main_conf (ngx_conf_t* a_cf)
{
    nginx_broker_service_conf_t* conf = (nginx_broker_service_conf_t*) ngx_pcalloc(a_cf->pool, sizeof(nginx_broker_service_conf_t));
    if ( NULL == conf ) {
        return NGX_CONF_ERROR;
    }
    // ... service ...
    conf->service_id                       = ngx_null_string;
    conf->resources_dir                    = ngx_null_string;
    conf->connection_validity              = NGX_CONF_UNSET;
    // ... redis ...
    conf->redis.ip_address                 = ngx_null_string;
    conf->redis.port_number                = NGX_CONF_UNSET;
    conf->redis.database                   = NGX_CONF_UNSET;
    conf->redis.max_conn_per_worker        = NGX_CONF_UNSET;
    // ... postgresql ...
    conf->postgresql.conn_str              = ngx_null_string;
    conf->postgresql.statement_timeout     = NGX_CONF_UNSET;
    conf->postgresql.max_conn_per_worker   = NGX_CONF_UNSET;
    conf->postgresql.post_connect_queries  = ngx_null_string;
    conf->postgresql.min_queries_per_conn  = NGX_CONF_UNSET;
    conf->postgresql.max_queries_per_conn  = NGX_CONF_UNSET;
    // ... beanstalk ...
    conf->beanstalkd.host                  = ngx_null_string;
    conf->beanstalkd.port                  = NGX_CONF_UNSET_UINT;
    conf->beanstalkd.timeout               = NGX_CONF_UNSET;
    conf->beanstalkd.tubes.action          = ngx_null_string;
    conf->beanstalkd.tubes.sessionless     = ngx_null_string;
    // ... curl ...
    conf->curl.max_conn_per_worker         = NGX_CONF_UNSET;
    // ... gatekeeper ...
    conf->gatekeeper.config_file_uri       = ngx_null_string;
    // ... done ...
    return conf;
}


/**
 * @brief Initialize module 'main' config.
 *
 * param a_cf
 * param a_conf
 */

#define nrs_conf_init_str_value(conf, default) \
    if ( NULL == conf.data ) { \
        conf.len  = strlen(default); \
        conf.data = (u_char*)ngx_palloc(a_cf->pool, conf.len); \
        ngx_memcpy(conf.data, default, conf.len); \
    }

static char* ngx_http_casper_broker_module_init_main_conf (ngx_conf_t* a_cf, void* a_conf)
{
    nginx_broker_service_conf_t* conf = (nginx_broker_service_conf_t*)a_conf;
    /* service */
    nrs_conf_init_str_value (conf->service_id                     , "development" );
    nrs_conf_init_str_value (conf->resources_dir                  ,            "" );
    ngx_conf_init_value     (conf->connection_validity            ,           300 );
    // ... redis ...
    nrs_conf_init_str_value (conf->redis.ip_address               ,            "" );
    ngx_conf_init_value     (conf->redis.port_number              ,          6379 );
    ngx_conf_init_value     (conf->redis.database                 ,            -1 );
    ngx_conf_init_value     (conf->redis.max_conn_per_worker      ,             4 );
    // ... postgresql ...
    nrs_conf_init_str_value (conf->postgresql.conn_str            ,            "" );
    ngx_conf_init_value     (conf->postgresql.statement_timeout   ,           300 ); /* in seconds */
    ngx_conf_init_value     (conf->postgresql.max_conn_per_worker ,             2 );
    nrs_conf_init_str_value (conf->postgresql.post_connect_queries,          "[]" );
    ngx_conf_init_value     (conf->postgresql.min_queries_per_conn,            -1 );
    ngx_conf_init_value     (conf->postgresql.max_queries_per_conn,            -1 );
    // ... beanstalk ...
    nrs_conf_init_str_value (conf->beanstalkd.host                ,   "127.0.0.1" );
    ngx_conf_init_uint_value(conf->beanstalkd.port                ,         11300 );
    ngx_conf_init_value     (conf->beanstalkd.timeout             ,           0.0 );
    nrs_conf_init_str_value (conf->beanstalkd.tubes.sessionless   ,            "" );
    nrs_conf_init_str_value (conf->beanstalkd.tubes.action        ,            "" );
    // ... curl ...
    ngx_conf_init_value     (conf->curl.max_conn_per_worker       ,             4 );
    // ... gatekeeper ...
    nrs_conf_init_str_value (conf->gatekeeper.config_file_uri     ,            "" );

    // ... done ...
    return NGX_CONF_OK;
}

/**
 * @brief Alocate the module configuration structure.
 *
 * @param a_cf
 */
static void* ngx_http_casper_broker_module_create_loc_conf (ngx_conf_t* a_cf)
{
    ngx_http_casper_broker_module_loc_conf_t* conf =
        (ngx_http_casper_broker_module_loc_conf_t*) ngx_pcalloc(a_cf->pool, sizeof(ngx_http_casper_broker_module_loc_conf_t));
    if ( NULL == conf ) {
        return NGX_CONF_ERROR;
    }

    conf->location                         = ngx_null_string;
    conf->supported_content_types          = ngx_null_string;
    // ... session cookie ...
    conf->session.cookie_name              = ngx_null_string;
    conf->session.cookie_domain            = ngx_null_string;
    conf->session.cookie_path              = ngx_null_string;
    // ... jwt ...
    conf->jwt_iss                          = ngx_null_string;
    conf->jwt_rsa_public_key_uri           = ngx_null_string;
    conf->jwt_allowed_patch_members_set    = ngx_null_string;
    // ... cdn config ...
    conf->cdn.ast                          = ngx_null_string;
    conf->cdn.h2e_map                      = ngx_null_string;
    conf->cdn.redirect.protocol            = ngx_null_string;
    conf->cdn.redirect.host                = ngx_null_string;
    conf->cdn.redirect.port                = NGX_CONF_UNSET_UINT;
    conf->cdn.redirect.path                = ngx_null_string;
    conf->cdn.redirect.location            = ngx_null_string;
    conf->cdn.response.content_disposition = ngx_null_string;
    conf->cdn.directories.temporary_prefix = ngx_null_string;
    conf->cdn.directories.archive_prefix   = ngx_null_string;
    // ... cc log ...
    conf->cc_log.set                       = NGX_CONF_UNSET;
    conf->cc_log.write_body                = NGX_CONF_UNSET;
    // ... jobs ...
    conf->jobs.timeouts.max                = NGX_CONF_UNSET;
    conf->jobs.timeouts.enforce            = NGX_CONF_UNSET;
    conf->jobs.log.payload                 = NGX_CONF_UNSET;
    // ... awful code enabler ...
    conf->awful_conf.ordered_errors_json_serialization_enabled = NGX_CONF_UNSET;
    
    return conf;
}

/**
 * @brief The merge conf callback.
 *
 * @param a_cf
 * @param a_parent
 * @param a_child
 */
static char* ngx_http_casper_broker_module_merge_loc_conf (ngx_conf_t* /* a_cf */, void* a_parent, void* a_child)
{
    ngx_http_casper_broker_module_loc_conf_t* prev = (ngx_http_casper_broker_module_loc_conf_t*) a_parent;
    ngx_http_casper_broker_module_loc_conf_t* conf = (ngx_http_casper_broker_module_loc_conf_t*) a_child;
    
    ngx_conf_merge_str_value (conf->location                       , prev->location                       ,            "" );
    ngx_conf_merge_str_value (conf->supported_content_types        , prev->supported_content_types,
                              "[\"application/json\",\"application/vnd.api+json\",\"application/text\",\"application/x-www-form-urlencoded\"]"
    );

    // ... session cookie ...
    ngx_conf_merge_str_value (conf->session.cookie_name            , conf->session.cookie_name            ,            "" );
    ngx_conf_merge_str_value (conf->session.cookie_domain          , conf->session.cookie_domain          ,            "" );
    ngx_conf_merge_str_value (conf->session.cookie_path            , conf->session.cookie_path            ,            "" );
    
    // ... jwt ...
    ngx_conf_merge_str_value (conf->jwt_iss                        , prev->jwt_iss                        ,            "" );
    ngx_conf_merge_str_value (conf->jwt_rsa_public_key_uri         , prev->jwt_rsa_public_key_uri         ,            "" );
    ngx_conf_merge_str_value (conf->jwt_allowed_patch_members_set  , prev->jwt_allowed_patch_members_set  ,            "" );

    // ... redirect config ...
    ngx_conf_merge_str_value (conf->cdn.ast                            , prev->cdn.ast                         ,          "{}" );
    ngx_conf_merge_str_value (conf->cdn.h2e_map                        , prev->cdn.h2e_map                     ,          "{}" );
    ngx_conf_merge_str_value (conf->cdn.redirect.protocol              , prev->cdn.redirect.protocol           ,        "http" );
    ngx_conf_merge_uint_value(conf->cdn.redirect.port                  , prev->cdn.redirect.port               ,            80 );
    ngx_conf_merge_str_value (conf->cdn.redirect.host                  , prev->cdn.redirect.host               ,   "127.0.0.1" );
    ngx_conf_merge_str_value (conf->cdn.redirect.path                  , prev->cdn.redirect.path               ,    "archive"  );
    ngx_conf_merge_str_value (conf->cdn.redirect.location              , prev->cdn.redirect.location           ,    "redirect" );
    ngx_conf_merge_str_value (conf->cdn.response.content_disposition   , prev->cdn.response.content_disposition, "attachment"  );
    ngx_conf_merge_str_value (conf->cdn.directories.temporary_prefix   , prev->cdn.directories.temporary_prefix, "/tmp"        );
    ngx_conf_merge_str_value (conf->cdn.directories.archive_prefix     , prev->cdn.directories.archive_prefix  , "/tmp"        );
    
    // ... cc log ...
    ngx_conf_merge_value     (conf->cc_log.set                         , prev->cc_log.set                      ,             0 ); /* 0 - not set */
    ngx_conf_merge_value     (conf->cc_log.write_body                  , prev->cc_log.write_body               ,             0 ); /* 1 - enabled */

    // ... jobs ...
    ngx_conf_merge_value     (conf->jobs.timeouts.max                  , prev->jobs.timeouts.max               ,          1800 ); /* 1800 seconds, 30 minutes */
    ngx_conf_merge_value     (conf->jobs.timeouts.enforce              , prev->jobs.timeouts.enforce           ,             1 ); /* 1 - enable, 0 - disable */
    ngx_conf_merge_value     (conf->jobs.log.payload                   , prev->jobs.log.payload                ,             0 ); /* 1 - log, 0 - do not log */

    // ... awful code enabler ...
    ngx_conf_merge_value     (conf->awful_conf.ordered_errors_json_serialization_enabled, prev->awful_conf.ordered_errors_json_serialization_enabled, 0 ); /* 0 - disabled - as it always should be */
    
    // ... done ...
    return (char*) NGX_CONF_OK;
}

#include "ngx/casper/broker/initializer.h"
#include "ngx/casper/broker/exception.h"

static std::mutex s_nginx_broker_life_cycle_mutex_;

/**
 * @brief Called whe the a process has started.
 *
 * @param a_cycle NGINX cycle info.
 */
static ngx_int_t ngx_http_casper_broker_module_init_process (ngx_cycle_t* a_cycle)
{
    std::lock_guard<std::mutex> lock(s_nginx_broker_life_cycle_mutex_);

    // ... at the time of writing, nginx log is not ready yet ...
    fprintf(stdout, "Initializing %s\n", NGX_INFO);
    fflush(stdout);

    // ... one-shot initialization ...
    ngx_int_t rv = NGX_OK;
    if ( false == ngx::casper::broker::Initializer::IsInitialized() ) {
        try {
            ngx::casper::broker::Initializer::GetInstance().Startup(a_cycle);
        } catch (...) {
            // ... mark ...
            rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
            try {
                ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
            } catch (const ::cc::Exception& a_cc_exception) {
                // ... at the time of writing, nginx log is not ready yet ...
                fprintf(stderr, "%s initialization failed: %s\n", NGX_INFO, a_cc_exception.what());
                fflush(stderr);
            }
        }
    }
    // ... done ...
    return rv;
}

/**
 * @brief Called when a process is about to exit.
 *
 * @param a_cycle NGINX cycle info.
 */
static void ngx_http_casper_broker_module_exit_process (ngx_cycle_t* /* a_cycle */)
{
    std::lock_guard<std::mutex> lock(s_nginx_broker_life_cycle_mutex_);

    ngx::casper::broker::Initializer::GetInstance().Shutdown(SIGQUIT, /* a_for_cleanup_only */ false);
}
