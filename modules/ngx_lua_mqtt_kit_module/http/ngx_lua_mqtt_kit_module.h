
#ifndef NGX_HTTP_LUA_mqtt_kit_MODULE_H
#define NGX_HTTP_LUA_mqtt_kit_MODULE_H


typedef struct {
	ngx_int_t						workerId1;
} ngx_lua_mqtt_kit_main_conf_t;

typedef struct {
	ngx_flag_t enable;
	ngx_str_t logfile;
	ngx_str_t debugfile;
	ngx_int_t log_level;
} ngx_lua_mqtt_kit_svr_conf_t;

typedef struct {
    uint8_t                      value_type;
    uint32_t                     value_len;
    ngx_queue_t                  queue;
    u_char                       data[1];
} ngx_x_lua_shdict_list_node_t;

char* ngx_http_bizlog_init(ngx_conf_t *cf, ngx_lua_mqtt_kit_svr_conf_t *cscf);

#endif // NGX_HTTP_LUA_mqtt_kit_MODULE_H
