#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>
#include <ngx_stream_lua_api.h>
#include <ngx_stream_lua_shdict.h>
#include <ndk_conf_file.h>
#include <stdbool.h>
#include "ddebug.h"
#include "ngx_lua_mqtt_kit_module.h"

/**
--- config
    location = /test {
        content_by_lua '
            local example = require "nginx.mqtt"
            ngx.say(example.get_uri())
        ';
    }
--- request
*/

static ngx_flag_t ngx_http_mqtt_kit_enabled = 0;


ngx_module_t ngx_stream_lua_mqtt_kit_module;


static ngx_int_t ngx_lua_mqtt_kit_init(ngx_conf_t *cf);
static void *ngx_lua_mqtt_kit_create_main_conf(ngx_conf_t *cf);
static char *ngx_lua_mqtt_kit_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_lua_mqtt_kit_init_worker(ngx_cycle_t *cycle);
static void ngx_lua_mqtt_kit_exit_master(ngx_cycle_t *cycle);
static void ngx_lua_mqtt_kit_exit_worker(ngx_cycle_t *cycle);


static ngx_command_t  ngx_lua_mqtt_kit_commands[] = {
	{ ngx_string("disable_accept_worker_id"),
		NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_STREAM_MAIN_CONF_OFFSET,
		offsetof(ngx_lua_mqtt_kit_main_conf_t, workerId),
		NULL },

      ngx_null_command
};


static ngx_stream_module_t ngx_lua_mqtt_kit_ctx = {
#if (nginx_version >= 1011002)
    NULL,                                  /* preconfiguration */
#endif
    ngx_lua_mqtt_kit_init,         		/* postconfiguration */
    ngx_lua_mqtt_kit_create_main_conf,		/* create main configuration */
    ngx_lua_mqtt_kit_init_main_conf,       /* init main configuration */
    NULL,                           		/* create server configuration */
    NULL	                          		/* merge server configuration */
};


ngx_module_t ngx_stream_lua_mqtt_kit_module = {
    NGX_MODULE_V1,
    &ngx_lua_mqtt_kit_ctx,  					/* module context */
    ngx_lua_mqtt_kit_commands,					/* module directives */
    NGX_STREAM_MODULE,            				/* module type */
    NULL,   									/* init master */
    NULL,              /* init module */
    ngx_lua_mqtt_kit_init_worker,              /* init process */
    NULL,                       				/* init thread */
    NULL,                       				/* exit thread */
    ngx_lua_mqtt_kit_exit_worker,             	/* exit process */
    ngx_lua_mqtt_kit_exit_master,             	/* exit master */
    NGX_MODULE_V1_PADDING
};

static void *ngx_lua_mqtt_kit_create_main_conf(ngx_conf_t *cf){
	ngx_lua_mqtt_kit_main_conf_t *mconf = ngx_pcalloc(cf->pool, sizeof(ngx_lua_mqtt_kit_main_conf_t));
	if (mconf == NULL) {
		return NULL;
	}
	mconf->workerId=-1;
	ngx_log_error(NGX_LOG_INFO, cf->log, 0, "ngx_lua_mqtt_kit_main_conf_t will be created.");

	return mconf;
}

static char *ngx_lua_mqtt_kit_init_main_conf(ngx_conf_t *cf, void *conf){
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return NGX_CONF_ERROR;
	}

	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_lua_mqtt_kit_main_conf_t will be initialized.");
	/*

	*/
	ngx_http_mqtt_kit_enabled =1;
	return NGX_CONF_OK;
}


static void ngx_lua_mqtt_kit_exit_master(ngx_cycle_t *cycle)
{
}


static void ngx_lua_mqtt_kit_exit_worker(ngx_cycle_t *cycle)
{
	if(!ngx_http_mqtt_kit_enabled)return NGX_OK;
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_stream_cycle_get_module_main_conf(cycle, ngx_stream_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return;
	}
    if(mcf->workerId!=ngx_worker){
        return ;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

}



ngx_inline static ngx_int_t
ngx_disable_accept_events(ngx_cycle_t *cycle)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (!c->read->active) {
            continue;
        }
		/*
        if (ngx_event_flags & NGX_USE_RTSIG_EVENT) {
            if (ngx_del_conn(c, NGX_DISABLE_EVENT) == NGX_ERROR) {
                return NGX_ERROR;
            }

        } else {
            if (ngx_del_event(c->read, NGX_READ_EVENT, NGX_DISABLE_EVENT)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }
        }
        */
    }

    return NGX_OK;
}

static ngx_int_t ngx_lua_mqtt_kit_init_worker(ngx_cycle_t *cycle)
{
	if(!ngx_http_mqtt_kit_enabled)return NGX_OK;
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_stream_cycle_get_module_main_conf(cycle, ngx_stream_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return NGX_ERROR;
	}
    if(mcf->workerId!=ngx_worker){
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }
	ngx_log_error(NGX_LOG_DEBUG, cycle->log, 0, "disable workerId: %d.",mcf->workerId);
	ngx_close_listening_sockets(cycle);
	return NGX_OK;
    //return ngx_disable_accept_events(cycle);
}



static int ipc_test(lua_State * L)
{
	ngx_http_request_t  *r;

    r = ngx_http_lua_get_request(L);

	return 0;
}
static int luaopen_nginx_mqtt(lua_State * L)
{
    lua_createtable(L, 0, 1);
	lua_pushcfunction(L, ipc_test);
	lua_setfield(L, -2, "ipc_test");


    return 1;
}

static ngx_int_t
ngx_lua_mqtt_kit_init(ngx_conf_t *cf)
{
	if(!ngx_http_mqtt_kit_enabled)return NGX_OK;
	/*
	dd("lua_add_package_preload nginx.mqtt_kit");
    if (ngx_http_lua_add_package_preload(cf, "nginx.mqtt_kit",
                                         luaopen_nginx_mqtt)
        != NGX_OK)
    {
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "lua_add_package_preload nginx.mqtt_kit failued");
        return NGX_ERROR;
    }
	*/
    return NGX_OK;
}

int ngx_lua_utility_indexOf(const u_char *str1,const u_char *str2)
{
	u_char *p;
	int i=0;
	p=(u_char *)str1;
	p=(u_char *)ngx_strstr(str1,str2);
	if(p==NULL)
		return -1;
	else{
		while(str1!=p)
		{
			str1++;
			i++;
		}
	}
	return i;
}


