#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>
#include <ngx_http_lua_api.h>
#include <ngx_http_lua_shdict.h>
#include <stdbool.h>
#include "ddebug.h"
//#include "ngx_http_lua_mqtt_module_ipc.h"
#include "ngx_http_lua_mqtt_module.h"

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

ngx_module_t ngx_http_lua_mqtt_module;

ngx_flag_t ngx_http_lua_mqtt_enabled = 0;

static ngx_http_lua_mqtt_main_conf_t *mcf;

ngx_shm_zone_t     *ngx_http_lua_mqtt_global_shm_zone ;


static ngx_int_t ngx_http_lua_mqtt_init(ngx_conf_t *cf);
static void *ngx_http_lua_mqtt_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_lua_mqtt_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_http_lua_mqtt_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_lua_mqtt_init_worker(ngx_cycle_t *cycle);
static void ngx_http_lua_mqtt_exit_master(ngx_cycle_t *cycle);
static void ngx_http_lua_mqtt_exit_worker(ngx_cycle_t *cycle);
static char *ngx_http_lua_mqtt_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_lua_mqtt_init_global_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

static ngx_command_t  ngx_http_lua_mqtt_commands[] = {

    { ngx_string("mqtt_push"),
      NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_lua_mqtt_set_on,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_lua_mqtt_main_conf_t, enabled),
      NULL },
	/* Main directives
	{ ngx_string("mqtt_shared_memory_size"),
		NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
		ngx_http_lua_mqtt_set_shm_size_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		0,
		NULL },*/
	{ ngx_string("mqtt_server_id"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_lua_mqtt_main_conf_t, serverId),
		NULL },

      ngx_null_command
};


static ngx_http_module_t ngx_http_lua_mqtt_ctx = {
    NULL,                           		/* preconfiguration */
    ngx_http_lua_mqtt_init,         		/* postconfiguration */
    ngx_http_lua_mqtt_create_main_conf,		/* create main configuration */
    ngx_http_lua_mqtt_init_main_conf,       /* init main configuration */
    NULL,                           		/* create server configuration */
    NULL,                           		/* merge server configuration */
    NULL,                           		/* create location configuration */
    NULL                            		/* merge location configuration */
};


ngx_module_t ngx_http_lua_mqtt_module = {
    NGX_MODULE_V1,
    &ngx_http_lua_mqtt_ctx,  					/* module context */
    ngx_http_lua_mqtt_commands,					/* module directives */
    NGX_HTTP_MODULE,            				/* module type */
    NULL,   									/* init master */
    NULL,              /* init module */
    ngx_http_lua_mqtt_init_worker,              /* init process */
    NULL,                       				/* init thread */
    NULL,                       				/* exit thread */
    ngx_http_lua_mqtt_exit_worker,             	/* exit process */
    ngx_http_lua_mqtt_exit_master,             	/* exit master */
    NGX_MODULE_V1_PADDING
};

static void *ngx_http_lua_mqtt_create_main_conf(ngx_conf_t *cf){
	ngx_http_lua_mqtt_main_conf_t *mconf = ngx_pcalloc(cf->pool, sizeof(ngx_http_lua_mqtt_main_conf_t));
	if (mconf == NULL) {
		return NULL;
	}
	mconf->serverId.data=NULL;
	mconf->serverId.len=0;
	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_http_lua_mqtt_main_conf_t will be created.");
	//mconf->upstream_names=NGX_CONF_UNSET_PTR;
	mcf = mconf;


	// initialize global shm   ngx_http_lua_mqtt_global_shm_zone
	/*
	size_t size = ngx_align(2 * sizeof(ngx_http_lua_mqtt_global_shm_data_t), ngx_pagesize);
    ngx_shm_zone_t     *shm_zone = ngx_shared_memory_add(cf, &ngx_http_lua_mqtt_global_shm_name, size, &ngx_http_lua_mqtt_module);
    if (shm_zone == NULL) {
        return NULL;
    }
    shm_zone->init = ngx_http_lua_mqtt_init_global_shm_zone;
    shm_zone->data = (void *) 1;
	*/
	return mconf;
}

static char *ngx_http_lua_mqtt_init_main_conf(ngx_conf_t *cf, void *conf){
	ngx_http_lua_mqtt_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_lua_mqtt_module);
	if (mcf == NULL) {
		return NGX_CONF_ERROR;
	}

	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_http_lua_mqtt_main_conf_t will be initialized.");
	/*

	*/
	return NGX_CONF_OK;
}

static ngx_int_t ngx_http_lua_mqtt_init_module(ngx_cycle_t *cycle){
	//ngx_core_conf_t 						*ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

	if (!ngx_http_lua_mqtt_enabled) {
		ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_http_lua_mqtt_module will not be used with this configuration.");
		return NGX_OK;
	}

	// initialize our little IPC
	if(mcf->serverId.data==NULL){
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "ngx_http_lua_mqtt_module serverId no init.");
		return NGX_ERROR;

	}
	//ngx_int_t rc;
	/*
	if ((rc = ngx_http_lua_mqtt_init_ipc(cycle, ccf->worker_processes)) == NGX_OK) {
		ngx_http_lua_mqtt_alert_shutting_down_workers();
	}
*/
	return NGX_ERROR;
}

static ngx_int_t ngx_http_lua_mqtt_init_worker(ngx_cycle_t *cycle)
{
    if (!ngx_http_lua_mqtt_enabled) {
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }
/*
    if ((ngx_http_lua_mqtt_ipc_init_worker()) != NGX_OK) {
        return NGX_ERROR;
    }
*/
    // turn on timer to cleanup memory of old messages and channels
    //ngx_http_lua_mqtt_memory_cleanup_timer_set();

    return NGX_OK;//ngx_http_lua_mqtt_register_worker_message_handler(cycle);
}


static void ngx_http_lua_mqtt_exit_master(ngx_cycle_t *cycle)
{
    if (!ngx_http_lua_mqtt_enabled) {
        return;
    }

    // destroy channel tree in shared memory
    //ngx_http_lua_mqtt_collect_expired_messages_and_empty_channels(1);
    //ngx_http_lua_mqtt_free_memory_of_expired_messages_and_channels(1);
}


static void ngx_http_lua_mqtt_exit_worker(ngx_cycle_t *cycle)
{
    if (!ngx_http_lua_mqtt_enabled) {
        return;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

    //ngx_http_lua_mqtt_cleanup_shutting_down_worker();

    //ngx_http_lua_mqtt_ipc_exit_worker(cycle);
}

static char *
ngx_http_lua_mqtt_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
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
		ngx_http_lua_mqtt_enabled=1;
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

static int
get_serverId(lua_State * L)
{
    ngx_http_request_t  *r;

    r = ngx_http_lua_get_request(L);
	if(mcf->serverId.data==NULL){
		return luaL_error(L, "serverId no configure.");
	}
    lua_pushlstring(L, (const char *) mcf->serverId.data, mcf->serverId.len);
	ngx_log_error(NGX_LOG_NOTICE, r->pool->log,  0,
						 "serverId: \"%s\", len: \"%d\"",
						 mcf->serverId.data, mcf->serverId.len);

    return 1;
}


static int
get_uri(lua_State * L)
{
    ngx_http_request_t  *r;

    r = ngx_http_lua_get_request(L);

    lua_pushlstring(L, (const char *) r->uri.data, r->uri.len);

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
 * ����Topicͨ���
/��������ʾ��Σ�����a/b��a/b/c��
#����ʾƥ��>=0����Σ�����a/#��ƥ��a/��a/b��a/b/c��
������һ��#��ʾƥ�����С�
������ a#��a/#/c��
+����ʾƥ��һ����Σ�����a/+ƥ��a/b��a/c����ƥ��a/b/c��
������һ��+������ģ�a+������a/+/b����
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
ngx_http_lua_shdict_lookup(ngx_shm_zone_t *shm_zone, ngx_uint_t hash,
    u_char *kdata, size_t klen, ngx_http_lua_shdict_node_t **sdp)
{
    ngx_int_t                    rc;
    ngx_time_t                  *tp;
    uint64_t                     now;
    int64_t                      ms;
    ngx_rbtree_node_t           *node, *sentinel;
    ngx_http_lua_shdict_ctx_t   *ctx;
    ngx_http_lua_shdict_node_t  *sd;

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

        sd = (ngx_http_lua_shdict_node_t *) &node->color;

        rc = ngx_memn2cmp(kdata, sd->data, klen, (size_t) sd->key_len);

        if (rc == 0) {
            ngx_queue_remove(&sd->queue);
            ngx_queue_insert_head(&ctx->sh->lru_queue, &sd->queue);

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
    ngx_http_lua_shdict_ctx_t   *ctx;
	ngx_shm_zone_t              *zone;
	ngx_queue_t                 *q, *prev;
	ngx_http_lua_shdict_node_t  *sd;
	ngx_time_t                  *tp;
	int                          n;
	//int                          total = 0;
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

	zone = ngx_http_lua_find_zone(zone_name.data,zone_name.len);
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
		n = ngx_http_lua_shdict_lookup(zone, hash, topic.data, topic.len, &sd);
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

	if (ngx_queue_empty(&ctx->sh->lru_queue)) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        lua_createtable(L, 0, 0);
        return 1;
    }

	lua_createtable(L, 10000, 0);
	q = ngx_queue_last(&ctx->sh->lru_queue);
	while (q != ngx_queue_sentinel(&ctx->sh->lru_queue)) {
        prev = ngx_queue_prev(q);

        sd = ngx_queue_data(q, ngx_http_lua_shdict_node_t, queue);
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


static int luaopen_nginx_mqtt(lua_State * L)
{
    lua_createtable(L, 0, 1);
	lua_pushcfunction(L, get_serverId);
	lua_setfield(L, -2, "get_serverId");
    lua_pushcfunction(L, get_uri);
    lua_setfield(L, -2, "get_uri");
	lua_pushcfunction(L, shared_dict_get2mqtt);
    lua_setfield(L, -2, "shdict_get2mqtt");



    return 1;
}

// shared memory zone initializer
static ngx_int_t
ngx_http_lua_mqtt_init_global_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_slab_pool_t                            *shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
    ngx_http_lua_mqtt_global_shm_data_t     *d;
    int i;

    if (data) { /* zone already initialized */
        shm_zone->data = data;
        ngx_queue_init(&((ngx_http_lua_mqtt_global_shm_data_t *) data)->shm_datas_queue);
        ngx_http_lua_mqtt_global_shm_zone = shm_zone;
        return NGX_OK;
    }

    if ((d = (ngx_http_lua_mqtt_global_shm_data_t *) ngx_slab_alloc(shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    shm_zone->data = d;
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->pid[i] = -1;
    }

    ngx_queue_init(&d->shm_datas_queue);

    ngx_http_lua_mqtt_global_shm_zone = shm_zone;

    return NGX_OK;
}



static ngx_int_t
ngx_http_lua_mqtt_init(ngx_conf_t *cf)
{
	dd("lua_add_package_preload nginx.mqtt");
    if (ngx_http_lua_add_package_preload(cf, "nginx.mqtt",
                                         luaopen_nginx_mqtt)
        != NGX_OK)
    {
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "lua_add_package_preload nginx.mqtt failued");
        return NGX_ERROR;
    }

    return NGX_OK;
}

