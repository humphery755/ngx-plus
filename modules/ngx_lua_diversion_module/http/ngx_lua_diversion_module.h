
#ifndef NGX_HTTP_LUA_diversion_MODULE_H
#define NGX_HTTP_LUA_diversion_MODULE_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
	ngx_uint_t						workerId;
} ngx_lua_diversion_main_conf_t;

typedef struct {
    uint64_t					start;
	uint64_t					end;
} diversion_policy_range_t;

typedef struct {
    uint16_t					type;
	uint32_t					datalen;
	uint32_t					id;
	uint8_t						bitsetId; //0: none, 1: bitset1, 2: bitset2
    void						*data;
} diversion_policy_t;

typedef struct {
    ngx_str_t                   uri;
    ngx_str_t                   backend;
	ngx_str_t					text;
	uint16_t					action;
	uint16_t					policyslen;
    diversion_policy_t			**policys;
} diversion_policy_group_t;

typedef struct {
    uint8_t                      value_type;
    uint32_t                     value_len;
    ngx_queue_t                  queue;
    u_char                       data[1];
} ngx_x_lua_shdict_list_node_t;

typedef struct {
	uint16_t					action;
    ngx_str_t					backend;
	ngx_str_t					text;	
} diversion_cacl_res_t;

typedef struct {
	ngx_str_t					key;
	ngx_str_t					value;
} diversion_env_host_t;

typedef struct {
	uint32_t					id;
    int16_t						status;	 //-1:none, 0:init -> none, 1:svc -> sysnc, 2:self -> active, 3:svc -> cancel
	uint32_t					version;
	ngx_str_t					src;
	ngx_str_t					to;
	uint16_t					envhosts_len;
	diversion_env_host_t		*envhosts;
	
	uint16_t					policy_grouplen;
	diversion_policy_group_t	*policy_group;
	ngx_slab_pool_t             *pgshpool;
	ngx_slab_pool_t             *vhshpool;
	ngx_shmtx_t					*mutex;
	ngx_atomic_t				lock;
} diversion_runtime_info_t;


//int ngx_ffi_diversion_exec_policy(diversion_user_info_t *userInfo,diversion_cacl_res_t *out);

#ifdef __cplusplus
}
#endif

#endif // NGX_HTTP_LUA_mqtt_kit_MODULE_H
