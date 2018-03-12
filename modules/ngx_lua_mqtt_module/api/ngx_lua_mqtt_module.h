
#ifndef NGX_HTTP_LUA_MQTT_MODULE_H
#define NGX_HTTP_LUA_MQTT_MODULE_H

ngx_module_t ngx_http_lua_mqtt_module;


#define NGX_HTTP_lua_mqtt_MESSAGE_BUFFER_CLEANUP_INTERVAL                5000     // 5 seconds
//static time_t NGX_HTTP_lua_mqtt_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL = 10;      // 10 seconds
//static time_t NGX_HTTP_lua_mqtt_DEFAULT_SHM_MEMORY_CLEANUP_INTERVAL    = 4000;    // 4 seconds
//static time_t NGX_HTTP_lua_mqtt_DEFAULT_MESSAGE_TTL                    = 1800;    // 30 minutes
//static time_t NGX_HTTP_lua_mqtt_DEFAULT_CHANNEL_INACTIVITY_TIME        = 30;      // 30 seconds

#define NGX_HTTP_lua_mqtt_EVENT_TEMPLATE "{\"type\": \"%V\", \"channel\": \"%V\"}%Z"
#define NGX_HTTP_lua_mqtt_UNSET_CHANNEL_ID               (void *) -1
#define NGX_HTTP_lua_mqtt_TOO_LARGE_CHANNEL_ID           (void *) -2
#define NGX_HTTP_lua_mqtt_NUMBER_OF_CHANNELS_EXCEEDED    (void *) -3


extern ngx_shm_zone_t     *ngx_http_lua_mqtt_global_shm_zone;

typedef struct ngx_http_lua_mqtt_msg_s ngx_http_lua_mqtt_msg_t;
typedef struct ngx_http_lua_mqtt_global_shm_data_s ngx_http_lua_mqtt_global_shm_data_t;
typedef struct ngx_http_lua_mqtt_shm_data_s ngx_http_lua_mqtt_shm_data_t;
typedef struct ngx_http_lua_mqtt_channel_s ngx_http_lua_mqtt_channel_t;

// shared memory segment name
//static ngx_str_t    ngx_http_lua_mqtt_shm_name = ngx_string("lua_mqtt_module");


typedef struct {
	ngx_flag_t                      enabled;
    ngx_str_t                       channel_deleted_message_text;
	ngx_str_t						serverId;
    time_t                          channel_inactivity_time;
    ngx_str_t                       ping_message_text;
    ngx_uint_t                      qtd_templates;
    ngx_str_t                       wildcard_channel_prefix;
    ngx_uint_t                      max_number_of_channels;
//    ngx_uint_t                      max_number_of_wildcard_channels;
//    time_t                          message_ttl;
//    ngx_uint_t                      max_subscribers_per_channel;
//    ngx_uint_t                      max_messages_stored_per_channel;
    ngx_uint_t                      max_channel_id_length;
    ngx_queue_t                     msg_templates;
    ngx_flag_t                      timeout_with_body;
    ngx_str_t                       events_channel_id;
    ngx_http_lua_mqtt_channel_t 	*events_channel;
    ngx_regex_t                    *backtrack_parser_regex;
    ngx_http_lua_mqtt_msg_t     	*ping_msg;
    ngx_http_lua_mqtt_msg_t     	*longpooling_timeout_msg;
 //   ngx_shm_zone_t                 *shm_zone;
    ngx_slab_pool_t                *shpool;
    ngx_http_lua_mqtt_shm_data_t	*shm_data;
} ngx_http_lua_mqtt_main_conf_t;

// message queue
struct ngx_http_lua_mqtt_msg_s {
    ngx_queue_t                     queue;
    time_t                          expires;
    time_t                          time;
    ngx_flag_t                      deleted;
    ngx_int_t                       id;
    ngx_str_t                       raw;
    ngx_int_t                       tag;
    ngx_str_t                      *event_id;
    ngx_str_t                      *event_type;
    ngx_str_t                      *event_id_message;
    ngx_str_t                      *event_type_message;
    ngx_str_t                      *formatted_messages;
    ngx_int_t                       workers_ref_count;
    ngx_uint_t                      qtd_templates;
};

struct ngx_http_lua_mqtt_channel_s {
    ngx_rbtree_node_t                   node;
    ngx_queue_t                         queue;
    ngx_str_t                           id;
    ngx_uint_t                          last_message_id;
    time_t                              last_message_time;
    ngx_int_t                           last_message_tag;
    ngx_uint_t                          stored_messages;
    ngx_uint_t                          subscribers;
    ngx_queue_t                         workers_with_subscribers;
    ngx_queue_t                         message_queue;
    time_t                              expires;
    ngx_flag_t                          deleted;
    ngx_flag_t                          wildcard;
    char                                for_events;
    ngx_http_lua_mqtt_msg_t         	*channel_deleted_message;
    ngx_shmtx_t                        *mutex;
};

// shared memory
struct ngx_http_lua_mqtt_global_shm_data_s {
    pid_t                                   pid[NGX_MAX_PROCESSES];
    ngx_queue_t                             shm_datas_queue;
};

// messages to worker processes
typedef struct {
    ngx_queue_t                         queue;
    ngx_http_lua_mqtt_msg_t         	*msg; // ->shared memory
    ngx_pid_t                           pid;
    ngx_http_lua_mqtt_channel_t     	*channel; // ->shared memory
    ngx_queue_t                        *subscriptions_sentinel; // ->a worker's local pool
    ngx_http_lua_mqtt_main_conf_t   	*mcf;
} ngx_http_lua_mqtt_worker_msg_t;

typedef struct {
    ngx_queue_t                         messages_queue;
    ngx_queue_t                         subscribers_queue;
    ngx_uint_t                          subscribers; // # of subscribers in the worker
    time_t                              startup;
    pid_t                               pid;
} ngx_http_lua_mqtt_worker_data_t;

struct ngx_http_lua_mqtt_shm_data_s {
    ngx_rbtree_t                            tree;
    ngx_uint_t                              channels;           // # of channels being used
    ngx_uint_t                              wildcard_channels;  // # of wildcard channels being used
    ngx_uint_t                              published_messages; // # of published messagens in all channels
    ngx_uint_t                              stored_messages;    // # of messages being stored
    ngx_uint_t                              subscribers;        // # of subscribers in all channels
    ngx_queue_t                             messages_trash;
    ngx_shmtx_t                             messages_trash_mutex;
    ngx_shmtx_sh_t                          messages_trash_lock;
    ngx_queue_t                             channels_queue;
    ngx_shmtx_t                             channels_queue_mutex;
    ngx_shmtx_sh_t                          channels_queue_lock;
    ngx_queue_t                             channels_trash;
    ngx_shmtx_t                             channels_trash_mutex;
    ngx_shmtx_sh_t                          channels_trash_lock;
    ngx_queue_t                             channels_to_delete;
    ngx_shmtx_t                             channels_to_delete_mutex;
    ngx_shmtx_sh_t                          channels_to_delete_lock;
    ngx_uint_t                              channels_in_trash;  // # of channels in trash queue
    ngx_uint_t                              messages_in_trash;  // # of messages in trash queue
    ngx_http_lua_mqtt_worker_data_t      	ipc[NGX_MAX_PROCESSES]; // interprocess stuff
    time_t                                  startup;
    time_t                                  last_message_time;
    ngx_int_t                               last_message_tag;
    ngx_queue_t                             shm_data_queue;
    ngx_http_lua_mqtt_main_conf_t       	*mcf;
    ngx_shm_zone_t                         *shm_zone;
    ngx_slab_pool_t                        *shpool;
    ngx_uint_t                              slots_for_census;
    ngx_uint_t                              mutex_round_robin;
    ngx_shmtx_t                             channels_mutex[10];
    ngx_shmtx_sh_t                          channels_lock[10];
    ngx_shmtx_t                             cleanup_mutex;
    ngx_shmtx_sh_t                          cleanup_lock;
    ngx_shmtx_t                             events_channel_mutex;
    ngx_shmtx_sh_t                          events_channel_lock;
};

typedef struct {
    ngx_queue_t                         queue;
    pid_t                               pid;
    ngx_int_t                           slot;
    ngx_queue_t                         subscriptions;
    ngx_uint_t                          subscribers;
} ngx_http_lua_mqtt_pid_queue_t;



char *
ngx_http_lua_mqtt_set_shm_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#endif // NGX_HTTP_LUA_MQTT_MODULE_H
