#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>
#include "ngx_http_lua_mqtt_module.h"
#include "ngx_http_lua_mqtt_module_ipc.h"

static ngx_str_t *
ngx_http_lua_mqtt_create_str(ngx_pool_t *pool, uint len)
{
    ngx_str_t *aux = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t) + len + 1);
    if (aux != NULL) {
        aux->data = (u_char *) (aux + 1);
        aux->len = len;
        ngx_memset(aux->data, '\0', len + 1);
    }
    return aux;
}

ngx_http_lua_mqtt_msg_t *
ngx_http_lua_mqtt_convert_char_to_msg_on_shared(ngx_http_lua_mqtt_main_conf_t *mcf, u_char *data, size_t len, ngx_http_lua_mqtt_channel_t *channel, ngx_int_t id, ngx_str_t *event_id, ngx_str_t *event_type, ngx_pool_t *temp_pool)
{
    ngx_slab_pool_t                           *shpool = mcf->shpool;
    ngx_http_lua_mqtt_shm_data_t           *shm_data = mcf->shm_data;
    ngx_http_lua_mqtt_msg_t                *msg;


    if ((msg = ngx_slab_alloc(shpool, sizeof(ngx_http_lua_mqtt_msg_t))) == NULL) {
        return NULL;
    }

    msg->event_id = NULL;
    msg->event_type = NULL;
    msg->event_id_message = NULL;
    msg->event_type_message = NULL;
    msg->formatted_messages = NULL;
    msg->deleted = 0;
    msg->expires = 0;
    msg->id = id;
    msg->workers_ref_count = 0;
    msg->time = (id < 0) ? 0 : ngx_time();
    msg->tag = (id < 0) ? 0 : ((msg->time == shm_data->last_message_time) ? (shm_data->last_message_tag + 1) : 1);
    msg->qtd_templates = mcf->qtd_templates;
    ngx_queue_init(&msg->queue);

    if ((msg->raw.data = ngx_slab_alloc(shpool, len + 1)) == NULL) {
        //ngx_http_lua_mqtt_free_message_memory(shpool, msg);
        return NULL;
    }

    msg->raw.len = len;
    // copy the message to shared memory
    ngx_memcpy(msg->raw.data, data, len);
    msg->raw.data[msg->raw.len] = '\0';



    return msg;
}



ngx_int_t
ngx_http_lua_mqtt_add_msg_to_channel(ngx_http_lua_mqtt_main_conf_t *mcf, ngx_log_t *log, ngx_http_lua_mqtt_channel_t *channel, u_char *text, size_t len, ngx_str_t *event_id, ngx_str_t *event_type, ngx_flag_t store_messages, ngx_pool_t *temp_pool)
{
    ngx_http_lua_mqtt_shm_data_t        *data = mcf->shm_data;
    ngx_http_lua_mqtt_msg_t             *msg;
    ngx_uint_t                              qtd_removed;

    // create a buffer copy in shared mem
    msg = ngx_http_lua_mqtt_convert_char_to_msg_on_shared(mcf, text, len, channel, channel->last_message_id + 1, event_id, event_type, temp_pool);
    if (msg == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate message in shared memory");
        return NGX_ERROR;
    }

    ngx_shmtx_lock(channel->mutex);
    channel->last_message_id++;

    // tag message with time stamp and a sequence tag
    channel->last_message_time = msg->time;
    channel->last_message_tag = msg->tag;
    // set message expiration time
    msg->expires = msg->time + mcf->message_ttl;
    channel->expires = ngx_time() + mcf->channel_inactivity_time;

    // put messages on the queue
    if (store_messages) {
        ngx_queue_insert_tail(&channel->message_queue, &msg->queue);
        channel->stored_messages++;
    }
    ngx_shmtx_unlock(channel->mutex);


    // send an alert to workers
    ngx_http_lua_mqtt_broadcast(channel, msg, log, mcf);

    // turn on timer to cleanup buffer of old messages
    //ngx_http_lua_mqtt_buffer_cleanup_timer_set();

    return NGX_OK;
}

ngx_int_t
ngx_http_lua_mqtt_send_event(ngx_http_lua_mqtt_main_conf_t *mcf, ngx_log_t *log, ngx_http_lua_mqtt_channel_t *channel, ngx_str_t *event_type, ngx_pool_t *received_temp_pool)
{
    ngx_pool_t *temp_pool = received_temp_pool;

    if ((temp_pool == NULL) && ((temp_pool = ngx_create_pool(4096, log)) == NULL)) {
        return NGX_ERROR;
    }

    if ((mcf->events_channel_id.len > 0) && !channel->for_events) {
        size_t len = ngx_strlen(NGX_HTTP_lua_mqtt_EVENT_TEMPLATE) + event_type->len + channel->id.len;
        ngx_str_t *event = ngx_http_lua_mqtt_create_str(temp_pool, len);
        if (event != NULL) {
            ngx_sprintf(event->data, NGX_HTTP_lua_mqtt_EVENT_TEMPLATE, event_type, &channel->id);
            ngx_http_lua_mqtt_add_msg_to_channel(mcf, log, mcf->events_channel, event->data, ngx_strlen(event->data), NULL, event_type, 1, temp_pool);
        }
    }

    if ((received_temp_pool == NULL) && (temp_pool != NULL)) {
        ngx_destroy_pool(temp_pool);
    }

    return NGX_OK;
}



