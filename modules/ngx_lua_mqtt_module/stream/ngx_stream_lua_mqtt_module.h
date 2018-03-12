
#ifndef NGX_HTTP_LUA_MQTT_MODULE_H
#define NGX_HTTP_LUA_MQTT_MODULE_H

ngx_module_t ngx_stream_lua_mqtt_module;





// shared memory segment name
//static ngx_str_t    ngx_http_lua_mqtt_shm_name = ngx_string("lua_mqtt_module");


typedef struct {
	ngx_flag_t                      enabled;
	ngx_str_t						serverId;
 //   ngx_shm_zone_t                 *shm_zone;
    ngx_slab_pool_t                *shpool;
    //ngx_stream_lua_mqtt_shm_data_t	*shm_data;
} ngx_stream_lua_mqtt_main_conf_t;

typedef struct {
	ngx_flag_t                      enabled;
    ngx_str_t                       channel_deleted_message_text;
	ngx_str_t						serverId;
 //   ngx_shm_zone_t                 *shm_zone;
    ngx_slab_pool_t                *shpool;
    //ngx_stream_lua_mqtt_shm_data_t	*shm_data;
} ngx_stream_mqtt_srv_conf_t;




char *
ngx_stream_lua_mqtt_set_shm_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#endif // NGX_HTTP_LUA_MQTT_MODULE_H
