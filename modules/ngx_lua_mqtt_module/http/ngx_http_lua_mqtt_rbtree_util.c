#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_crc32.h>
#include "ngx_http_lua_mqtt_module.h"

static ngx_str_t  NGX_HTTP_lua_mqtt_EVENT_TYPE_CHANNEL_CREATED = ngx_string("channel_created");


static void
ngx_http_lua_mqtt_rbtree_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static int
ngx_http_lua_mqtt_compare_rbtree_node(const ngx_rbtree_node_t *v_left, const ngx_rbtree_node_t *v_right);
static ngx_http_lua_mqtt_channel_t *
ngx_http_lua_mqtt_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_lua_mqtt_main_conf_t *mcf);
static ngx_http_lua_mqtt_channel_t *
ngx_http_lua_mqtt_find_channel_on_tree(ngx_str_t *id, ngx_log_t *log, ngx_rbtree_t *tree);

ngx_int_t
ngx_http_lua_mqtt_create_shmtx(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    u_char           *file;

#if (NGX_HAVE_ATOMIC_OPS)

    file = NULL;

#else

    ngx_str_t        logs_dir = ngx_string("logs/");

    if (ngx_conf_full_name((ngx_cycle_t  *) ngx_cycle, &logs_dir, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    file = ngx_pnalloc(ngx_cycle->pool, logs_dir.len + ngx_strlen(name));
    if (file == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_sprintf(file, "%V%s%Z", &logs_dir, name);

#endif

    if (ngx_shmtx_create(mtx, addr, file) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t
ngx_http_lua_mqtt_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_lua_mqtt_global_shm_data_t *global_shm_data = (ngx_http_lua_mqtt_global_shm_data_t *) ngx_http_lua_mqtt_global_shm_zone->data;
    ngx_http_lua_mqtt_main_conf_t       *mcf = shm_zone->data;
    ngx_http_lua_mqtt_shm_data_t        *d;
    int i;

    mcf->shm_zone = shm_zone;
    mcf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) { /* zone already initialized */
        shm_zone->data = data;
        d = (ngx_http_lua_mqtt_shm_data_t *) data;
        d->mcf = mcf;
        d->shm_zone = shm_zone;
        d->shpool = mcf->shpool;
        mcf->shm_data = data;
        ngx_queue_insert_tail(&global_shm_data->shm_datas_queue, &d->shm_data_queue);
        return NGX_OK;
    }

    ngx_rbtree_node_t                   *sentinel;

    if ((d = (ngx_http_lua_mqtt_shm_data_t *) ngx_slab_alloc(mcf->shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    d->mcf = mcf;
    mcf->shm_data = d;
    shm_zone->data = d;
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->ipc[i].pid = -1;
        d->ipc[i].startup = 0;
        d->ipc[i].subscribers = 0;
        ngx_queue_init(&d->ipc[i].messages_queue);
        ngx_queue_init(&d->ipc[i].subscribers_queue);
    }

    d->channels = 0;
    d->wildcard_channels = 0;
    d->published_messages = 0;
    d->stored_messages = 0;
    d->subscribers = 0;
    d->channels_in_trash = 0;
    d->messages_in_trash = 0;
    d->startup = ngx_time();
    d->last_message_time = 0;
    d->last_message_tag = 0;
    d->shm_zone = shm_zone;
    d->shpool = mcf->shpool;
    d->slots_for_census = 0;

    // initialize rbtree
    if ((sentinel = ngx_slab_alloc(mcf->shpool, sizeof(*sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->tree, sentinel, ngx_http_lua_mqtt_rbtree_insert);

    ngx_queue_init(&d->messages_trash);
    ngx_queue_init(&d->channels_queue);
    ngx_queue_init(&d->channels_to_delete);
    ngx_queue_init(&d->channels_trash);

    ngx_queue_insert_tail(&global_shm_data->shm_datas_queue, &d->shm_data_queue);

    if (ngx_http_lua_mqtt_create_shmtx(&d->messages_trash_mutex, &d->messages_trash_lock, (u_char *) "lua_mqtt_messages_trash") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_lua_mqtt_create_shmtx(&d->channels_queue_mutex, &d->channels_queue_lock, (u_char *) "lua_mqtt_channels_queue") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_lua_mqtt_create_shmtx(&d->channels_to_delete_mutex, &d->channels_to_delete_lock, (u_char *) "lua_mqtt_channels_to_delete") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_lua_mqtt_create_shmtx(&d->channels_trash_mutex, &d->channels_trash_lock, (u_char *) "lua_mqtt_channels_trash") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_lua_mqtt_create_shmtx(&d->cleanup_mutex, &d->cleanup_lock, (u_char *) "lua_mqtt_cleanup") != NGX_OK) {
        return NGX_ERROR;
    }

    u_char lock_name[25];
    for (i = 0; i < 10; i++) {
        ngx_sprintf(lock_name, "lua_mqtt_channels_%d", i);
        if (ngx_http_lua_mqtt_create_shmtx(&d->channels_mutex[i], &d->channels_lock[i], lock_name) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    d->mutex_round_robin = 0;

    if (mcf->events_channel_id.len > 0) {
        if ((mcf->events_channel = ngx_http_lua_mqtt_get_channel(&mcf->events_channel_id, ngx_cycle->log, mcf)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: unable to create events channel");
            return NGX_ERROR;
        }

        if (ngx_http_lua_mqtt_create_shmtx(&d->events_channel_mutex, &d->events_channel_lock, (u_char *) "lua_mqtt_events_channel") != NGX_OK) {
            return NGX_ERROR;
        }

        mcf->events_channel->mutex = &d->events_channel_mutex;
    }

    return NGX_OK;
}

// shared memory
char *
ngx_http_lua_mqtt_set_shm_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_lua_mqtt_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_lua_mqtt_module);
    size_t                               shm_size;
    size_t                               shm_size_limit = 32 * ngx_pagesize;
    ngx_str_t                           *value;
    ngx_str_t                           *name;

	if (mcf == NULL) {
		return NULL;
	}

    value = cf->args->elts;

    shm_size = ngx_align(ngx_parse_size(&value[1]), ngx_pagesize);
    if (shm_size < shm_size_limit) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "The lua_mqtt_shared_memory_size value must be at least %ulKiB", shm_size_limit >> 10);
        return NGX_CONF_ERROR;
    }

    name = (cf->args->nelts > 2) ? &value[2] : &ngx_http_lua_mqtt_shm_name;
    if ((ngx_http_lua_mqtt_global_shm_zone != NULL) && (ngx_http_lua_mqtt_global_shm_zone->data != NULL)) {
        ngx_http_lua_mqtt_global_shm_data_t *global_data = (ngx_http_lua_mqtt_global_shm_data_t *) ngx_http_lua_mqtt_global_shm_zone->data;
        ngx_queue_t                            *q;

        for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
            ngx_http_lua_mqtt_shm_data_t *data = ngx_queue_data(q, ngx_http_lua_mqtt_shm_data_t, shm_data_queue);
            if ((name->len == data->shm_zone->shm.name.len) &&
                (ngx_strncmp(name->data, data->shm_zone->shm.name.data, name->len) == 0) &&
                (data->shm_zone->shm.size != shm_size)) {
                shm_size = data->shm_zone->shm.size;
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "Cannot change memory area size without restart, ignoring change on zone: %V", name);
            }
        }
    }
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Using %udKiB of shared memory for push stream module on zone: %V", shm_size >> 10, name);

    mcf->shm_zone = ngx_shared_memory_add(cf, name, shm_size, &ngx_http_lua_mqtt_module);

    if (mcf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (mcf->shm_zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "duplicate zone \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    mcf->shm_zone->init = ngx_http_lua_mqtt_init_shm_zone;
    mcf->shm_zone->data = mcf;

    return NGX_CONF_OK;
}


// find a channel by id. if channel not found, make one, insert it, and return that.
static ngx_http_lua_mqtt_channel_t *
ngx_http_lua_mqtt_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_lua_mqtt_main_conf_t *mcf)
{
    ngx_http_lua_mqtt_shm_data_t       *data = mcf->shm_data;
    ngx_http_lua_mqtt_channel_t        *channel;
    ngx_slab_pool_t                       *shpool = mcf->shpool;
    ngx_flag_t                             is_wildcard_channel = 0;

    if (id == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: tried to create a channel with a null id");
        return NULL;
    }

    ngx_shmtx_lock(&data->channels_queue_mutex);

    // check again to see if any other worker didn't create the channel
    channel = ngx_http_lua_mqtt_find_channel_on_tree(id, log, &data->tree);
    if (channel != NULL) { // we found our channel
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        return channel;
    }

    if ((mcf->wildcard_channel_prefix.len > 0) && (ngx_strncmp(id->data, mcf->wildcard_channel_prefix.data, mcf->wildcard_channel_prefix.len) == 0)) {
        is_wildcard_channel = 1;
    }

    if (((!is_wildcard_channel) && (mcf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_channels == data->channels)) ||
        ((is_wildcard_channel) && (mcf->max_number_of_wildcard_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_wildcard_channels == data->wildcard_channels))) {
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: number of channels were exceeded");
        return NGX_HTTP_lua_mqtt_NUMBER_OF_CHANNELS_EXCEEDED;
    }

    if ((channel = ngx_slab_alloc(shpool, sizeof(ngx_http_lua_mqtt_channel_t))) == NULL) {
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate memory for new channel");
        return NULL;
    }

    if ((channel->id.data = ngx_slab_alloc(shpool, id->len + 1)) == NULL) {
        ngx_slab_free(shpool, channel);
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate memory for new channel id");
        return NULL;
    }

    channel->id.len = id->len;
    ngx_memcpy(channel->id.data, id->data, channel->id.len);
    channel->id.data[channel->id.len] = '\0';

    channel->wildcard = is_wildcard_channel;
    channel->channel_deleted_message = NULL;
    channel->last_message_id = 0;
    channel->last_message_time = 0;
    channel->last_message_tag = 0;
    channel->stored_messages = 0;
    channel->subscribers = 0;
    channel->deleted = 0;
    channel->for_events = ((mcf->events_channel_id.len > 0) && (channel->id.len == mcf->events_channel_id.len) && (ngx_strncmp(channel->id.data, mcf->events_channel_id.data, mcf->events_channel_id.len) == 0));
    channel->expires = ngx_time() + mcf->channel_inactivity_time;

    ngx_queue_init(&channel->message_queue);
    ngx_queue_init(&channel->workers_with_subscribers);

    channel->node.key = ngx_crc32_short(channel->id.data, channel->id.len);
    ngx_rbtree_insert(&data->tree, &channel->node);
    ngx_queue_insert_tail(&data->channels_queue, &channel->queue);
    (channel->wildcard) ? data->wildcard_channels++ : data->channels++;

    channel->mutex = &data->channels_mutex[data->mutex_round_robin++ % 9];

    ngx_shmtx_unlock(&data->channels_queue_mutex);

    ngx_http_lua_mqtt_send_event(mcf, log, channel, &NGX_HTTP_lua_mqtt_EVENT_TYPE_CHANNEL_CREATED, NULL);

    return channel;
}

static ngx_http_lua_mqtt_channel_t *
ngx_http_lua_mqtt_find_channel_on_tree(ngx_str_t *id, ngx_log_t *log, ngx_rbtree_t *tree)
{
    uint32_t                            hash;
    ngx_rbtree_node_t                  *node, *sentinel;
    ngx_int_t                           rc;
    ngx_http_lua_mqtt_channel_t     *channel = NULL;

    hash = ngx_crc32_short(id->data, id->len);

    node = tree->root;
    sentinel = tree->sentinel;

    while ((node != NULL) && (node != sentinel)) {
        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        channel = (ngx_http_lua_mqtt_channel_t *) node;

        rc = ngx_memn2cmp(id->data, channel->id.data, id->len, channel->id.len);
        if (rc == 0) {
            return channel;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}

static void
ngx_rbtree_generic_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel, int (*compare) (const ngx_rbtree_node_t *left, const ngx_rbtree_node_t *right))
{
    ngx_rbtree_node_t       **p;

    for (;;) {
        if (node->key < temp->key) {
            p = &temp->left;
        } else if (node->key > temp->key) {
            p = &temp->right;
        } else { /* node->key == temp->key */
            p = (compare(node, temp) < 0) ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

static void
ngx_http_lua_mqtt_rbtree_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_generic_insert(temp, node, sentinel, ngx_http_lua_mqtt_compare_rbtree_node);
}


static int
ngx_http_lua_mqtt_compare_rbtree_node(const ngx_rbtree_node_t *v_left, const ngx_rbtree_node_t *v_right)
{
    ngx_http_lua_mqtt_channel_t *left = (ngx_http_lua_mqtt_channel_t *) v_left, *right = (ngx_http_lua_mqtt_channel_t *) v_right;

    return ngx_memn2cmp(left->id.data, right->id.data, left->id.len, right->id.len);
}

