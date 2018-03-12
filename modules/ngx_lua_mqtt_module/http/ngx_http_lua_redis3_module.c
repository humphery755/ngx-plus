#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>
#include "ngx_http_lua_api.h"
#include "ngx_http_lua_shdict.h"
#include <stdbool.h>
#include "ddebug.h"
#include "ngx_http_lua_socket_tcp.h"
#include "ngx_http_lua_mqtt_module.h"
#include "ngx_http_lua_redis3.h"

static const uint16_t crc16tab[256]= {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
    0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
    0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
    0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
    0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
    0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
    0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
    0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

//Redis遵循的标准是CRC-16-CCITT标准
uint16_t lua_utility_redisCRC16(const char *buf, int len) {
    int counter;
    uint16_t crc = 0;
    for (counter = 0; counter < len; counter++)
            crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *buf++)&0x00FF];
    return crc;
}

#define REDIS_ATOI(p,len) while(*p != '\r') {len = (len*10)+(*p - '0');p++;}

static ngx_flag_t ngx_http_lua_redis3_enabled = 0;
static ngx_str_t					pre_md5;

typedef struct {
	uint16_t      		min;
	uint16_t			max;
	ngx_str_t			addr_ip[10];
	int					addr_port[10];
	int					addr_num;
	unsigned			failed_addr0;
	ngx_atomic_t 		count;
} ngx_http_lua_redis_slot_t;


enum {
    SOCKET_CTX_INDEX = 1,
    SOCKET_TIMEOUT_INDEX = 2,
    SOCKET_KEY_INDEX = 3
};

enum redis_protocol_slots_status_e{redis_p_star,redis_p_row_star,redis_p_min,redis_p_max,redis_p_addr,redis_p_row_end};

static ngx_http_lua_redis3_conf_t *redis3_conf=NULL;

char *ngx_http_redis3_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
ngx_int_t ngx_http_redis3_get_peer(ngx_peer_connection_t *pc, void *data);


ngx_int_t ngx_http_lua_redis3_init(ngx_conf_t *cf)
{

	redis3_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_lua_redis3_conf_t));
	if (redis3_conf == NULL) {
		return NGX_ERROR;
	}

    return NGX_OK;
}


static ngx_inline ngx_int_t ngx_http_lua_redis3_build_slots(const u_char *buf,ngx_int_t len,const u_char *pos,ngx_int_t rows,const ngx_array_t *redis_slot_array){
	u_char						*p;
	ngx_int_t					r_total,num,addr_num;
	ngx_http_lua_redis_slot_t	*redis_slot;

	enum redis_protocol_slots_status_e slots_status;
	p = pos;
	r_total = rows;
	slots_status = redis_p_row_end;

	do{
		switch(*p){
		case '*':
			p++;
			switch(slots_status){
			case redis_p_row_end:
				slots_status=redis_p_row_star;
				redis_slot=ngx_array_push(redis_slot_array);
				redis_slot->failed_addr0=0;
				r_total--;
				addr_num = 0;

				num=0;
				while(*p != '\r' && p-buf<len) {num = (num*10)+(*p - '0');p++;}
				redis_slot->addr_num=num-2;
				break;
			default:
				while(*p != '\r' && p-buf<len) {p++;}
            	break;
			}

			break;
		case ':':
			p++;
			switch(slots_status){
			case redis_p_row_star:
				num=0;
				while(*p != '\r' && p-buf<len) {num = (num*10)+(*p - '0');p++;}
				redis_slot->min=num;
				slots_status=redis_p_max;
				break;
			case redis_p_max:
				num=0;
				while(*p != '\r' && p-buf<len) {num = (num*10)+(*p - '0');p++;}
				redis_slot->max=num;
				slots_status=redis_p_addr;
				break;
			case redis_p_addr:
				num=0;
				while(*p != '\r' && p-buf<len) {num = (num*10)+(*p - '0');p++;}
				redis_slot->addr_port[addr_num]=num;
				if(addr_num==redis_slot->addr_num-1){
					slots_status = redis_p_row_end;
					if(r_total==0)return NGX_OK;
				}
				addr_num++;
				break;
			default:
            	break;
			}
			break;
		case '$':
			p++;
			switch(slots_status){
			case redis_p_addr:
				num=0;
				while(*p != '\r' && p-buf<len) {num = (num*10)+(*p - '0');p++;}
				if((p-buf)+4+num>=len)return NGX_ERROR;
				p+=2;
				redis_slot->addr_ip[addr_num].len = num;
				redis_slot->addr_ip[addr_num].data = ngx_palloc(redis_slot_array->pool,num+1);
				ngx_memcpy(redis_slot->addr_ip[addr_num].data,p,num);
				p+=2;
				break;
			default:
            	break;
			}
			break;
		default:
            break;
		}
		p++;
	}while(p-buf<len);

	return NGX_ERROR;
}

int get_host(lua_State * L)
{
    ngx_http_request_t  *r;
	ngx_str_t			upstream_name;
	ngx_str_t			key;
    uint16_t 			crc,slot;
	ngx_uint_t			i,opt_flg;
	ngx_http_lua_redis_slot_t	*redis_slot;

	if (lua_gettop(L) < 3) {
		return luaL_error(L, "expecting 3/4 arguments (including the object), "
						  "but got %d", lua_gettop(L));
	}
	if(pre_md5.len<=0){
		return luaL_error(L, "Redis cluster is not initialized");
	}
	r = ngx_http_lua_get_request(L);

	upstream_name.data = (u_char *) luaL_checklstring(L, 2, &upstream_name.len);
	if (upstream_name.len == 0) {
		//lua_pushnil(L);
		//lua_pushliteral(L, "empty upstream_name");
		//return 2;
		return luaL_error(L, "empty upstream_name");
	}

	key.data = (u_char *) luaL_checklstring(L, 3, &key.len);

	if (key.len == 0) {
		return luaL_error(L, "empty key");
		/*lua_pushnil(L);
		lua_pushliteral(L, "empty key");
		return 2;
		*/
	}

	opt_flg = luaL_checkinteger(L, 4);

    crc = lua_utility_redisCRC16(key.data,key.len);

	slot = crc%16384;

	i = 0;
	for (; i < redis3_conf->redis_slots->nelts;i++)
	{
		redis_slot = ((ngx_http_lua_redis_slot_t*)redis3_conf->redis_slots->elts   ) + i;
		if(redis_slot->min <= slot && slot <= redis_slot->max){
			break;
		}
	}

	if(i<redis3_conf->redis_slots->nelts){
		/* redis cluster no support readonly */
		switch(opt_flg){
		case 1://读且实时性要求不严
			i=redis_slot->count%redis_slot->addr_num;
			ngx_atomic_fetch_add(&redis_slot->count,1);
			break;
		default:
			i=0;
			break;
		}

		if(redis_slot->failed_addr0 && i==0){
			i=1;
		}
		ngx_log_error(NGX_LOG_INFO, r->pool->log, 0, "index:%d,key:%s,min:%d,max:%d,addr:%s:%d,slot:%d",i,key.data,redis_slot->min,redis_slot->max,redis_slot->addr_ip[i].data,redis_slot->addr_port[i],slot);
		lua_pushlstring(L, (const char *) redis_slot->addr_ip[i].data,redis_slot->addr_ip[i].len);
		lua_pushinteger(L, redis_slot->addr_port[i]);

	}else{
    	//lua_pushnil(L);
		return luaL_error(L, "redis no found");
	}

    return 2;
}

int failed_notify(lua_State * L)
{
    ngx_http_request_t  *r;
	ngx_str_t			upstream_name,key,info;
    uint16_t 			crc,slot;
	ngx_uint_t			i;
	ngx_http_lua_redis_slot_t	*redis_slot;

	if (lua_gettop(L) != 4) {
		return luaL_error(L, "expecting 4 arguments (including the object), "
						  "but got %d", lua_gettop(L));
	}
	if(pre_md5.len<=0){
		return luaL_error(L, "Redis cluster is not initialized");
	}
	r = ngx_http_lua_get_request(L);

	upstream_name.data = (u_char *) luaL_checklstring(L, 2, &upstream_name.len);
	if (upstream_name.len == 0) {
		lua_pushliteral(L, "empty upstream");
		return 1;
	}

	key.data = (u_char *) luaL_checklstring(L, 3, &key.len);

	if (key.len == 0) {
		lua_pushliteral(L, "empty key");
		return 1;
	}

	info.data = (u_char *) luaL_checklstring(L, 4, &info.len);

	if (info.len == 0) {
		//lua_pushnil(L);
		lua_pushliteral(L, "empty info");
		return 1;
	}

    crc = lua_utility_redisCRC16(key.data,key.len);

	slot = crc%16384;

	i = 0;
	for (; i < redis3_conf->redis_slots->nelts;i++)
	{
		redis_slot = ((ngx_http_lua_redis_slot_t*)redis3_conf->redis_slots->elts   ) + i;
		if(redis_slot->min <= slot && slot <= redis_slot->max){
			//ngx_log_error(NGX_LOG_INFO, r->pool->log, 0, "index:%d,min:%d,max:%d,addr0:%s:%d,addr1:%s:%d,slot:%d",i,redis_slot->min,(redis_slot)->max,(redis_slot)->addr0_ip,redis_slot->addr0_port,(redis_slot)->addr1_ip,redis_slot->addr1_port,slot);
			break;
		}
	}

	if(i<redis3_conf->redis_slots->nelts){
		lua_pushlstring(L, (const char *) redis_slot->addr_ip[0].data,redis_slot->addr_ip[0].len);
		if(ngx_strstr(info.data,"MISCONF")==0){
			redis_slot->failed_addr0=1;
		}
		lua_pushnil(L);
	}else{
    	//lua_pushnil(L);
		lua_pushliteral(L, "redis no found");
	}

    return 1;
}

char *
ngx_http_redis3_set_on(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t        *value;
    ngx_conf_post_t  *post;

	ngx_flag_t 		*fp;

	if(p!=NULL){
	    fp = (ngx_flag_t *) (p + cmd->offset);

	    if (*fp != NGX_CONF_UNSET) {
	        return "is duplicate";
	    }
	}
    value = cf->args->elts;

    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
		ngx_http_lua_redis3_enabled=1;
		pre_md5.data=ngx_palloc(ngx_cycle->pool,16);
    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
    	ngx_http_lua_redis3_enabled=0;
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



int build_slots(lua_State * L)
{
    ngx_http_request_t  		*r;
	ngx_str_t					data;
	u_char						*p;
	ngx_buf_t					*buff;
	ngx_int_t                   rc,r_total,i,j;
	ngx_array_t 	 			*redis_slot_array;
	ngx_http_lua_redis_slot_t	*redis_slot;

	if(!ngx_http_lua_redis3_enabled){
		return luaL_error(L, "redis3 enabled off");
	}

	if (lua_gettop(L) < 1) {
		return luaL_error(L, "expecting 1/2 arguments (including the object), "
						  "but got %d", lua_gettop(L));
	}
	r = ngx_http_lua_get_request(L);

	data.data = (u_char *) luaL_checklstring(L, 2, &data.len);
	if (data.len == 0) {
		return luaL_error(L, "empty data");
	}
	if(pre_md5.len>0){
		ngx_md5_t ctx;
		u_char result[16];
		ngx_md5_init(&ctx);
		ngx_md5_update(&ctx,data.data,data.len);
		ngx_md5_final(result,&ctx);
		if(ngx_memcmp(result,pre_md5.data,pre_md5.len)==0){
			lua_pushnil(L);
			lua_pushinteger(L, 0);
			return 2;
		}
	}

	p=data.data;
	while(*p!='*')p++;
	p++;
	r_total=0;
	REDIS_ATOI(p,r_total);
	p+=2;

	redis_slot_array = ngx_array_create(ngx_cycle->pool,r_total+1, sizeof(ngx_http_lua_redis_slot_t));
	if(redis_slot_array==NULL){
		if(rc != NGX_OK){
			return luaL_error(L, "Cannot allocate memory: %d",r_total+1);
		}
	}
	rc = ngx_http_lua_redis3_build_slots(data.data,data.len,p,r_total,redis_slot_array);
	if(rc != NGX_OK){
		lua_pushnil(L);
		lua_pushinteger(L, 1);
		ngx_array_destroy(redis_slot_array);
		return 2;
	}

	redis3_conf->redis_slots = redis_slot_array;
	i = 0;
	for (; i < redis3_conf->redis_slots->nelts;i++)
	{
		redis_slot = ((ngx_http_lua_redis_slot_t*)redis3_conf->redis_slots->elts)  + i;
		ngx_log_error(NGX_LOG_INFO, redis_slot_array->pool->log, 0, "index=%d,min=%d,max=%d",i,redis_slot->min,redis_slot->max);
		for(j=0;j<redis_slot->addr_num;j++){
			ngx_log_error(NGX_LOG_INFO, redis_slot_array->pool->log, 0, "addr%d=%s:%d",j,redis_slot->addr_ip[j].data,redis_slot->addr_port[j]);
		}
	}
	lua_pushnil(L);
	lua_pushinteger(L, 0);

	ngx_md5_t ctx;
	ngx_md5_init(&ctx);
	ngx_md5_update(&ctx,data.data,data.len);
	ngx_md5_final(pre_md5.data,&ctx);
	pre_md5.len=16;
    return 2;
}

