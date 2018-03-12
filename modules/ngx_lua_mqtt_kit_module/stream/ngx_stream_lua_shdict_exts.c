#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>
#include "ngx_stream_lua_api.h"
#include "ngx_stream_lua_shdict.h"
#include <stdbool.h>
#include "ddebug.h"
#include "ngx_stream_lua_socket_tcp.h"
#include "ngx_lua_mqtt_kit_module.h"

int ngx_lua_ffi_shdict_lpush(u_char *zone,u_char *key, u_char *_value,ngx_uint_t len);
int ngx_lua_ffi_shdict_rpush(u_char *zone,u_char *key, u_char *_value,ngx_uint_t len);
static int ngx_lua_shdict_push_helper(ngx_str_t *zoneName,ngx_str_t *_key, ngx_str_t *_value,int flags);

int ngx_lua_ffi_shdict_lpop(u_char *zone,u_char *key, ngx_str_t *out);
int ngx_lua_ffi_shdict_rpop(u_char *zone,u_char *key, ngx_str_t *out);
static int ngx_lua_shdict_pop_helper(ngx_str_t *zoneName,ngx_str_t *_key, ngx_str_t *_value,int flags);
//static int ngx_lua_ffi_shdict_llen(lua_State *L);
//static int ngx_lua_ffi_shdict_set_expire(lua_State *L);

#define ngx_stream_LUA_SHDICT_LEFT        0x0001
#define ngx_stream_LUA_SHDICT_RIGHT       0x0002

enum {
    SHDICT_USERDATA_INDEX = 1,
};


static ngx_inline ngx_shm_zone_t *
ngx_lua_shdict_get_zone(lua_State *L, int index)
{
    ngx_shm_zone_t      *zone;

    lua_rawgeti(L, index, SHDICT_USERDATA_INDEX);
    zone = lua_touserdata(L, -1);
    lua_pop(L, 1);

    return zone;
}
int ngx_lua_shdict_expire(ngx_stream_lua_shdict_ctx_t *ctx, ngx_uint_t n)
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
ngx_lua_shdict_lookup(ngx_shm_zone_t *shm_zone, ngx_uint_t hash,
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


#define ngx_lua_shdict_get_list_head(sd, key_len)                      \
    (ngx_queue_t *) ngx_align_ptr(((u_char *) &sd->data + key_len),         \
                                  NGX_ALIGNMENT)

int
ngx_lua_ffi_shdict_lpush(u_char *zone,u_char *_key, u_char *_value,ngx_uint_t len)
{
	ngx_str_t key;
	ngx_str_t value;
	ngx_str_t zoneName;

	zoneName.data=zone;
	zoneName.len=ngx_strlen(zone);
	key.data=_key;
	key.len=ngx_strlen(_key);
	value.data=_value;
	value.len=len;

    return ngx_lua_shdict_push_helper(&zoneName,&key,&value, ngx_stream_LUA_SHDICT_LEFT);
}

int
ngx_lua_ffi_shdict_rpush(u_char *zone,u_char *_key, u_char *_value,ngx_uint_t len)
{
	ngx_str_t key;
	ngx_str_t value;
	ngx_str_t zoneName;

	zoneName.data=zone;
	zoneName.len=ngx_strlen(zone);
	key.data=_key;
	key.len=ngx_strlen(_key);
	value.data=_value;
	value.len=len;

    return ngx_lua_shdict_push_helper(&zoneName,&key,&value, ngx_stream_LUA_SHDICT_RIGHT);
}

static int
ngx_lua_shdict_push_helper(ngx_str_t *zoneName,ngx_str_t *_key, ngx_str_t *_value,int flags)
{
    int                              n;
    ngx_str_t                        key;
    uint32_t                         hash;
    ngx_int_t                        rc;
    ngx_stream_lua_shdict_ctx_t       *ctx;
    ngx_stream_lua_shdict_node_t      *sd;
    ngx_str_t                        value;
    int                              value_type;
    ngx_rbtree_node_t               *node;
    ngx_shm_zone_t                  *zone;
    ngx_queue_t                     *queue, *q;
    ngx_x_lua_shdict_list_node_t *lnode;

	zone = ngx_stream_lua_find_zone(zoneName->data,zoneName->len);
	if(zone==NULL){
		ngx_log_debug(NGX_LOG_ERR,ngx_cycle->log,0, "shdict is null[%s]",zoneName->data);
		return NGX_ERROR;
	}
	key.data=_key->data;
	key.len=_key->len;
	value.data=_value->data;
	value.len=_value->len;

    ctx = zone->data;


    hash = ngx_crc32_short(key.data, key.len);

    value_type = LUA_TSTRING;

    ngx_shmtx_lock(&ctx->shpool->mutex);

#if 1
    ngx_lua_shdict_expire(ctx, 1);
#endif

    rc = ngx_lua_shdict_lookup(zone, hash, key.data, key.len, &sd);

    dd("shdict lookup returned %d", (int) rc);

    if (rc == NGX_DONE) {
        /* exists but expired */

        if (sd->value_type != LUA_TTABLE) {
            /* TODO: reuse when length matched */

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                           "lua shared dict push: found old entry and value "
                           "type not matched, remove it first");

            ngx_queue_remove(&sd->queue);

            node = (ngx_rbtree_node_t *)
                        ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));

            ngx_rbtree_delete(&ctx->sh->rbtree, node);

            ngx_slab_free_locked(ctx->shpool, node);

            dd("go to init_list");
            goto init_list;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                       "lua shared dict push: found old entry and value "
                       "type matched, reusing it");

        sd->expires = 0;

        /* free list nodes */

        queue = ngx_lua_shdict_get_list_head(sd, key.len);

        for (q = ngx_queue_head(queue);
             q != ngx_queue_sentinel(queue);
             q = ngx_queue_next(q))
        {
            /* TODO: reuse matched size list node */
            lnode = ngx_queue_data(q, ngx_x_lua_shdict_list_node_t, queue);
            ngx_slab_free_locked(ctx->shpool, lnode);
        }

        ngx_queue_init(queue);

        ngx_queue_remove(&sd->queue);
        ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

        dd("go to push_node");
        goto push_node;

    } else if (rc == NGX_OK) {

        if (sd->value_type != LUA_TTABLE) {
            ngx_shmtx_unlock(&ctx->shpool->mutex);
			ngx_log_debug(NGX_LOG_ERR,ctx->log,0, "wrongtype operation");
            return NGX_ERROR;
        }

        queue = ngx_lua_shdict_get_list_head(sd, key.len);

        ngx_queue_remove(&sd->queue);
        ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

        dd("go to push_node");
        goto push_node;
    }

    /* rc == NGX_DECLINED, not found */

init_list:

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "lua shared dict list: creating a new entry");

    /* NOTICE: we assume the begin point aligned in slab, be careful */
    n = offsetof(ngx_rbtree_node_t, color)
        + offsetof(ngx_stream_lua_shdict_node_t, data)
        + key.len
        + sizeof(ngx_queue_t);

    dd("length before aligned: %d", n);

    n = (int) (uintptr_t) ngx_align_ptr(n, NGX_ALIGNMENT);

    dd("length after aligned: %d", n);

    node = ngx_slab_alloc_locked(ctx->shpool, n);

    if (node == NULL) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ngx_log_debug(NGX_LOG_ERR,ctx->log,0, "no memory");
        return NGX_ERROR;
    }

    sd = (ngx_stream_lua_shdict_node_t *) &node->color;

    queue = ngx_lua_shdict_get_list_head(sd, key.len);

    node->key = hash;
    sd->key_len = (u_short) key.len;

    sd->expires = 0;

    sd->value_len = 0;

    dd("setting value type to %d", (int) LUA_TTABLE);

    sd->value_type = (uint8_t) LUA_TTABLE;

    ngx_memcpy(sd->data, key.data, key.len);

    ngx_queue_init(queue);

    ngx_rbtree_insert(&ctx->sh->rbtree, node);

    ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);

push_node:

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "lua shared dict list: creating a new list node");

    n = offsetof(ngx_x_lua_shdict_list_node_t, data)
        + value.len;

    dd("list node length: %d", n);

    lnode = ngx_slab_alloc_locked(ctx->shpool, n);

    if (lnode == NULL) {

        if (sd->value_len == 0) {

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                           "lua shared dict list: no memory for create"
                           " list node and list empty, remove it");

            ngx_queue_remove(&sd->queue);

            node = (ngx_rbtree_node_t *)
                        ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));

            ngx_rbtree_delete(&ctx->sh->rbtree, node);

            ngx_slab_free_locked(ctx->shpool, node);
        }

        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ngx_log_debug(NGX_LOG_ERR,ctx->log,0, "no memory");
        return NGX_ERROR;
    }

    dd("setting list length to %d", sd->value_len + 1);

    sd->value_len = sd->value_len + 1;

    dd("setting list node value length to %d", (int) value.len);

    lnode->value_len = (uint32_t) value.len;

    dd("setting list node value type to %d", value_type);

    lnode->value_type = (uint8_t) value_type;

    ngx_memcpy(lnode->data, value.data, value.len);

    if (flags == ngx_stream_LUA_SHDICT_LEFT) {
        ngx_queue_insert_head(queue, &lnode->queue);

    } else {
        ngx_queue_insert_tail(queue, &lnode->queue);
    }

    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return NGX_OK;
}



int
ngx_lua_ffi_shdict_lpop(u_char *zone,u_char *_key, ngx_str_t *out)
{
	ngx_str_t key;
	ngx_str_t value;
	ngx_str_t zoneName;

	zoneName.data=zone;
	zoneName.len=ngx_strlen(zone);
	key.data=_key;
	key.len=ngx_strlen(_key);

    return ngx_lua_shdict_pop_helper(&zoneName,&key,out, ngx_stream_LUA_SHDICT_LEFT);
}


int
ngx_lua_ffi_shdict_rpop(u_char *zone,u_char *_key, ngx_str_t *out)
{
	ngx_str_t key;
	ngx_str_t zoneName;

	zoneName.data=zone;
	zoneName.len=ngx_strlen(zone);
	key.data=_key;
	key.len=ngx_strlen(_key);

    return ngx_lua_shdict_pop_helper(&zoneName,&key,out, ngx_stream_LUA_SHDICT_RIGHT);
}

static int
ngx_lua_shdict_pop_helper(ngx_str_t *zoneName,ngx_str_t *_key, ngx_str_t *out,int flags)
{
    int                              n;
    ngx_str_t                        name;
    ngx_str_t                        key;
    uint32_t                         hash;
    ngx_int_t                        rc;
    ngx_stream_lua_shdict_ctx_t       *ctx;
    ngx_stream_lua_shdict_node_t      *sd;
    ngx_str_t                        value;
    int                              value_type;
    double                           num;
    ngx_rbtree_node_t               *node;
    ngx_shm_zone_t                  *zone;
    ngx_queue_t                     *queue;
	ngx_x_lua_shdict_list_node_t 	*lnode;

	out->len=0;

	zone = ngx_stream_lua_find_zone(zoneName->data,zoneName->len);
	if(zone==NULL){
		ngx_log_error(NGX_LOG_ERR,ngx_cycle->log,0,"shdict is null[%s]",zoneName->data);
		return NGX_ERROR;
	}
	key.data=_key->data;
	key.len=_key->len;

	//value.data=_value->data;
	//value.len=_value->len;

    ctx = zone->data;
    name = ctx->name;


    if (key.len > 65535) {
        //lua_pushnil(L);
        ngx_log_error(NGX_LOG_ERR,ctx->log,0,"key too long");
        return NGX_ERROR;
    }

    hash = ngx_crc32_short(key.data, key.len);

    ngx_shmtx_lock(&ctx->shpool->mutex);

#if 1
    ngx_lua_shdict_expire(ctx, 1);
#endif

    rc = ngx_lua_shdict_lookup(zone, hash, key.data, key.len, &sd);

    //dd("shdict lookup returned %d", (int) rc);

    if (rc == NGX_DECLINED || rc == NGX_DONE) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        //lua_pushnil(L);
        return NGX_OK;
    }

    /* rc == NGX_OK */

    if (sd->value_type != LUA_TTABLE) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);

        //lua_pushnil(L);
        ngx_log_error(NGX_LOG_ERR,ctx->log,0,"wrongtype operation");
        return NGX_ERROR;
    }

    if (sd->value_len <= 0) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ngx_log_error(NGX_LOG_ERR,ctx->log,0, "bad lua list length found for key %s "
                          "in shared_dict %s: %lu", key.data, name.data,
                          (unsigned long) sd->value_len);
		return NGX_ERROR;
    }

    queue = ngx_lua_shdict_get_list_head(sd, key.len);

    if (flags == ngx_stream_LUA_SHDICT_LEFT) {
        queue = ngx_queue_head(queue);

    } else {
        queue = ngx_queue_last(queue);
    }

    lnode = ngx_queue_data(queue, ngx_x_lua_shdict_list_node_t, queue);

    value_type = lnode->value_type;

    //dd("data: %p", lnode->data);
    dd("list length: %lu, value length: %lu", (unsigned long)sd->value_len,(unsigned long)lnode->value_len);

    //value.data = lnode->data;
    //value.len = (size_t) lnode->value_len;

    switch (value_type) {

    case LUA_TSTRING:

		out->data=malloc(lnode->value_len+1);
		ngx_memcpy(out->data,lnode->data,lnode->value_len);
		out->len = (size_t) lnode->value_len;
		//ngx_log_error(NGX_LOG_ERR,ctx->log,0, "value %s, %d", out->data,out->len);
        //lua_pushlstring(L, (char *) value.data, value.len);
        break;

    case LUA_TNUMBER:

        if (lnode->value_len != sizeof(double)) {

            ngx_shmtx_unlock(&ctx->shpool->mutex);

            ngx_log_error(NGX_LOG_ERR,ctx->log,0, "bad lua list node number value size found "
                              "for key %s in shared_dict %s: %lu", key.data,
                              name.data, (unsigned long) lnode->value_len);
			return NGX_ERROR;
        }

		//out->data=ngx_pstrdup(ngx_cycle->pool,lnode);
		out->data=malloc(lnode->value_len+1);
		ngx_memcpy(out->data,lnode->data,lnode->value_len);
		out->len = (size_t) lnode->value_len;
		//ngx_log_error(NGX_LOG_ERR,ctx->log,0, "value %s, %d", out->data,out->len);
        //ngx_memcpy(&num, value.data, sizeof(double));

        //lua_pushnumber(L, num);
        break;

    default:

        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ngx_log_error(NGX_LOG_ERR,ctx->log,0, "bad list node value type found for key %s in "
                          "shared_dict %s: %d", key.data, name.data,
                          value_type);
		return NGX_ERROR;
    }

    ngx_queue_remove(queue);

    ngx_slab_free_locked(ctx->shpool, lnode);

    if (sd->value_len == 1) {

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                       "lua shared dict list: empty node after pop, "
                       "remove it");

        ngx_queue_remove(&sd->queue);

        node = (ngx_rbtree_node_t *)
                    ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));

        ngx_rbtree_delete(&ctx->sh->rbtree, node);

        ngx_slab_free_locked(ctx->shpool, node);

    } else {

        sd->value_len = sd->value_len - 1;

        ngx_queue_remove(&sd->queue);
        ngx_queue_insert_head(&ctx->sh->queue, &sd->queue);
    }

    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return NGX_OK;
}