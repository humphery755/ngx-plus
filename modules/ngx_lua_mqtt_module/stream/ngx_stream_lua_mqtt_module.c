#include <ngx_core.h>
#include <ngx_stream.h>
#include <ngx_channel.h>
#include <ngx_stream_lua_api.h>
#include <ngx_stream_lua_shdict.h>
#include <stdbool.h>
#include "ddebug.h"
//#include <ngx_stream_lua_util.h>

#include "ngx_stream_lua_mqtt_module.h"


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

ngx_module_t ngx_stream_lua_mqtt_module;

ngx_flag_t ngx_stream_lua_mqtt_enabled = 0;

static ngx_stream_lua_mqtt_main_conf_t *mcf;

//ngx_shm_zone_t     *ngx_stream_lua_mqtt_global_shm_zone ;


static ngx_int_t ngx_stream_lua_mqtt_init(ngx_conf_t *cf);
static void *ngx_stream_lua_mqtt_create_main_conf(ngx_conf_t *cf);
static char *ngx_stream_lua_mqtt_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_stream_lua_mqtt_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_stream_lua_mqtt_init_worker(ngx_cycle_t *cycle);
static void ngx_stream_lua_mqtt_exit_master(ngx_cycle_t *cycle);
static void ngx_stream_lua_mqtt_exit_worker(ngx_cycle_t *cycle);
static char *ngx_stream_lua_mqtt_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_stream_lua_mqtt_init_global_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

//static ngx_str_t    ngx_stream_lua_mqtt_global_shm_name = ngx_string("lua_mqtt_module_global");


static ngx_command_t  ngx_stream_lua_mqtt_commands[] = {

    { ngx_string("mqtt_push"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_1MORE,
      ngx_stream_lua_mqtt_set_on,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_stream_lua_mqtt_main_conf_t, enabled),
      NULL },

    { ngx_string("mqtt_server_id"),
        NGX_STREAM_MAIN_CONF|NGX_CONF_1MORE,
        ngx_conf_set_str_slot,
        NGX_STREAM_MAIN_CONF_OFFSET,
        offsetof(ngx_stream_lua_mqtt_main_conf_t, serverId),
        NULL },

      ngx_null_command
};


static ngx_stream_module_t ngx_stream_lua_mqtt_ctx = {
    ngx_stream_lua_mqtt_init,                 /* postconfiguration */
    ngx_stream_lua_mqtt_create_main_conf,     /* create main configuration */
    ngx_stream_lua_mqtt_init_main_conf,       /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL                                    /* merge server configuration */
};


ngx_module_t  ngx_stream_lua_mqtt_module = {
    NGX_MODULE_V1,
    &ngx_stream_lua_mqtt_ctx,            /* module context */
    ngx_stream_lua_mqtt_commands,          /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,         							/* init module */
    ngx_stream_lua_mqtt_init_worker,        /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_stream_lua_mqtt_exit_worker,         /* exit process */
    ngx_stream_lua_mqtt_exit_master,         /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *ngx_stream_lua_mqtt_create_main_conf(ngx_conf_t *cf){
	ngx_stream_lua_mqtt_main_conf_t *mconf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_lua_mqtt_main_conf_t));
	if (mconf == NULL) {
		return NULL;
	}
	mconf->serverId.data=NULL;
	mconf->serverId.len=0;
	mconf->enabled=NGX_CONF_UNSET;
	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_stream_lua_mqtt_main_conf_t will be created.");
	//mconf->upstream_names=NGX_CONF_UNSET_PTR;
	mcf = mconf;


	/* initialize global shm   ngx_stream_lua_mqtt_global_shm_zone
	size_t size = ngx_align(2 * sizeof(ngx_stream_lua_mqtt_global_shm_data_t), ngx_pagesize);
    ngx_shm_zone_t     *shm_zone = ngx_shared_memory_add(cf, &ngx_stream_lua_mqtt_global_shm_name, size, &ngx_stream_lua_mqtt_module);
    if (shm_zone == NULL) {
        return NULL;
    }
    shm_zone->init = ngx_stream_lua_mqtt_init_global_shm_zone;
    shm_zone->data = (void *) 1;
	*/
	return mconf;
}

static char *ngx_stream_lua_mqtt_init_main_conf(ngx_conf_t *cf, void *conf){
	ngx_stream_lua_mqtt_main_conf_t    *mcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_lua_mqtt_module);
	if (mcf == NULL) {
		return NGX_CONF_ERROR;
	}

	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_stream_lua_mqtt_main_conf_t will be initialized.");
	/*

	*/
	return NGX_CONF_OK;
}

static void *
ngx_stream_mqtt_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_mqtt_srv_conf_t  *cscf;

    cscf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_mqtt_srv_conf_t));
    if (cscf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     cscf->handler = NULL;
     *     cscf->error_log = NULL;
     */

    cscf->serverId.data = NGX_CONF_UNSET;
	cscf->serverId.len = NGX_CONF_UNSET;
	cscf->enabled=NGX_CONF_UNSET;

    return cscf;
}


static char *
ngx_stream_mqtt_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_stream_mqtt_srv_conf_t *prev = parent;
    ngx_stream_mqtt_srv_conf_t *conf = child;

    if (conf->enabled== 0) {
		/*
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no enabled for server in %s:%ui",
                      cf->conf_file->file_name, cf->conf_file->line);
                      */
        return NGX_CONF_ERROR;
    }

    if (conf->serverId.len== NGX_CONF_UNSET) {
		/*
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no enabled for server in %s:%ui",
                      cf->conf_file->file_name, cf->conf_file->line);
                      */
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->enabled, prev->enabled, 0);

    return NGX_CONF_OK;
}


static ngx_int_t ngx_stream_lua_mqtt_init_worker(ngx_cycle_t *cycle)
{
	ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "ngx_stream_lua_mqtt_init_worker.");
    if (!ngx_stream_lua_mqtt_enabled) {
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }
/*
    if ((ngx_stream_lua_mqtt_ipc_init_worker()) != NGX_OK) {
        return NGX_ERROR;
    }
*/
    // turn on timer to cleanup memory of old messages and channels
    //ngx_stream_lua_mqtt_memory_cleanup_timer_set();

    return NGX_OK;//ngx_stream_lua_mqtt_register_worker_message_handler(cycle);
}


static void ngx_stream_lua_mqtt_exit_master(ngx_cycle_t *cycle)
{
	ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_stream_lua_mqtt_exit_master.");
    if (!ngx_stream_lua_mqtt_enabled) {
        return;
    }

    // destroy channel tree in shared memory
    //ngx_stream_lua_mqtt_collect_expired_messages_and_empty_channels(1);
    //ngx_stream_lua_mqtt_free_memory_of_expired_messages_and_channels(1);
}


static void ngx_stream_lua_mqtt_exit_worker(ngx_cycle_t *cycle)
{
	ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_stream_lua_mqtt_exit_worker");
    if (!ngx_stream_lua_mqtt_enabled) {
        return;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

    //ngx_stream_lua_mqtt_cleanup_shutting_down_worker();

    //ngx_stream_lua_mqtt_ipc_exit_worker(cycle);
}

static char *
ngx_stream_lua_mqtt_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t        *value;
    ngx_conf_post_t  *post;

	ngx_flag_t 		*fp;
	fp=NULL;
	if(p!=NULL){
	    fp = (ngx_flag_t *) (p + cmd->offset);

	    if (*fp != NGX_CONF_UNSET) {
	        return "is duplicate";
	    }
	}
    value = cf->args->elts;

    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        mcf->enabled   = 1;
		ngx_stream_lua_mqtt_enabled=1;
    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        mcf->enabled =  0;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                     "invalid value \"%s\" in \"%s\" directive, "
                     "it must be \"on\" or \"off\"",
                     value[1].data, cmd->name.data);
        return NGX_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, fp);
    }

    return NGX_CONF_OK;
}

#define ngx_stream_lua_session_key  "__ngx_sess"

ngx_inline ngx_stream_session_t *
ngx_stream_lua_get_session(lua_State *L)
{
    ngx_stream_session_t    *s;

    lua_getglobal(L, ngx_stream_lua_session_key);
    s = lua_touserdata(L, -1);
    lua_pop(L, 1);

    return s;
}


static int
get_serverId(lua_State * L)
{
    ngx_stream_session_t  *r;

    r = ngx_stream_lua_get_session(L);
	if(mcf->serverId.data==NULL){
		return luaL_error(L, "serverId no configure.");
	}
    lua_pushlstring(L, (const char *) mcf->serverId.data, mcf->serverId.len);
	ngx_log_error(NGX_LOG_NOTICE, r->connection->pool->log,  0,
						 "serverId: \"%s\", len: \"%d\"",
						 mcf->serverId.data, mcf->serverId.len);

    return 1;
}



bool ngx_mqtt_topic_is_expression(u_char *topic){
	u_char c;
	size_t i;
	size_t topLen=ngx_strlen(topic);
	for(i=0;i<topLen;i++){
		c = topic[i];
		switch(c){
		case '+':
		case '#':
			return true;
		default:

			break;
		}
	}
	return false;
}

/**
 * 关于Topic通配符
/：用来表示层次，比如a/b，a/b/c。
#：表示匹配>=0个层次，比如a/#就匹配a/，a/b，a/b/c。
单独的一个#表示匹配所有。
不允许 a#和a/#/c。
+：表示匹配一个层次，例如a/+匹配a/b，a/c，不匹配a/b/c。
单独的一个+是允许的，a+不允许，a/+/b允许
 * @param topic
 * @param set
 * @return
 */
bool ngx_mqtt_topic_wildcard_match(u_char *topic, size_t topLen, u_char *search,size_t seachLen){
	u_char c,oc;
	bool flg;
	if(topLen<seachLen)return false;
	flg = true;
	size_t j=0,i;
	for(i=0;i<seachLen && flg;i++){
		c = search[i];
		switch(c){
		case '+':
			do{
				oc = topic[j++];
			}while(oc!='/'&&j<topLen);
			if(j<topLen){
				if(i>=seachLen-1){
					flg=false;
					break;
				}
				j--;
			}else{
				return true;
			}
			break;
		case '#':
			return true;
		case '/':
			oc = topic[j++];
			if(c!=oc){
				if(i-j==2){
					return true;
				}
				flg=false;
			}
			break;
		default:
			oc = topic[j++];
			if(c!=oc){
				flg=false;
				break;
			}
			break;
		}
	}
	if(flg && topLen==j)
		return true;
	return false;
}

ngx_inline static ngx_int_t
ngx_stream_lua_shdict_lookup(ngx_shm_zone_t *shm_zone, ngx_uint_t hash,
    u_char *kdata, size_t klen, ngx_stream_lua_shdict_node_t **sdp)
{
    ngx_int_t                    rc;
    ngx_time_t                  *tp;
    uint64_t                     now;
    int64_t                      ms;
    ngx_rbtree_node_t           *node, *sentinel;
    ngx_stream_lua_shdict_ctx_t   *ctx;
    ngx_stream_lua_shdict_node_t  *sd;

    ctx = shm_zone->data;

    node = ctx->sh->rbtree.root;
    sentinel = ctx->sh->rbtree.sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        sd = (ngx_stream_lua_shdict_node_t *) &node->color;

        rc = ngx_memn2cmp(kdata, sd->data, klen, (size_t) sd->key_len);

        if (rc == 0) {
            ngx_queue_remove(&sd->queue);
            ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

            *sdp = sd;

            dd("node expires: %lld", (long long) sd->expires);

            if (sd->expires != 0) {
                tp = ngx_timeofday();

                now = (uint64_t) tp->sec * 1000 + tp->msec;
                ms = sd->expires - now;

                dd("time to live: %lld", (long long) ms);

                if (ms < 0) {
                    dd("node already expired");
                    return NGX_DONE;
                }
            }

            return NGX_OK;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    *sdp = NULL;

    return NGX_DECLINED;
}

static int shared_dict_get2mqtt(lua_State * L)
{
    ngx_stream_lua_shdict_ctx_t   *ctx;
	ngx_shm_zone_t              *zone;
	ngx_queue_t                 *q, *prev;
	ngx_stream_lua_shdict_node_t  *sd;
	ngx_time_t                  *tp;
	int                          n;
	int                          total = 0;
	uint64_t                     now;
	ngx_str_t                    zone_name,topic;
	double						 *num;
	bool						 is_expression;
	uint32_t                     hash;

    n = lua_gettop(L);


    if (n != 2) {
        return luaL_error(L, "expecting 2 arguments, "
                          "but only seen %d", n);
    }

    //zone = lua_touserdata(L, 1);
    //if (zone == NULL) {
        //return luaL_error(L, "bad \"zone\" argument");
    //}
    zone_name.data = (u_char *) luaL_checklstring(L, 1, &zone_name.len);

    if (zone_name.len == 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "empty zone_name");
        return 2;
    }

	topic.data = (u_char *) luaL_checklstring(L, 2, &topic.len);

    if (topic.len == 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "empty topic");
        return 2;
    }

	zone = ngx_stream_lua_find_zone(zone_name.data,zone_name.len);
    if (zone == NULL) {
        return luaL_error(L, "not found \"zone\" by argument: %s",zone_name.data);
    }

	ctx = zone->data;
	if (ctx == NULL) {
        return luaL_error(L, "bad \"ctx\" argument");
    }

	tp = ngx_timeofday();

    now = (uint64_t) tp->sec * 1000 + tp->msec;


	for(n=topic.len-1;n>=0;n--){
		switch(topic.data[n]){
			case '+':
			case '#':
				n=-1;
				is_expression=true;
				break;
		}
	}

	if(!is_expression)
		hash = ngx_crc32_short(topic.data, topic.len);

	//ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "is_expression %d,topic: %s",is_expression,topic.data);

	ngx_shmtx_lock(&ctx->shpool->mutex);
	if(!is_expression){
		n = ngx_stream_lua_shdict_lookup(zone, hash, topic.data, topic.len, &sd);
		ngx_shmtx_unlock(&ctx->shpool->mutex);
		if (n == NGX_OK) {
			lua_createtable(L, 1, 0);
			lua_pushlstring(L, (const char *)sd->data, sd->key_len);
            num = (double *) (sd->data+sd->key_len);
			lua_pushnumber(L, *num);
            lua_rawset(L, -3);
		}else{
			lua_createtable(L, 0, 0);
		}

		return 1;
	}

	if (ngx_queue_empty(&ctx->sh->queue)) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        lua_createtable(L, 0, 0);
        return 1;
    }

	lua_createtable(L, 10000, 0);
	total = 0;
	q = ngx_queue_last(&ctx->sh->queue);
	while (q != ngx_queue_sentinel(&ctx->sh->queue)) {
        prev = ngx_queue_prev(q);

        sd = ngx_queue_data(q, ngx_stream_lua_shdict_node_t, queue);
		if(ngx_mqtt_topic_wildcard_match(sd->data, sd->key_len,topic.data,topic.len)){
	        if (sd->expires == 0 || sd->expires > now) {
	            lua_pushlstring(L, (const char *)sd->data, sd->key_len);
	            num = (double *) (sd->data+sd->key_len);
				lua_pushnumber(L, *num);
	            //lua_rawseti(L, -3);//, ++total);
	            lua_rawset(L, -3);
	        }
		}
        q = prev;
    }
	ngx_shmtx_unlock(&ctx->shpool->mutex);
    return 1;
}


static int ipc_test(lua_State * L)
{
	ngx_stream_session_t  *r;

    r = ngx_stream_lua_get_session(L);

  //ngx_rbtree_init(&tree, &sentinel, ngx_string_rbtree_insert_value);

	//ngx_stream_lua_mqtt_channel_t channel;

	//ngx_stream_lua_mqtt_msg_t msg;
	//ngx_stream_lua_mqtt_broadcast(&channel,&msg,r->pool->log,mcf);
	return 0;
}
static int luaopen_nginx_mqtt(lua_State * L)
{
	int         rc;

    /* new coroutine table */
    lua_createtable(L, 0 /* narr */, 14 /* nrec */);

    /* get old coroutine table */
    lua_getglobal(L, "test");
    lua_createtable(L, 0, 1);
	lua_pushcfunction(L, get_serverId);
	lua_setfield(L, -2, "get_serverId");
	lua_pushcfunction(L, shared_dict_get2mqtt);
    lua_setfield(L, -2, "shdict_get2mqtt");
	lua_pushcfunction(L, ipc_test);
	lua_setfield(L, -2, "ipc_test");


    return 1;
}



ngx_int_t
_ngx_stream_lua_add_package_preload(ngx_conf_t *cf, const char *package,lua_CFunction func)
{
	lua_State *L;
	ngx_stream_lua_main_conf_t *lmcf;
	ngx_stream_lua_preload_hook_t *hook;
	lmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_lua_module);
	L = lmcf->lua;
	if (L) {
		lua_getglobal(L, "package");
		lua_getfield(L, -1, "preload");
		lua_pushcfunction(L, func);
		lua_setfield(L, -2, package);
		lua_pop(L, 2);
		return NGX_OK;
	}
	/* L == NULL */
	if (lmcf->preload_hooks == NULL) {
		lmcf->preload_hooks =
		ngx_array_create(cf->pool, 4,
		sizeof(ngx_stream_lua_preload_hook_t));
		if (lmcf->preload_hooks == NULL) {
			return NGX_ERROR;
		}
	}
	hook = ngx_array_push(lmcf->preload_hooks);
	if (hook == NULL) {
		return NGX_ERROR;
	}
	hook->package = (u_char *) package;
	hook->loader = func;
	return NGX_OK;
}

static ngx_int_t
ngx_stream_lua_mqtt_init(ngx_conf_t *cf)
{
	dd("lua_add_package_preload nginx.mqtt");
	//ngx_stream_lua_main_conf_t *lmcf;
    ////lmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_lua_module);
	//luaopen_nginx_mqtt(lmcf->lua);

    if (_ngx_stream_lua_add_package_preload(cf, "nginx.mqtt",
                                         luaopen_nginx_mqtt)
        != NGX_OK)
    {
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "lua_add_package_preload nginx.mqtt failued");
        return NGX_ERROR;
    }

    return NGX_OK;
}

