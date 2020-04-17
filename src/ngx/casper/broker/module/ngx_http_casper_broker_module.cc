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


static void      ngx_http_casper_broker_module_exit_master_process (ngx_cycle_t* a_cycle);

// static ngx_int_t ngx_http_casper_broker_module_init_process        (ngx_cycle_t* a_cycle);
static void      ngx_http_casper_broker_module_exit_process        (ngx_cycle_t* a_cycle);

static void*     ngx_http_casper_broker_module_create_loc_conf (ngx_conf_t* a_cf);
static char*     ngx_http_casper_broker_module_merge_loc_conf  (ngx_conf_t* a_cf, void* a_parent, void* a_child);

#ifdef __APPLE__
#pragma mark -
#pragma mark - Data && Data Types
#pragma mark -
#endif

/**
 * @brief This struct defines the configuration command handlers
 */
static ngx_command_t ngx_http_casper_broker_module_commands[] = {
    {
       ngx_string("nginx_casper_broker_location"), /* directive name */
       NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
       ngx_conf_set_str_slot,
       NGX_HTTP_LOC_CONF_OFFSET,
       offsetof(ngx_http_casper_broker_module_loc_conf_t, location),
       NULL
    },
    {
        ngx_string("nginx_casper_broker_service_id"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, service_id),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_resources_dir"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, resources_dir),
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
    {
        ngx_string("nginx_casper_broker_connection_validity"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, connection_validity),
        NULL
    },
    /* redis */
    {
        ngx_string("nginx_casper_broker_redis_ip_address"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, redis_ip_address),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_redis_port_number"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, redis_port_number),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_redis_database"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, redis_database),
        NULL
    },
    /* postgresql */
    {
        ngx_string("nginx_casper_broker_redis_max_conn_per_worker"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, redis_max_conn_per_worker),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_connection_string"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_conn_str),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_statement_timeout"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_statement_timeout),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_max_conn_per_worker"),\
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_max_conn_per_worker),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_post_connect_queries"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_post_connect_queries),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_min_queries_per_conn"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_min_queries_per_conn),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_postgresql_max_queries_per_conn"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, postgresql_max_queries_per_conn),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_curl_max_conn_per_worker"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, curl_max_conn_per_worker),
        NULL
    },
    /* beanstalkd */
    {
        ngx_string("nginx_casper_broker_beanstalkd_host"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, beanstalkd.host),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_port"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, beanstalkd.port),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_timeout"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, beanstalkd.timeout),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_action_tubes"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, beanstalkd.tubes.action),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_beanstalkd_sessionless_tubes"),         /* directive name */
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, beanstalkd.tubes.sessionless),
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
    {
        ngx_string("nginx_casper_broker_session_cookie_name"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session_cookie_name),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_session_cookie_domain"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session_cookie_domain),
        NULL
    },
    {
        ngx_string("nginx_casper_broker_session_cookie_path"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_casper_broker_module_loc_conf_t, session_cookie_path),
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
    /* gatekeeper */
    {
       ngx_string("nginx_casper_broker_gatekeeper_config_file_uri"),
       NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
       ngx_conf_set_str_slot,
       NGX_HTTP_LOC_CONF_OFFSET,
       offsetof(ngx_http_casper_broker_module_loc_conf_t, gatekeeper.config_file_uri),
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
    NULL,                                            /* create main configuration     */
    NULL,                                            /* init main configuration       */
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
    NULL,                                              /* init master      - NOT CALLED  */
    NULL,                                              /* init module       */
    NULL,                                              /* init process      */ /* ngx_http_casper_broker_module_init_process */
    NULL,                                              /* init thread       */
    NULL,                                              /* exit thread       */
    ngx_http_casper_broker_module_exit_process,        /* exit process      */
    ngx_http_casper_broker_module_exit_master_process, /* exit master       */
    NGX_MODULE_V1_PADDING
};

#ifdef __APPLE__
#pragma mark -
#pragma mark - Module - Implementation
#pragma mark -
#endif

// static bool s_master_initialized_ = false;

/**
 * @brief Called when the master is about to exit.
 *
 * @param a_cycle
 */
static void ngx_http_casper_broker_module_exit_master_process (ngx_cycle_t* a_cycle)
{
    return;
}

///**
// * @brief Called whe the a process has started.
// *
// * @param a_cycle
// */
//static ngx_int_t ngx_http_casper_broker_module_init_process (ngx_cycle_t* a_cycle)
//{
//    ngx_core_conf_t* ccf = (ngx_core_conf_t*) ngx_get_conf(a_cycle->conf_ctx, ngx_core_module);
//
//    const bool master = ( ( ngx_process == NGX_PROCESS_SINGLE ) || ( 1 == ccf->master) );
//
//    const std::string pid_file = std::string(reinterpret_cast<const char*>(ccf->pid.data), ccf->pid.len);
//
//    ngx_log_error(NGX_LOG_ALERT, a_cycle->log, 0,
//                  "Initializing '%s' process w/pid file '%s'...",
//                  master ? "master" : "worker",
//                  pid_file.c_str()
//    );
//
//    if ( true == master && true == s_master_initialized_ ) {
//        return NGX_OK;
//    }
//
//    ngx::casper::broker::Initializer::GetInstance().PreStartup(ccf, master);
//
//    if ( true == master ) {
//        s_master_initialized_ = true;
//    }
//
//    return NGX_OK;
//}

/**
 * @brief Called when a process is about to exit.
 *
 * @param a_cycle
 */
static void ngx_http_casper_broker_module_exit_process (ngx_cycle_t* a_cycle)
{
    return;
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
    conf->service_id                       = ngx_null_string;
    conf->resources_dir                    = ngx_null_string;
    conf->supported_content_types          = ngx_null_string;
    conf->connection_validity              = NGX_CONF_UNSET;
    // ... redis ...
    conf->redis_ip_address                 = ngx_null_string;
    conf->redis_port_number                = NGX_CONF_UNSET;
    conf->redis_database                   = NGX_CONF_UNSET;
    conf->redis_max_conn_per_worker        = NGX_CONF_UNSET;
    // ... postgresql ...
    conf->postgresql_conn_str              = ngx_null_string;
    conf->postgresql_statement_timeout     = NGX_CONF_UNSET;
    conf->postgresql_max_conn_per_worker   = NGX_CONF_UNSET;
    conf->postgresql_post_connect_queries  = ngx_null_string;
    conf->postgresql_min_queries_per_conn  = NGX_CONF_UNSET;
    conf->postgresql_max_queries_per_conn  = NGX_CONF_UNSET;
    // ... curl ...
    conf->curl_max_conn_per_worker         = NGX_CONF_UNSET;
    // ... beanstalk ...
    conf->beanstalkd.host                  = ngx_null_string;
    conf->beanstalkd.port                  = NGX_CONF_UNSET;
    conf->beanstalkd.timeout               = NGX_CONF_UNSET;
    conf->beanstalkd.tubes.action          = ngx_null_string;
    conf->beanstalkd.tubes.sessionless     = ngx_null_string;
    // ... jwt ...
    conf->jwt_iss                          = ngx_null_string;
    conf->jwt_rsa_public_key_uri           = ngx_null_string;
    conf->jwt_allowed_patch_members_set    = ngx_null_string;
    // ... session cookie ...
    conf->session_cookie_name              = ngx_null_string;
    conf->session_cookie_domain            = ngx_null_string;
    conf->session_cookie_path              = ngx_null_string;
    // ... cdn config ...
    conf->cdn.ast                          = ngx_null_string;
    conf->cdn.h2e_map                      = ngx_null_string;
    conf->cdn.redirect.protocol            = ngx_null_string;
    conf->cdn.redirect.host                = ngx_null_string;
    conf->cdn.redirect.port                = NGX_CONF_UNSET;
    conf->cdn.redirect.path                = ngx_null_string;
    conf->cdn.redirect.location            = ngx_null_string;
    conf->cdn.response.content_disposition = ngx_null_string;
    conf->cdn.directories.temporary_prefix = ngx_null_string;
    conf->cdn.directories.archive_prefix   = ngx_null_string;
    // ... gatekeeper ...
    conf->gatekeeper.config_file_uri       = ngx_null_string;
    // ... cc log ...
    conf->cc_log.set                       = NGX_CONF_UNSET;
    conf->cc_log.write_body                = NGX_CONF_UNSET;
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
static char* ngx_http_casper_broker_module_merge_loc_conf (ngx_conf_t* a_cf, void* a_parent, void* a_child)
{
    ngx_http_casper_broker_module_loc_conf_t* prev = (ngx_http_casper_broker_module_loc_conf_t*) a_parent;
    ngx_http_casper_broker_module_loc_conf_t* conf = (ngx_http_casper_broker_module_loc_conf_t*) a_child;
    
    ngx_conf_merge_str_value (conf->location                       , prev->location                       ,            "" );
    ngx_conf_merge_str_value (conf->service_id                     , prev->service_id                     , "development" );
    ngx_conf_merge_str_value (conf->resources_dir                  , prev->resources_dir                  ,            "" );
    ngx_conf_merge_str_value (conf->supported_content_types        , prev->supported_content_types        ,
                              "[\"application/json\",\"application/vnd.api+json\",\"application/text\",\"application/x-www-form-urlencoded\"]"
    );
    ngx_conf_merge_value      (conf->connection_validity           , prev->connection_validity            ,           300 );
    // ... redis ...
    ngx_conf_merge_str_value (conf->redis_ip_address               , prev->redis_ip_address               ,            "" );
    ngx_conf_merge_value     (conf->redis_port_number              , prev->redis_port_number              ,          6379 );
    ngx_conf_merge_value     (conf->redis_database                 , prev->redis_database                 ,            -1 );
    ngx_conf_merge_value     (conf->redis_max_conn_per_worker      , prev->redis_max_conn_per_worker      ,             4 );
    // ... postgresql ...
    ngx_conf_merge_str_value (conf->postgresql_conn_str            , prev->postgresql_conn_str            ,            "" );
    ngx_conf_merge_value     (conf->postgresql_statement_timeout   , prev->postgresql_statement_timeout   ,           300 ); /* in seconds */
    ngx_conf_merge_value     (conf->postgresql_max_conn_per_worker , prev->postgresql_max_conn_per_worker ,             2 );
    ngx_conf_merge_str_value (conf->postgresql_post_connect_queries, prev->postgresql_post_connect_queries,          "[]" );
    ngx_conf_merge_value     (conf->postgresql_min_queries_per_conn, prev->postgresql_min_queries_per_conn,            -1 );
    ngx_conf_merge_value     (conf->postgresql_max_queries_per_conn, prev->postgresql_max_queries_per_conn,            -1 );
    // ... curl ...
    ngx_conf_merge_value     (conf->curl_max_conn_per_worker       , prev->curl_max_conn_per_worker       ,             4 );
    // ... beanstalk ...
    ngx_conf_merge_str_value (conf->beanstalkd.host                , prev->beanstalkd.host                ,   "127.0.0.1" );
    ngx_conf_merge_value     (conf->beanstalkd.port                , prev->beanstalkd.port                ,         11300 );
    ngx_conf_merge_value     (conf->beanstalkd.timeout             , prev->beanstalkd.timeout             ,           0.0 );
    ngx_conf_merge_str_value (conf->beanstalkd.tubes.sessionless   , prev->beanstalkd.tubes.sessionless   ,            "" );
    ngx_conf_merge_str_value (conf->beanstalkd.tubes.action        , prev->beanstalkd.tubes.action        ,            "" );
    // ... jwt ...
    ngx_conf_merge_str_value (conf->jwt_iss                        , prev->jwt_iss                        ,            "" );
    ngx_conf_merge_str_value (conf->jwt_rsa_public_key_uri         , prev->jwt_rsa_public_key_uri         ,            "" );
    ngx_conf_merge_str_value (conf->jwt_allowed_patch_members_set  , prev->jwt_allowed_patch_members_set  ,            "" );
    // ... session cookie ...
    ngx_conf_merge_str_value (conf->session_cookie_name            , prev->session_cookie_name            ,            "" );
    ngx_conf_merge_str_value (conf->session_cookie_domain          , prev->session_cookie_domain          ,            "" );
    ngx_conf_merge_str_value (conf->session_cookie_path            , prev->session_cookie_path            ,            "" );

    // ... redirect config ...
    ngx_conf_merge_str_value (conf->cdn.ast                            , prev->cdn.ast                         ,          "{}" );
    ngx_conf_merge_str_value (conf->cdn.h2e_map                        , prev->cdn.h2e_map                     ,          "{}" );
    ngx_conf_merge_str_value (conf->cdn.redirect.protocol              , prev->cdn.redirect.protocol           ,        "http" );
    ngx_conf_merge_value     (conf->cdn.redirect.port                  , prev->cdn.redirect.port               ,            80 );
    ngx_conf_merge_str_value (conf->cdn.redirect.host                  , prev->cdn.redirect.host               ,   "127.0.0.1" );
    ngx_conf_merge_str_value (conf->cdn.redirect.path                  , prev->cdn.redirect.path               ,    "archive"  );
    ngx_conf_merge_str_value (conf->cdn.redirect.location              , prev->cdn.redirect.location           ,    "redirect" );
    ngx_conf_merge_str_value (conf->cdn.response.content_disposition   , prev->cdn.response.content_disposition, "attachment"  );
    ngx_conf_merge_str_value (conf->cdn.directories.temporary_prefix   , prev->cdn.directories.temporary_prefix, "/tmp"        );
    ngx_conf_merge_str_value (conf->cdn.directories.archive_prefix     , prev->cdn.directories.archive_prefix  , "/tmp"        );
    
    // ... gatekeeper ...
    ngx_conf_merge_str_value (conf->gatekeeper.config_file_uri         , prev->gatekeeper.config_file_uri      ,            "" );

    // ... cc log ...
    ngx_conf_merge_value     (conf->cc_log.set                         , prev->cc_log.set                      ,             0 ); /* 0 - not set */
    ngx_conf_merge_value     (conf->cc_log.write_body                  , prev->cc_log.write_body               ,             1 ); /* 1 - enabled */

    // ... awful code enabler ... 
    ngx_conf_merge_value     (conf->awful_conf.ordered_errors_json_serialization_enabled, prev->awful_conf.ordered_errors_json_serialization_enabled, 0 ); /* 0 - disabled - as it always should be */
    
    // ... done ...
    return (char*) NGX_CONF_OK;
}
