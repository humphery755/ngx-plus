
#ifndef NGX_HTTP_LUA_REDIS3_H
#define NGX_HTTP_LUA_REDIS3_H

typedef struct {
	ngx_array_t      *upstream_names;
	ngx_array_t      *redis_slots;
} ngx_http_lua_redis3_conf_t;
ngx_int_t ngx_http_lua_redis3_init(ngx_conf_t *cf);
char *ngx_http_redis3_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
int failed_notify(lua_State *L);
int build_slots(lua_State *L);
int get_host(lua_State *L);

#endif // NGX_HTTP_LUA_REDIS3_H
