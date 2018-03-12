#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>
#include <ngx_http_lua_api.h>

#include <ndk_conf_file.h>
#include <uuid/uuid.h>
#include <stdbool.h>
#include "ngx_bizlog.h"

#include "ddebug.h"
#include <ngx_http_lua_util.h>
#include "ngx_lua_mqtt_kit_module.h"

/**
--- config
    location = /test {
        content_by_lua '
            local example = require "nginx.mqtt"
            ngx.say(example.get_uri())
        ';
    }
--- request
*/

ngx_flag_t ngx_http_mqtt_kit_enabled = 0;


ngx_module_t ngx_lua_mqtt_kit_module;
static ngx_str_t   server_addr;
static ngx_str_t   register_addr;

ngx_shm_zone_t     *ngx_lua_mqtt_kit_global_shm_zone ;

static char *ngx_conf_set_str_register(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_lua_mqtt_kit_set_num_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_lua_mqtt_kit_init(ngx_conf_t *cf);
static void *ngx_lua_mqtt_kit_create_main_conf(ngx_conf_t *cf);
static char *ngx_lua_mqtt_kit_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_lua_mqtt_kit_init_worker(ngx_cycle_t *cycle);
static void ngx_lua_mqtt_kit_exit_master(ngx_cycle_t *cycle);
static void ngx_lua_mqtt_kit_exit_worker(ngx_cycle_t *cycle);
static void* ngx_http_bizlog_create_srv_conf(ngx_conf_t *cf);
static char* ngx_http_bizlog_merge_srv_conf(ngx_conf_t *cf,void *parent, void *child);

//static ngx_str_t    ngx_lua_mqtt_kit_global_shm_name = ngx_string("lua_mqtt_module_global");
static ngx_conf_enum_t ngx_http_bizlog_loglevels[] = {
		{ngx_string("0"), 0},
		{ngx_string("1"), 1},
		{ngx_string("2"), 2},
		{ngx_string("3"), 3},
		{ngx_string("4"), 4},
		{ngx_string("5"), 5},
		{ngx_string("error"), 0},
		{ngx_string("warn"), 1},
		{ngx_string("info"), 2},
		{ngx_string("debug"), 3},
		{ngx_string("debug2"), 4},
		{ngx_string("all"), 5}
    };
    
static ngx_command_t  ngx_lua_mqtt_kit_commands[] = {
	{ ngx_string("disable_accept_worker_id"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_lua_mqtt_kit_set_num_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_lua_mqtt_kit_main_conf_t, workerId1),
		NULL },
    { ngx_string("bizlog"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_lua_mqtt_kit_svr_conf_t, enable),
      NULL },
    { ngx_string("bizlog_level"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot, 
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_lua_mqtt_kit_svr_conf_t, log_level),
      ngx_http_bizlog_loglevels},
    { ngx_string("bizlog_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_lua_mqtt_kit_svr_conf_t, logfile),
      NULL },
    { ngx_string("bizlog_debug_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_lua_mqtt_kit_svr_conf_t, debugfile),
      NULL },
    { ngx_string("register_addr"),
		NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_register,
		NGX_HTTP_SRV_CONF_OFFSET,
		0,
		NULL },
      ngx_null_command
};


static ngx_http_module_t ngx_lua_mqtt_kit_ctx = {
    NULL,                           		/* preconfiguration */
    ngx_lua_mqtt_kit_init,         		/* postconfiguration */
    ngx_lua_mqtt_kit_create_main_conf,		/* create main configuration */
    ngx_lua_mqtt_kit_init_main_conf,       /* init main configuration */
    ngx_http_bizlog_create_srv_conf,                           		/* create server configuration */
    ngx_http_bizlog_merge_srv_conf,                           		/* merge server configuration */
    NULL,                           		/* create location configuration */
    NULL                            		/* merge location configuration */
};


ngx_module_t ngx_lua_mqtt_kit_module = {
    NGX_MODULE_V1,
    &ngx_lua_mqtt_kit_ctx,  					/* module context */
    ngx_lua_mqtt_kit_commands,					/* module directives */
    NGX_HTTP_MODULE,            				/* module type */
    NULL,   									/* init master */
    NULL,              /* init module */
    ngx_lua_mqtt_kit_init_worker,              /* init process */
    NULL,                       				/* init thread */
    NULL,                       				/* exit thread */
    ngx_lua_mqtt_kit_exit_worker,             	/* exit process */
    ngx_lua_mqtt_kit_exit_master,             	/* exit master */
    NGX_MODULE_V1_PADDING
};

static void *ngx_lua_mqtt_kit_create_main_conf(ngx_conf_t *cf){
	ngx_lua_mqtt_kit_main_conf_t *mconf = ngx_pcalloc(cf->pool, sizeof(ngx_lua_mqtt_kit_main_conf_t));
	if (mconf == NULL) {
		return NULL;
	}
	mconf->workerId1=-1;
	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_lua_mqtt_kit_main_conf_t will be created.");

	return mconf;
}

static char *ngx_lua_mqtt_kit_init_main_conf(ngx_conf_t *cf, void *conf){
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return NGX_CONF_ERROR;
	}
	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_lua_mqtt_kit_main_conf_t will be initialized.");
	ngx_http_mqtt_kit_enabled =1;
	return NGX_CONF_OK;
}

static void* ngx_http_bizlog_create_srv_conf(ngx_conf_t *cf)
{
    ngx_lua_mqtt_kit_svr_conf_t  *conf;

    conf = (ngx_lua_mqtt_kit_svr_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_lua_mqtt_kit_svr_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
	conf->log_level = NGX_CONF_UNSET;
	conf->enable = NGX_CONF_UNSET;
	
	ngx_memzero(&conf->logfile, sizeof(ngx_str_t));
	ngx_memzero(&conf->debugfile, sizeof(ngx_str_t));
	
    return conf;
}

static char *
ngx_http_bizlog_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_lua_mqtt_kit_svr_conf_t *prev = (ngx_lua_mqtt_kit_svr_conf_t*)parent;
    ngx_lua_mqtt_kit_svr_conf_t *conf = (ngx_lua_mqtt_kit_svr_conf_t*)child;

	ngx_conf_merge_value(conf->log_level, prev->log_level, (ngx_int_t)NL_INFO);
	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_str_value(conf->logfile, prev->logfile, "logs/ngx_biz.log");
	ngx_conf_merge_str_value(conf->debugfile, prev->debugfile, "logs/ngx_biz.debug");

	if(conf->enable){
		return ngx_http_bizlog_init(cf, conf);
	}
   return NGX_CONF_OK;
}

static void ngx_lua_mqtt_kit_exit_master(ngx_cycle_t *cycle){}


static void ngx_lua_mqtt_kit_exit_worker(ngx_cycle_t *cycle)
{
	if(!ngx_http_mqtt_kit_enabled)return ;
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return;
	}
    if(mcf->workerId1!=(ngx_int_t)ngx_worker){
        return ;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

}


char *
ngx_lua_mqtt_kit_set_num_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_int_t        *np;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;
	
    np = (ngx_int_t *) (p + cmd->offset);

    if (*np != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    register_addr.len=0;
    server_addr.len=0;

    value = cf->args->elts;
    *np = ngx_atoi(value[1].data, value[1].len);
    if (*np == NGX_ERROR) {
        return "invalid number";
    }	
	ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "ngx_lua_mqtt_kit_set_num_slot will be call.");
    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, np);
    }	
	
    return NGX_CONF_OK;
}

static char *ngx_conf_set_str_register(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    //ngx_stream_socks5_srv_conf_t *pscf = conf;
    ngx_http_core_main_conf_t   *cmcf;
    ngx_http_conf_ctx_t         *ctx;
    ngx_http_core_srv_conf_t    *cscf;
    ngx_http_conf_port_t        *port;
    ngx_http_conf_addr_t        *addr;
    ngx_http_core_srv_conf_t    **server;
    ngx_str_t                   *value;
    ngx_uint_t                  i,j,k;

    if (register_addr.len > 0) {
        return "is duplicate";
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    ctx = cf->ctx;
    cscf = ctx->srv_conf[ngx_http_core_module.ctx_index];
    /*
    if (pscf->passwd_list) {
        return "is duplicate";
    }*/

    value = cf->args->elts;
    
    port=cmcf->ports->elts;
    
    for(i=0; i<cmcf->ports->nelts; ++i)
    {
        port=port+i;
        addr = port->addrs.elts;
        for (j = 0; j < port->addrs.nelts; j++) {
            server = addr[j].servers.elts;
            for (k = 0; k < addr[j].servers.nelts; k++) {
                if (server[k] == cscf) {
                    register_addr.data=value[1].data;
                    register_addr.len=value[1].len;
                    char *host=inet_ntoa(addr[j].opt.sockaddr.sockaddr_in.sin_addr);
                    if(ngx_memcmp(host,"0.0.0.0",7)==0)return "Need to configure the listener ip,for example: \"listen 192.168.0.1:80;\"";
                    server_addr.data=ngx_pcalloc(cf->pool, 30);
                    sprintf((char*)server_addr.data,"%s:%d",host,htons(addr[j].opt.sockaddr.sockaddr_in.sin_port));                    
                    server_addr.len=strlen((char*)server_addr.data);
                    //ngx_log_error(NGX_ERROR_ERR,cf->log,0,"remote IP:PORT = %s",server_addr.data);
                    
                    return NGX_CONF_OK;
                }
            }
        }
    }
    return "not found.";
}

static ngx_int_t ngx_lua_mqtt_kit_init_worker(ngx_cycle_t *cycle)
{
	ngx_core_conf_t     *ccf;

    if(g_biz_logger != NULL){
		dup2(g_biz_logger->fd, fileno(stderr));
	}
	if(g_biz_debuger != NULL){
		dup2(g_biz_debuger->fd, fileno(stdout));	
    }
    
	if(!ngx_http_mqtt_kit_enabled)return NGX_OK;
	ngx_lua_mqtt_kit_main_conf_t    *mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_lua_mqtt_kit_module);
	if (mcf == NULL) {
		return NGX_ERROR;
	}

	ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    if ((mcf->workerId1 >= 0) && (ccf->worker_processes <= 1)) {
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "worker_processes[%d] not be less than 1",mcf->workerId1);
        return NGX_ERROR;
    }

    if(mcf->workerId1!=(ngx_int_t)ngx_worker){
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }
	//ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "disable workerId: %d.",mcf->workerId1);
	do{
		if (ngx_shmtx_trylock(&ngx_accept_mutex)) {
			ngx_close_listening_sockets(cycle);
			ngx_shmtx_unlock(&ngx_accept_mutex);
			ngx_accept_mutex_held = 0;
			break;
		}
		usleep(5);
	}while(1);
	//
	ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "disable workerId[%d]: %d.",mcf->workerId1,getpid());
	return NGX_OK;
    //return ngx_disable_accept_events(cycle);
}



static int ipc_test(lua_State * L)
{
	//ngx_http_request_t  *r;

    //r = ngx_http_lua_get_request(L);
	return 0;
}
static int
log_wrapper(const char *ident, ngx_uint_t level,
    lua_State *L)
{
    u_char              *buf;
    u_char              *p, *q;
    ngx_str_t            name;
    int                  nargs, i;
    size_t               size, len;
    size_t               src_len = 0;
    int                  type;
    const char          *msg;
    lua_Debug            ar;



#if 1
    /* add debug info */

    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "Snl", &ar);

    /* get the basename of the Lua source file path, stored in q */
    name.data = (u_char *) ar.short_src;
    if (name.data == NULL) {
        name.len = 0;

    } else {
        p = name.data;
        while (*p != '\0') {
            if (*p == '/' || *p == '\\') {
                name.data = p + 1;
            }
            p++;
        }

        name.len = p - name.data;
    }

#endif

    nargs = lua_gettop(L);

    size = name.len + NGX_INT_T_LEN + sizeof(":: ") - 1;

    if (*ar.namewhat != '\0' && *ar.what == 'L') {
        src_len = ngx_strlen(ar.name);
        size += src_len + sizeof("(): ") - 1;
    }

    for (i = 1; i <= nargs; i++) {
        type = lua_type(L, i);
        switch (type) {
            case LUA_TNUMBER:
            case LUA_TSTRING:
                lua_tolstring(L, i, &len);
                size += len;
                break;

            case LUA_TNIL:
                size += sizeof("nil") - 1;
                break;

            case LUA_TBOOLEAN:
                if (lua_toboolean(L, i)) {
                    size += sizeof("true") - 1;

                } else {
                    size += sizeof("false") - 1;
                }

                break;

            case LUA_TTABLE:
                if (!luaL_callmeta(L, i, "__tostring")) {
                    return luaL_argerror(L, i, "expected table to have "
                                         "__tostring metamethod");
                }

                lua_tolstring(L, -1, &len);
                size += len;
                break;

            case LUA_TLIGHTUSERDATA:
                if (lua_touserdata(L, i) == NULL) {
                    size += sizeof("null") - 1;
                    break;
                }

                continue;

            default:
                msg = lua_pushfstring(L, "string, number, boolean, or nil "
                                      "expected, got %s",
                                      lua_typename(L, type));
                return luaL_argerror(L, i, msg);
        }
    }

    buf = lua_newuserdata(L, size);

    p = ngx_copy(buf, name.data, name.len);

    *p++ = ':';

    p = ngx_snprintf(p, NGX_INT_T_LEN, "%d",
                     ar.currentline ? ar.currentline : ar.linedefined);

    *p++ = ':'; *p++ = ' ';

    if (*ar.namewhat != '\0' && *ar.what == 'L') {
        p = ngx_copy(p, ar.name, src_len);
        *p++ = '(';
        *p++ = ')';
        *p++ = ':';
        *p++ = ' ';
    }

    for (i = 1; i <= nargs; i++) {
        type = lua_type(L, i);
        switch (type) {
            case LUA_TNUMBER:
            case LUA_TSTRING:
                q = (u_char *) lua_tolstring(L, i, &len);
                p = ngx_copy(p, q, len);
                break;

            case LUA_TNIL:
                *p++ = 'n';
                *p++ = 'i';
                *p++ = 'l';
                break;

            case LUA_TBOOLEAN:
                if (lua_toboolean(L, i)) {
                    *p++ = 't';
                    *p++ = 'r';
                    *p++ = 'u';
                    *p++ = 'e';

                } else {
                    *p++ = 'f';
                    *p++ = 'a';
                    *p++ = 'l';
                    *p++ = 's';
                    *p++ = 'e';
                }

                break;

            case LUA_TTABLE:
                luaL_callmeta(L, i, "__tostring");
                q = (u_char *) lua_tolstring(L, -1, &len);
                p = ngx_copy(p, q, len);
                break;

            case LUA_TLIGHTUSERDATA:
                *p++ = 'n';
                *p++ = 'u';
                *p++ = 'l';
                *p++ = 'l';

                break;

            default:
                return luaL_error(L, "impossible to reach here");
        }
    }

    if (p - buf > (off_t) size) {
        return luaL_error(L, "buffer error: %d > %d", (int) (p - buf),
                          (int) size);
						}
	
	if(level>=NGX_LOG_DEBUG) {
		if(g_NlogLevel>=NL_DEBUG2)NCPrintBig(NWriteDebugLog,"DEBUG", "%s%*s", ident, (size_t) (p - buf), buf);
		else if(g_NlogLevel>=NL_DEBUG)NCPrint(NWriteDebugLog,"DEBUG", "%s%*s", ident, (size_t) (p - buf), buf);
    }else if(level>=NGX_LOG_INFO) {
		if(g_NlogLevel>=NL_INFO)NCPrint(NWriteLog,"INFO", "%s%*s", ident, (size_t) (p - buf), buf);
    }else if(level>=NGX_LOG_WARN) {
		if(g_NlogLevel>=NL_WARN)NCPrint(NWriteLog,"WARN", "%s%*s", ident, (size_t) (p - buf), buf);
    }else if(level>=NGX_LOG_ERR) {
		if(g_NlogLevel>=NL_ERROR)NCPrint(NWriteLog,"ERROR", "%s%*s", ident, (size_t) (p - buf), buf);
    }
    return 0;
}

int
ngx_http_lua_bizlog(lua_State *L)
{
    //ngx_http_request_t          *r;
    const char                  *msg;
    int                          level;

    //r = ngx_http_lua_get_req(L);


    level = luaL_checkint(L, 1);
    if (level < NGX_LOG_STDERR || level > NGX_LOG_DEBUG) {
        msg = lua_pushfstring(L, "bad log level: %d", level);
        return luaL_argerror(L, 1, msg);
	}
	
	if(g_NlogLevel<=NL_ERROR && level>NGX_LOG_ERR)return 0;
	else if(g_NlogLevel<=NL_INFO && level>NGX_LOG_INFO)return 0;
	else if(g_NlogLevel<=NL_DEBUG && level>NGX_LOG_DEBUG)return 0;

    /* remove log-level param from stack */
    lua_remove(L, 1);

    return log_wrapper("[lua] ", (ngx_uint_t) level, L);
}

static int get_server_addr(lua_State *L) {
    lua_pushlstring(L, (const char *)register_addr.data,register_addr.len);
    lua_pushlstring(L, (const char *)server_addr.data,server_addr.len);
    
  // lua_rawset(L, -1);
    return 2;
}

static int luaopen_nginx_mqtt(lua_State * L)
{
    lua_createtable(L, 0, 1);
	lua_pushcfunction(L, ipc_test);
	lua_setfield(L, -2, "ipc_test");
	lua_pushcfunction(L, ngx_http_lua_bizlog);
    lua_setfield(L, -2, "log");
    lua_pushcfunction(L, get_server_addr);
    lua_setfield(L, -2, "get_register_addr");
    return 1;
}

static ngx_int_t
ngx_lua_mqtt_kit_init(ngx_conf_t *cf)
{
	if(!ngx_http_mqtt_kit_enabled)return NGX_OK;
	dd("lua_add_package_preload nginx.mqtt_kit");
    if (ngx_http_lua_add_package_preload(cf, "nginx.mqtt_kit",
                                         luaopen_nginx_mqtt)
        != NGX_OK)
    {
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "lua_add_package_preload nginx.mqtt_kit failued");
        return NGX_ERROR;
    }

    return NGX_OK;
}

int ngx_lua_utility_indexOf(const u_char *str1,const u_char *str2)
{
	u_char *p;
	int i=0;
	p=(u_char *)str1;
	p=(u_char *)ngx_strstr(str1,str2);
	if(p==NULL)
		return -1;
	else{
		while(str1!=p)
		{
			str1++;
			i++;
		}
	}
	return i;
}

static char chars[] = { 
	'a','b','c','d','e','f','g','h',  
	'i','j','k','l','m','n','o','p',  
	'q','r','s','t','u','v','w','x',  
	'y','z','0','1','2','3','4','5',  
	'6','7','8','9','A','B','C','D',  
	'E','F','G','H','I','J','K','L',  
	'M','N','O','P','Q','R','S','T',  
	'U','V','W','X','Y','Z' 
}; 

void uuid(char *result, int len)
{
	unsigned char uuid[16];
	char output[24];
	const char *p = (const char*)uuid;

	uuid_generate(uuid);
	memset(output, 0, sizeof(output));

	int i, j;
	for (j = 0; j < 2; j++)
	{
		unsigned long long v = *(unsigned long long*)(p + j*8);
		int begin = j * 10;
		int end =  begin + 10;

		for (i = begin; i < end; i++)
		{
			int idx = 0X3D & v;
			output[i] = chars[idx];
			v = v >> 6;
		}
	}
	//printf("%s\n", output);
	len = (len > (int)sizeof(output)) ? (int)sizeof(output) :  len;
	memcpy(result, output, len);
}

void uuid8(char *result) 
{
	uuid(result, 8);
	result[8] = '\0';
}

void uuid20(char *result) 
{
	uuid(result, 20);
	result[20] = '\0';
}


int ngx_lua_utility_uuid(char *buffer)
{
	uuid(buffer, 20);
	buffer[20] = '\0';
	return 0;
}
