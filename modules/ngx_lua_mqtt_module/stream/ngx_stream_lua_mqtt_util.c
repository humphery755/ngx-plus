#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>
#include "ngx_stream_lua_api.h"
#include "ngx_stream_lua_shdict.h"
#include <stdbool.h>
#include "ddebug.h"
#include "ngx_stream_lua_socket_tcp.h"
#include "ngx_stream_lua_mqtt_module.h"
#include "mqtt_protocol.h"

#define INT_MAX_RING 2147480000
// 2147483647;
#define INT_MAX_RING1 INT_MAX_RING/3
#define INT_MAX_RING2 INT_MAX_RING1+INT_MAX_RING1


static ngx_inline int ngx_stream_lua_shdict_expire(ngx_stream_lua_shdict_ctx_t *ctx, ngx_uint_t n)
{
    ngx_time_t                  *tp;
    uint64_t                     now;
    ngx_queue_t                 *q;
    int64_t                      ms;
    ngx_rbtree_node_t           *node;
    ngx_stream_lua_shdict_node_t  *sd;
    int                          freed = 0;

    tp = ngx_timeofday();

    now = (uint64_t) tp->sec * 1000 + tp->msec;

    /*
     * n == 1 deletes one or two expired entries
     * n == 0 deletes oldest entry by force
     *        and one or two zero rate entries
     */

    while (n < 3) {

        if (ngx_queue_empty(&ctx->sh->queue)) {
            return freed;
        }

        q = ngx_queue_last(&ctx->sh->queue);

        sd = ngx_queue_data(q, ngx_stream_lua_shdict_node_t, queue);

        if (n++ != 0) {

            if (sd->expires == 0) {
                return freed;
            }

            ms = sd->expires - now;
            if (ms > 0) {
                return freed;
            }
        }

        ngx_queue_remove(q);

        node = (ngx_rbtree_node_t *)
                   ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));

        ngx_rbtree_delete(&ctx->sh->rbtree, node);

        ngx_slab_free_locked(ctx->shpool, node);

        freed++;
    }

    return freed;
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


#define LUA_SHDICT_ADD         		0x0001
#define LUA_SHDICT_REPLACE   	  	0x0002
#define LUA_SHDICT_SAFE_STORE  		0x0004

int
ngx_mqtt_shdict_update_offset(ngx_shm_zone_t *zone,ngx_str_t *key,ngx_str_t *value,int flags)
{
    int                          i, n;
    uint32_t                     hash;
    ngx_int_t                    rc;
    ngx_stream_lua_shdict_ctx_t   *ctx;
    ngx_stream_lua_shdict_node_t  *sd;
    lua_Number                   exptime = 60*30;
    u_char                      *p;
    ngx_rbtree_node_t           *node;
    ngx_time_t                  *tp;
	double 						*old_val,*new_val;
                         /* indicates whether to foricibly override other
                          * valid entries */
    int32_t                      user_flags = 0;

    ctx = zone->data;
	//ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,"topic=%s,%d",key->data,key->len);
     if (key->len == 0 || key->len > 65535) {
        //lua_pushliteral(L, "key too long");
        return 2;
    }

    hash = ngx_crc32_short(key->data, key->len);

    ngx_shmtx_lock(&ctx->shpool->mutex);

    ngx_stream_lua_shdict_expire(ctx, 1);

	rc = ngx_stream_lua_shdict_lookup(zone, hash, key->data, key->len, &sd);

    dd("shdict lookup returned %d", (int) rc);

    if (flags & LUA_SHDICT_REPLACE) {

 		if (rc == NGX_OK || rc == NGX_DONE) {
			old_val=(double*)(sd->data+sd->key_len);
			new_val=(double*) value->data;
			if((*old_val)>INT_MAX_RING2 && (*new_val)<(INT_MAX_RING1+1)){
				goto replace;
			}else if((*old_val)>=(*new_val)){
				//ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,"new_val=%f,<=,old_val=%f",*new_val,*old_val);
				ngx_shmtx_unlock(&ctx->shpool->mutex);
            	return NGX_OK;
			}
            dd("go to replace");
            goto replace;
        }

        /* rc == NGX_DECLINED */

        dd("go to insert");
        goto insert;

        if (rc != NGX_OK) {
            goto insert;
        }

        /* rc == NGX_OK */

        goto replace;
    }

    if (flags & LUA_SHDICT_ADD) {

        if (rc == NGX_OK) {
            ngx_shmtx_unlock(&ctx->shpool->mutex);
            //lua_pushliteral(L, "exists");
            return NGX_OK;
        }

        /* rc == NGX_DECLINED */

        dd("go to insert");
        goto insert;
    }

    if (rc == NGX_OK) {
		goto remove;
    }
	return NGX_OK;
replace:

        if (value->data && value->len == (size_t) sd->value_len) {

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                           "lua shared dict set: found old entry and value "
                           "size matched, reusing it");

            ngx_queue_remove(&sd->queue);
            ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

            sd->key_len = (u_short) key->len;

            if (exptime > 0) {
                tp = ngx_timeofday();
                sd->expires = (uint64_t) tp->sec * 1000 + tp->msec
                              + (uint64_t) (exptime * 1000);

            } else {
                sd->expires = 0;
            }

            sd->user_flags = user_flags;
            sd->value_len = (uint32_t) value->len;
            dd("setting value type to %d", LUA_TNUMBER);
            sd->value_type = (uint8_t) LUA_TNUMBER;
            p = ngx_copy(sd->data, key->data, key->len);
            ngx_memcpy(p, value->data, value->len);
            ngx_shmtx_unlock(&ctx->shpool->mutex);
			return NGX_OK;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                       "lua shared dict set: found old entry bug value size "
                       "NOT matched, removing it first");

remove:

        ngx_queue_remove(&sd->queue);

        node = (ngx_rbtree_node_t *)
                   ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));
        ngx_rbtree_delete(&ctx->sh->rbtree, node);
        ngx_slab_free_locked(ctx->shpool, node);


insert:

    /* rc == NGX_DECLINED or value size unmatch */

    if (value->data == NULL) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushnil(L);
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "lua shared dict set: creating a new entry");

    n = offsetof(ngx_rbtree_node_t, color)
        + offsetof(ngx_stream_lua_shdict_node_t, data)
        + key->len
        + value->len;

    node = ngx_slab_alloc_locked(ctx->shpool, n);

    if (node == NULL) {

        if (flags & LUA_SHDICT_SAFE_STORE) {
            ngx_shmtx_unlock(&ctx->shpool->mutex);
            //lua_pushliteral(L, "no memory");
            return NGX_ERROR;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                       "lua shared dict set: overriding non-expired items "
                       "due to memory shortage for entry \"%V\"", &key);

        for (i = 0; i < 30; i++) {
            if (ngx_stream_lua_shdict_expire(ctx, 0) == 0) {
                break;
            }


            node = ngx_slab_alloc_locked(ctx->shpool, n);
            if (node != NULL) {
                goto allocated;
            }
        }

        ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushliteral(L, "no memory");
        return NGX_ERROR;
    }

allocated:

    sd = (ngx_stream_lua_shdict_node_t *) &node->color;

    node->key = hash;
    sd->key_len = (u_short) key->len;

    if (exptime > 0) {
        tp = ngx_timeofday();
        sd->expires = (uint64_t) tp->sec * 1000 + tp->msec
                      + (uint64_t) (exptime * 1000);
    } else {
        sd->expires = 0;
    }

    sd->user_flags = user_flags;
    sd->value_len = (uint32_t) value->len;
    dd("setting value type to %d", LUA_TNUMBER);

    sd->value_type = (uint8_t) LUA_TNUMBER;

    p = ngx_copy(sd->data, key->data, key->len);
    ngx_memcpy(p, value->data, value->len);

    ngx_rbtree_insert(&ctx->sh->rbtree, node);
    ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);
    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return NGX_OK;
}

int ngx_mqtt_shdict_store(ngx_shm_zone_t *zone,ngx_str_t *key,ngx_str_t *value)
{
    int                          i, n;
    uint32_t                     hash;
    ngx_int_t                    rc;
    ngx_stream_lua_shdict_ctx_t   *ctx;
    ngx_stream_lua_shdict_node_t  *sd;
    lua_Number                   exptime = 60*30;
    u_char                      *p;
    ngx_rbtree_node_t           *node;
    ngx_time_t                  *tp;
	//double 						*old_val,*new_val;
                         /* indicates whether to foricibly override other
                          * valid entries */
    int32_t                      user_flags = 0;

    ctx = zone->data;
	//ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,"topic=%s,%d",key->data,key->len);
    if (key->len == 0) {
        //lua_pushliteral(L, "empty key");
        return 2;
    }

    if (key->len > 65535) {
        //lua_pushliteral(L, "key too long");
        return 2;
    }

    hash = ngx_crc32_short(key->data, key->len);

    ngx_shmtx_lock(&ctx->shpool->mutex);

    ngx_stream_lua_shdict_expire(ctx, 1);

	rc = ngx_stream_lua_shdict_lookup(zone, hash, key->data, key->len, &sd);

    dd("shdict lookup returned %d", (int) rc);

    if (rc == NGX_OK) {
        //ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushliteral(L, "exists");
        //return NGX_OK;
		goto replace;
    }


    /* rc == NGX_DECLINED or value size unmatch */

    if (value->data == NULL) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushnil(L);
        return NGX_ERROR;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "lua shared dict set: creating a new entry");

    n = offsetof(ngx_rbtree_node_t, color)
        + offsetof(ngx_stream_lua_shdict_node_t, data)
        + key->len
        + value->len;

    node = ngx_slab_alloc_locked(ctx->shpool, n);

    if (node == NULL) {

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                       "lua shared dict set: overriding non-expired items "
                       "due to memory shortage for entry \"%V\"", &key);

        for (i = 0; i < 30; i++) {
            if (ngx_stream_lua_shdict_expire(ctx, 0) == 0) {
                break;
            }


            node = ngx_slab_alloc_locked(ctx->shpool, n);
            if (node != NULL) {
                goto allocated;
            }
        }

        ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushliteral(L, "no memory");
        return NGX_ERROR;
    }


allocated:

    sd = (ngx_stream_lua_shdict_node_t *) &node->color;

    node->key = hash;
    sd->key_len = (u_short) key->len;

    if (exptime > 0) {
        tp = ngx_timeofday();
        sd->expires = (uint64_t) tp->sec * 1000 + tp->msec
                      + (uint64_t) (exptime * 1000);
    } else {
        sd->expires = 0;
    }

    sd->user_flags = user_flags;
    sd->value_len = (uint32_t) value->len;

    sd->value_type = (uint8_t) LUA_TSTRING;

    p = ngx_copy(sd->data, key->data, key->len);
    ngx_memcpy(p, value->data, value->len);

    ngx_rbtree_insert(&ctx->sh->rbtree, node);
    ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);
    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return NGX_OK;

replace:

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
				   "lua shared dict set: found old entry and value "
				   "size matched, reusing it");

	ngx_queue_remove(&sd->queue);
	ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

	sd->key_len = (u_short) key->len;

	if (exptime > 0) {
		tp = ngx_timeofday();
		sd->expires = (uint64_t) tp->sec * 1000 + tp->msec
					  + (uint64_t) (exptime * 1000);

	} else {
		sd->expires = 0;
	}

	sd->user_flags = user_flags;
	sd->value_len = (uint32_t) value->len;
	dd("setting value type to %d", LUA_TSTRING);
	sd->value_type = (uint8_t) LUA_TSTRING;
	p = ngx_copy(sd->data, key->data, key->len);
	ngx_memcpy(p, value->data, value->len);
	ngx_shmtx_unlock(&ctx->shpool->mutex);
	return NGX_OK;

}

int ngx_ffi_mqtt_offset_process(u_char *zoneName,u_char *_data,int d_len){
    u_char             					*p;
	int									n;
	ngx_str_t 							key,value,msg_key,msg_value,data,zName;
	ngx_shm_zone_t 						*zone,*store_zone;
	double								num;
	uint8_t 							cmd,qos,remaining_count;
	uint32_t 							remaining_length,index;
	int64_t								msg_id;

    value.len = sizeof(double);
	value.data=(u_char *)&num;
	data.data=_data;
	data.len=d_len;
	msg_id=-1;

	index=0;
	p = data.data;
	cmd=p[0];
	//if((cmd & 0xF5) == PUBLISH){}
	qos=(cmd & 0x06)>>1;
	p++;
	remaining_count=utility_mqtt_read_remaining_leng(p,data.len-1,&remaining_length);
	if(remaining_length < index)goto finished;
	p+=remaining_count;
	index=0;

	n=utility_mqtt_read_int64(p,remaining_length,&msg_id);
	if(n!=8)goto finished;
	p+=n;
	index+=n;

	//ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,"len:%d,msg_type:%d,remaining_length:%d,remaining_count:%d,qos:%d,msgId:%l",data.len,cmd,remaining_length,remaining_count,qos,msg_id);
	while(index < remaining_length){
		n=utility_mqtt_read_string(p,remaining_length-index,&key);
		if(n<3)break;
		p+=n;
		index+=n;

		msg_value.len=remaining_length-index;
		if(msg_value.len>0){
			msg_value.data=p;
			n=msg_value.len;
			if(n<3)break;
			p+=n;
			index+=n;

			n=msg_id;
			msg_key.len=1;
			while(n>=10){
				msg_key.len++;
				n=n/10;
			}
			n=msg_key.len;
			msg_key.len+=key.len+1;
			msg_key.data=ngx_palloc(ngx_cycle->pool,msg_key.len+1);
			ngx_memcpy(msg_key.data,key.data,key.len);
			ngx_sprintf(msg_key.data+key.len,"/%l",msg_id);

			//ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,"value.len=%d, key=%s",msg_value.len,msg_key.data);

			store_zone = ngx_stream_lua_find_zone(zoneName,ngx_strlen(zoneName));
			if(store_zone!=NULL)
				ngx_mqtt_shdict_store(store_zone,&msg_key,&msg_value);
			ngx_pfree(ngx_cycle->pool,msg_key.data);
		}
		p=(u_char*)ngx_strstr(key.data,"G/E/");
		if (p==NULL || p > key.data+4){
			zName.data=(u_char*)"MQTT_TOPIC_DICT";
			zName.len=ngx_strlen("MQTT_TOPIC_DICT");
		}else{
			zName.data=(u_char*)"MQTT_TOPIC_EXT_DICT";
			zName.len=strlen("MQTT_TOPIC_EXT_DICT");
		}
		zone = ngx_stream_lua_find_zone(zName.data,zName.len);
		if(zone!=NULL){
			//num=strtold(coffset,&numPEnd);
			//ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,"topic=%s,len=%d,offset=%l,msg_value.len=%d,msg_value.data=%s",key.data,key.len,msg_id,msg_value.len,msg_value.data);
			//dd("topic=%s,len=%zd,offset=%f",key.data,key.len,msg_id);

			//ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,"topic=%s, len=%d, offset=%l, msg_value.len=%d, msg_key=%s",key.data,key.len,msg_id,msg_value.len,msg_key.data);

			num=(double)msg_id;
			ngx_mqtt_shdict_update_offset(zone,&key,&value,LUA_SHDICT_REPLACE);

		}
	}

finished:
	//ngx_pfree(ngx_cycle->pool,data.data);
	return NGX_OK;
}

