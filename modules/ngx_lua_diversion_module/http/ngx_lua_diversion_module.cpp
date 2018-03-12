#include <set>
/*namespace std
{
 using namespace __gnu_cxx;
}*/
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include <ngx_channel.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_lua_api.h>
#include <ngx_http_lua_shdict.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ddebug.h"
#include "mqtt_protocol.h"
#include "ngx_lua_diversion_module.h"
#include <ngx_bizlog.h>

#define DIVS_PTYPE_RESERVED 9999
typedef enum {
  ST_NONE, ST_RANGE,ST_BIT_SET,ST_NUM_SET,ST_RBTREE,ST_SUFFIX,ST_RESERVED
} div_store_type_enum_t;

#define ngx_http_diversion_conf_cookies_len 3
static ngx_conf_enum_t ngx_http_diversion_conf_cookies[] = {
		{ngx_string("aid="), 0},
		{ngx_string("cid="), 1},
		{ngx_string("oid="), 2}
  };

#define LOG_USER_INFO(userInfo) NLOG_INFO("user info: {ver=%l, ip=%l, aid=%l, cid=%l, oid=%l, uri=%s}", \
      userInfo.ver, userInfo.req_val[0], userInfo.req_val[1],userInfo.req_val[2],userInfo.req_val[3], userInfo.uri.data)

typedef struct {
    ngx_str_t         uri;
    uint32_t					ver;    
    uint64_t          req_val[ngx_http_diversion_conf_cookies_len+1];//+1 uint32_t					ip;
  } ngx_div_user_info_t;

typedef struct {
    ngx_str_t                 name;
    ngx_uint_t                value; // ptype index
    ngx_int_t                cvalue; // user info index
    div_store_type_enum_t     stype; // store type
  } ngx_div_conf_enum_t;
  
static ngx_div_conf_enum_t ngx_http_diversion_conf_ptypes[] = {
		//{ngx_string("uri"), 1},
		{ngx_string("ipset"), 0, 0 ,ST_NUM_SET},
    {ngx_string("iprange"), 1, 0,ST_RANGE},
    {ngx_string("bitset"), 2, 0,ST_BIT_SET},
		{ngx_string("aidset"), 3, 1,ST_RBTREE},
    {ngx_string("aidrange"), 4, 1,ST_RANGE},
    {ngx_string("aidsuffix"), 5,1,ST_SUFFIX},
    {ngx_string("cidset"), 6,2,ST_RBTREE},
    {ngx_string("cidrange"), 7,2,ST_RANGE},
    {ngx_string("cidsuffix"), 8,2,ST_SUFFIX},
    {ngx_string("oidset"), 9,3,ST_RBTREE}
  };

static ngx_uint_t ngx_http_diversion_conf_ptypes_len = sizeof(ngx_http_diversion_conf_ptypes)/sizeof(ngx_http_diversion_conf_ptypes[0]);

static ngx_flag_t ngx_http_diversion_enabled = 0;

static ngx_shm_zone_t *diversion_sysconf_shm_zone;
//static ngx_shm_zone_t *diversion_userups_shm_zone;
static ngx_shm_zone_t *diversion_bitset_shm_zone;

static ngx_int_t ngx_http_lua_shdict_lookup(ngx_shm_zone_t *shm_zone,
                                            ngx_uint_t hash, u_char *kdata,
                                            size_t klen,
                                            ngx_http_lua_shdict_node_t **sdp);
static ngx_int_t ngx_http_mqtt_kit_shdict_store(ngx_shm_zone_t *zone, uint32_t hash,
                                          ngx_str_t *key, ngx_str_t *value,
                                          uint32_t _exptime);

static ngx_int_t ngx_lua_diversion_init(ngx_conf_t *cf);
static void *ngx_lua_diversion_create_main_conf(ngx_conf_t *cf);
static char *ngx_lua_diversion_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_lua_diversion_init_worker(ngx_cycle_t *cycle);
static void ngx_lua_diversion_exit_master(ngx_cycle_t *cycle);
static void ngx_lua_diversion_exit_worker(ngx_cycle_t *cycle);
static char *ngx_http_lua_diversion_set_env(ngx_conf_t *cf, ngx_command_t *cmd,
                                            void *conf);
static char *ngx_http_lua_diversion_set_sysconf_shm(ngx_conf_t *cf,
                                                    ngx_command_t *cmd,
                                                    void *conf);
static char *ngx_http_lua_diversion_set_bitset_shm(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_int_t diversion_exec_policy(ngx_div_user_info_t *userInfo,
                                  diversion_cacl_res_t *out);
static ngx_int_t diversion_decode_policy(ngx_str_t *ibuff, uint32_t ver);
static ngx_int_t diversion_decode_runtimeinfo(ngx_str_t *ibuff);

static ngx_int_t ngx_http_lua_diversion_init_zone(ngx_shm_zone_t *shm_zone,
                                                  void *data);
static ngx_int_t
ngx_http_lua_diversion_init_bitset_zone(ngx_shm_zone_t *shm_zone, void *data);
static ngx_inline void ngx_rbtree_clear_4diversion(ngx_rbtree_t *rbtree)  ;
static ngx_inline  ngx_rbtree_node_t* ngx_rbtree_lookup_4diversion(ngx_rbtree_t *rbtree, uint64_t key)  ;

static ngx_str_t div_env;
static diversion_runtime_info_t *global_runtimeinfo;
static bit_vector_t *global_bitvector_1;

//static ngx_temp_file_t *tf;

#define DIVERSION_POLICY_GROUP_T_LEN sizeof(diversion_policy_group_t)
#define DIVERSION_POLICY_T_LEN sizeof(diversion_policy_t)

static u_char *ngx_shmpool_strdup(ngx_slab_pool_t *pool, ngx_str_t *src) {
  u_char *dst;

  dst = (u_char *)ngx_slab_alloc(pool, src->len+1);
  if (dst == NULL) {
    return NULL;
  }

  ngx_memcpy(dst, src->data, src->len);
  dst[src->len]='\0';
  return dst;
}

static ngx_command_t ngx_lua_diversion_commands[] = {
    {ngx_string("diversion_env"), NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_http_lua_diversion_set_env, NGX_HTTP_MAIN_CONF_OFFSET, 0, NULL},
    {ngx_string("diversion_sysconf_shm"), NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE3,
     ngx_http_lua_diversion_set_sysconf_shm, NGX_HTTP_MAIN_CONF_OFFSET, 0,
     NULL},
    {ngx_string("diversion_bitset_shm"), NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE2,
     ngx_http_lua_diversion_set_bitset_shm, NGX_HTTP_MAIN_CONF_OFFSET, 0,
     NULL},
    ngx_null_command};

static ngx_http_module_t ngx_lua_diversion_ctx = {
    NULL,                               /* preconfiguration */
    ngx_lua_diversion_init,             /* postconfiguration */
    ngx_lua_diversion_create_main_conf, /* create main configuration */
    ngx_lua_diversion_init_main_conf,   /* init main configuration */
    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */
    NULL,                               /* create location configuration */
    NULL                                /* merge location configuration */
};

ngx_module_t ngx_lua_diversion_module = {
    NGX_MODULE_V1,
    &ngx_lua_diversion_ctx,        /* module context */
    ngx_lua_diversion_commands,    /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    ngx_lua_diversion_init_worker, /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    ngx_lua_diversion_exit_worker, /* exit process */
    ngx_lua_diversion_exit_master, /* exit master */
    NGX_MODULE_V1_PADDING};

static void *ngx_lua_diversion_create_main_conf(ngx_conf_t *cf) {
  ngx_lua_diversion_main_conf_t *mconf =
      (ngx_lua_diversion_main_conf_t *)ngx_pcalloc(
          cf->pool, sizeof(ngx_lua_diversion_main_conf_t));
  if (mconf == NULL) {
    return NULL;
  }
  mconf->workerId = -1;
  ngx_log_error(NGX_LOG_DEBUG, cf->log, 0,
                "ngx_lua_diversion_main_conf_t will be created.");

  return mconf;
}

static char *ngx_lua_diversion_init_main_conf(ngx_conf_t *cf, void *conf) {
  ngx_lua_diversion_main_conf_t *mcf =
      (ngx_lua_diversion_main_conf_t *)ngx_http_conf_get_module_main_conf(
          cf, ngx_lua_diversion_module);
  if (mcf == NULL) {
    return (char *)NGX_CONF_ERROR;
  }

  ngx_log_error(NGX_LOG_DEBUG, cf->log, 0,
                "ngx_lua_diversion_main_conf_t will be initialized.");

  return (char *)NGX_CONF_OK;
}

static void ngx_lua_diversion_exit_master(ngx_cycle_t *cycle) {}

static void ngx_lua_diversion_exit_worker(ngx_cycle_t *cycle) {
  if (!ngx_http_diversion_enabled)
    return;
  global_runtimeinfo->lock = 0;

  ngx_lua_diversion_main_conf_t *mcf =
      (ngx_lua_diversion_main_conf_t *)ngx_http_cycle_get_module_main_conf(
          cycle, ngx_lua_diversion_module);
  if (mcf == NULL) {
    return;
  }
}

static ngx_int_t ngx_lua_diversion_init_worker(ngx_cycle_t *cycle) {
  if (!ngx_http_diversion_enabled)
    return NGX_OK;
  ngx_lua_diversion_main_conf_t *mcf =
      (ngx_lua_diversion_main_conf_t *)ngx_http_cycle_get_module_main_conf(
          cycle, ngx_lua_diversion_module);
  if (mcf == NULL) {
    return (ngx_int_t)NGX_CONF_ERROR;
  }
  return NGX_OK;
  // return ngx_disable_accept_events(cycle);
}

static ngx_int_t ngx_http_lua_diversion_init_vh_zone(ngx_shm_zone_t *shm_zone,
                                                     void *data) {
  ngx_slab_pool_t *shpool;

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  global_runtimeinfo->vhshpool = shpool;
  if (data) { // reload
    NLOG_INFO("reload runtimeinfo->vhshpool address: %p",(ngx_slab_pool_t *)shm_zone->shm.addr);
    return NGX_OK;
  }

  ngx_slab_init(shpool);
  global_runtimeinfo->envhosts_len = 0;
  return NGX_OK;
}

static char *ngx_http_lua_diversion_set_env(ngx_conf_t *cf, ngx_command_t *cmd,
                                            void *conf) {
  char *p = (char *)conf;
  ngx_str_t *value;
  ngx_conf_post_t *post;
  ngx_flag_t *fp;

  fp = NULL;
  if (p != NULL) {
    fp = (ngx_flag_t *)(p + cmd->offset);

    if (*fp != NGX_CONF_UNSET) {
      return (char *)"is duplicate";
    }
  }
  value = (ngx_str_t *)cf->args->elts;

  div_env.data = value[1].data;
  div_env.len = value[1].len;
  ngx_http_diversion_enabled = 1;

  if (cmd->post) {
    post = (ngx_conf_post_t *)cmd->post;
    return post->post_handler(cf, post, fp);
  }

  return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_lua_diversion_init_sysconf_zone(ngx_shm_zone_t *shm_zone, void *data) {
  ngx_slab_pool_t *shpool;
  diversion_runtime_info_t *runtimeinfo;

  runtimeinfo = (diversion_runtime_info_t *)data;
  if (data) { // reload
    NLOG_INFO("reload runtimeinfo shpool address: %p",(ngx_slab_pool_t *)shm_zone->shm.addr);
    shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;

    /*
    ngx_slab_init(shpool);
    runtimeinfo = oruntimeinfo;
    goto init_sysconf_resetmem;
    */

    global_runtimeinfo = runtimeinfo;
    runtimeinfo->lock = 0;

    //global_runtimeinfo->mutex = &shpool->mutex;
    return NGX_OK;
  }

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  dd("alloc init_sysconf shpool address: %p", shpool);

  runtimeinfo = (diversion_runtime_info_t *)ngx_slab_alloc(
      shpool, sizeof(diversion_runtime_info_t));
  if (runtimeinfo == NULL) {
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "can't alloc memory: %d",
                  sizeof(diversion_runtime_info_t));
    return NGX_ERROR;
  }

  dd("reload init_sysconf mutex address: %p", &shpool->mutex);
  // diversion->shpool = shpool;
  shm_zone->data = runtimeinfo;

//init_sysconf_resetmem:
  ngx_memzero(runtimeinfo, sizeof(diversion_runtime_info_t));

  runtimeinfo->status = -1;
  runtimeinfo->id = 0;
  runtimeinfo->version = 0;
  runtimeinfo->policy_grouplen = 0;
  runtimeinfo->envhosts_len = 0;
  runtimeinfo->src.len = 0;
  runtimeinfo->to.len = 0;
  runtimeinfo->mutex = &shpool->mutex;
  runtimeinfo->lock = 0;

  global_runtimeinfo = runtimeinfo;
  return NGX_OK;
}

static ngx_int_t ngx_http_lua_diversion_init_pg1_zone(ngx_shm_zone_t *shm_zone,
                                                      void *data) {
  ngx_slab_pool_t *shpool;

  if (data) { // reload
    NLOG_INFO("reload runtimeinfo->pgshpool address: %p", data);
    global_runtimeinfo->pgshpool = (ngx_slab_pool_t *)data;
    //ngx_slab_init(global_runtimeinfo->pgshpool);
    //goto init_pg1_initmem;
    return NGX_OK;
  }

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  dd("alloc init_pg1 shpool address: %p", shpool);

  global_runtimeinfo->pgshpool = shpool;
  shm_zone->data = shpool;

//init_pg1_initmem:
  global_runtimeinfo->policy_grouplen = 0;
  return NGX_OK;
}
static ngx_str_t vh_name = ngx_string("DIVS_VH__XX"),
                 pg1_shname = ngx_string("DIVS_PG1__XX");
static char *ngx_http_lua_diversion_set_sysconf_shm(ngx_conf_t *cf,
                                                    ngx_command_t *cmd,
                                                    void *conf) {
  char *p = (char *)conf;
  ngx_str_t *value, name;
  size_t size,pgSize;
  ngx_conf_post_t *post;
  ngx_flag_t *fp;
  ngx_shm_zone_t *shm_zone_pg1, *shm_zone_vh;

  fp = NULL;
  if (p != NULL) {
    fp = (ngx_flag_t *)(p + cmd->offset);
    if (*fp != NGX_CONF_UNSET) {
      return (char *)"is duplicate";
    }
  }
  value = (ngx_str_t *)cf->args->elts;
  name.data = value[1].data;
  name.len = value[1].len;
  size = ngx_parse_size(&value[2]);
  pgSize = ngx_parse_size(&value[3]);

  if (!ngx_http_diversion_enabled) {
    return (char *)"'diversion_env' must be configured";
  }

  diversion_sysconf_shm_zone =
      ngx_shared_memory_add(cf, &name, size, &ngx_lua_diversion_module);
  if (diversion_sysconf_shm_zone == NULL) {
    return (char *)NGX_CONF_ERROR;
  }
  diversion_sysconf_shm_zone->init = ngx_http_lua_diversion_init_sysconf_zone;

  shm_zone_pg1 =
      ngx_shared_memory_add(cf, &pg1_shname, pgSize, &ngx_lua_diversion_module);
  if (shm_zone_pg1 == NULL) {
    return (char *)NGX_CONF_ERROR;
  }
  shm_zone_pg1->init = ngx_http_lua_diversion_init_pg1_zone;

  // ctx为本地的配置文件，而ctx中保存共享内存的地址
  // 即ctx->sh与ctx->shpool
  // 这样通过共享内存就可以拿到对应的ctx了
  // diversion_sysconf_shm_zone->data = ctx;
  shm_zone_vh =
      ngx_shared_memory_add(cf, &vh_name, size, &ngx_lua_diversion_module);
  if (shm_zone_vh == NULL) {
    return (char *)NGX_CONF_ERROR;
  }
  shm_zone_vh->init = ngx_http_lua_diversion_init_vh_zone;

  if (cmd->post) {
    post = (ngx_conf_post_t *)cmd->post;
    return post->post_handler(cf, post, fp);
  }

  return NGX_CONF_OK;
}

static char *ngx_http_lua_diversion_set_bitset_shm(ngx_conf_t *cf,
                                                   ngx_command_t *cmd,
                                                   void *conf) {
  char *p = (char *)conf;

  ngx_str_t *value, name;
  size_t size;
  ngx_conf_post_t *post;

  ngx_flag_t *fp;
  fp = NULL;
  if (p != NULL) {
    fp = (ngx_flag_t *)(p + cmd->offset);

    if (*fp != NGX_CONF_UNSET) {
      return (char *)"is duplicate";
    }
  }
  value = (ngx_str_t *)cf->args->elts;

  name.data = value[1].data;
  name.len = value[1].len;
  size = ngx_parse_size(&value[2]);

  if (diversion_sysconf_shm_zone == NULL) {
    return (char *)"'diversion_sysconf_shm' must be configured";
  }

  diversion_bitset_shm_zone =
      ngx_shared_memory_add(cf, &name, size, &ngx_lua_diversion_module);

  if (diversion_bitset_shm_zone == NULL) {
    return (char *)NGX_CONF_ERROR;
  }

  // 设置初始化函数
  diversion_bitset_shm_zone->init = ngx_http_lua_diversion_init_bitset_zone;

  if (cmd->post) {
    post = (ngx_conf_post_t *)cmd->post;
    return post->post_handler(cf, post, fp);
  }

  return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_lua_diversion_init_bitset_zone(ngx_shm_zone_t *shm_zone, void *data) {
  ngx_slab_pool_t *shpool;

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;

  if (data) {
    //utility_bit_vector_clear(global_bitvector_1);
    shm_zone->data = shpool;
    return NGX_OK;
  }
  global_bitvector_1 =
      (bit_vector_t *)ngx_slab_alloc(shpool, sizeof(bit_vector_t));
  if (global_bitvector_1 == NULL) {
    ngx_log_error(NGX_LOG_ERR, shm_zone->shm.log, 0,
                  "can't alloc 'bit_vector_t' memory: %d",
                  sizeof(bit_vector_t));
    return NGX_ERROR;
  }

  uint64_t pos = UINT_MAX;
  if(utility_bit_vector_init(shpool, global_bitvector_1, pos)==-1){
    ngx_log_error(NGX_LOG_ERR, shm_zone->shm.log, 0,
                  "can't init&alloc 'global_bitvector_1' memory: %l",
                  pos);
                  return NGX_ERROR;
  }

  shm_zone->data = shpool;

  return NGX_OK;
}
static ngx_int_t ngx_http_lua_diversion_init_zone(ngx_shm_zone_t *shm_zone,
                                                  void *data) {
  return NGX_OK;
}

ngx_inline static long div_atol(u_char *str, ngx_int_t len) {
  char *t, *h, *p, tmp;
  long l;
  ngx_int_t flg = 0;

  t = (char *)str + len - 1;
  h = (char *)str;
  do {
    if (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') {
      t--;
      flg++;
    }
    if (*h == ' ' || *h == '\t') {
      h++;
      flg++;
    }
    if (flg == 0) {
      break;
    }
    flg = 0;
  } while (t != h);

  if (t < (char *)str + len - 1) {
    tmp = t[1];
    t[1] = 0;
  }
  for (p = h; p <= t; p++) {
    if (!(*p >= '0' && *p <= '9')) // isdigit(*p))
    {
      ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "%s,%d not digit!", str,
                    len);
      if (t < (char *)str + len - 1) {
        t[1] = tmp;
      }
      return 0;
    }
  }

  l = atol(h);

  if (t < (char *)str + len - 1) {
    t[1] = tmp;
  }

  return l;
  // memcpy( str, h, ( t - h + 2 ) * sizeof( char) );
}

static int exec_policy(lua_State *L) {
  ngx_http_request_t *r;
  ngx_table_elt_t **cookies = NULL;
  ngx_table_elt_t **tab_ip = NULL;
  ngx_str_t strip = {0, NULL};
  ngx_uint_t rc, i;
  double ver;
  u_char *str, *p, *t,c_tmp;
  size_t plen;

  ngx_div_user_info_t userInfo;
  diversion_cacl_res_t out;

  r = ngx_http_lua_get_request(L);
  rc = lua_gettop(L);

  if (rc != 1) {
    return luaL_error(L, "expecting 1 arguments, but only seen %d", rc);
  }

  ver = luaL_checknumber(L, 1);

  memset(&userInfo,0,sizeof(ngx_div_user_info_t));
  userInfo.ver = ver;
  userInfo.uri.data = r->uri.data;
  userInfo.uri.len = r->uri.len;
  // XXXX
  c_tmp = r->uri.data[r->uri.len];
  r->uri.data[r->uri.len]='\0';
  // XXXX
  dd("Cookie count: %zd", r->headers_in.cookies.nelts);
  cookies = (ngx_table_elt_t **)r->headers_in.cookies.elts;
  for (i = 0; i < r->headers_in.cookies.nelts; i++) {
    dd("Cookie line %d: %s", i, cookies[i]->value.data);
    plen = cookies[i]->value.len;
    str = cookies[i]->value.data;
    t = cookies[i]->value.data + cookies[i]->value.len;
    do {
      p = ngx_strnstr(str, (char *)";",
                      cookies[i]->value.len - (str - cookies[i]->value.data));
      if (p != NULL) {
        plen = p - str;
        dd("Cookie %zd: %s", plen, p);
      }
      dd("Cookie %zd: %s", plen, str);
      for(int j=ngx_http_diversion_conf_cookies_len-1;j>=0;j--){
        if (ngx_memcmp(str, ngx_http_diversion_conf_cookies[j].name.data,ngx_http_diversion_conf_cookies[j].name.len ) == 0) {
          userInfo.req_val[j+1] = div_atol(str + ngx_http_diversion_conf_cookies[j].name.len, plen - ngx_http_diversion_conf_cookies[j].name.len);
          break;
        }
      }

      if (p != NULL) {
        str = p + 1;
        plen = (t - str);
        do {
          if (*str != ' ' and *str != '\t') {
            break;
          } else {
            str++;
            plen--;
          }
        } while (1);
      }
    } while (p != NULL);
  }

  /*
  if (userInfo.aid > 0) {
    rc = find_userups(&userInfo, &out);
    if (rc == NGX_OK) {
      lua_pushlstring(L, (const char *)out.backend.data, out.backend.len);
      lua_pushnumber(L, out.action);
      return 2;
    }
  }
  */

  if (r->headers_in.x_real_ip && r->headers_in.x_real_ip->value.len > 0) {
    strip.len = r->headers_in.x_real_ip->value.len;
    strip.data = r->headers_in.x_real_ip->value.data;

    struct in_addr inp;
    if (inet_aton((char *)strip.data, &inp) != 0) {
      //userInfo.ip = ntohl(inp.s_addr);
      userInfo.req_val[0]=ntohl(inp.s_addr);
    }
  } else {
    dd("x_forwarded_for count: %d", r->headers_in.x_forwarded_for.nelts);
    tab_ip = (ngx_table_elt_t **)r->headers_in.x_forwarded_for.elts;

    for (i = 0; i < r->headers_in.x_forwarded_for.nelts; i++) {
      dd("x_forwarded_for line %d: %s", i, tab_ip[i]->value.data);
      strip.len = tab_ip[i]->value.len;
      strip.data = tab_ip[i]->value.data;

      str = (u_char *)ngx_strnstr(strip.data, (char *)",", strip.len);
      char c;
      if (str) {
        strip.len = str - strip.data;
        c = strip.data[strip.len];
        strip.data[strip.len] = 0;
      }

      struct in_addr inp;
      if (inet_aton((char *)strip.data, &inp) != 0) {
        //userInfo.ip = ntohl(inp.s_addr);
        userInfo.req_val[0]=ntohl(inp.s_addr);
      }

      if (str) {
        strip.data[strip.len] = c;
      }
      break;
    }
  }
  if (strip.len == 0) {
    struct sockaddr_in *sin = (struct sockaddr_in *)r->connection->sockaddr;
    //userInfo.ip = ntohl(sin->sin_addr.s_addr);
    userInfo.req_val[0]=ntohl(sin->sin_addr.s_addr);
  }

  LOG_USER_INFO(userInfo);

  rc = diversion_exec_policy(&userInfo, &out);

  // XXXX
  r->uri.data[r->uri.len]=c_tmp;
  // XXXX
  if (rc != NGX_OK || out.backend.len == 0) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnumber(L, 0);
    lua_pushliteral(L, "error");
    return 4;
  } 
    /*
    if (userInfo.aid > 0) {
      set_userups(&userInfo, &out);
    }*/
    lua_pushlstring(L, (const char *)out.backend.data, out.backend.len);
    lua_pushlstring(L, (const char *)out.text.data, out.text.len);
    lua_pushnumber(L, out.action);
    return 3;
}

static int get_env(lua_State *L) {
  if (div_env.len == 0) {
    lua_pushnil(L);
  } else
    lua_pushlstring(L, (const char *)div_env.data, div_env.len);
  return 1;
}

static ngx_str_t diversion_version_file = ngx_string("logs/diversion.dat");
ngx_int_t ngx_lua_diversion_update_file(uint32_t ver, ngx_log_t *log) {

  size_t len;
  ngx_uint_t create;
  ngx_file_t file;
  u_char pid[NGX_INT64_LEN + 2];

  if (ngx_process > NGX_PROCESS_MASTER) {
    return NGX_OK;
  }

  ngx_memzero(&file, sizeof(ngx_file_t));

  file.name = diversion_version_file;
  file.log = log;

  create = ngx_test_config ? NGX_FILE_CREATE_OR_OPEN : NGX_FILE_TRUNCATE;

  file.fd = ngx_open_file(file.name.data, NGX_FILE_RDWR, create,
                          NGX_FILE_DEFAULT_ACCESS);

  if (file.fd == NGX_INVALID_FILE) {
    ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                  ngx_open_file_n " \"%s\" failed", file.name.data);
    return NGX_ERROR;
  }

  if (!ngx_test_config) {
    len = ngx_snprintf(pid, NGX_INT64_LEN + 2, "%P%N", ver) - pid;

    if (ngx_write_file(&file, pid, len, 0) == NGX_ERROR) {
      return NGX_ERROR;
    }
  }

  if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
    ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                  ngx_close_file_n " \"%s\" failed", file.name.data);
  }

  return NGX_OK;
}

static int update_runtimeinfo(lua_State *L) {
  ngx_str_t ibuff;
  ngx_int_t rc;

  rc = lua_gettop(L);
  if (rc != 1) {
    return luaL_error(L, "expecting 1 arguments, but only seen %d", rc);
  }
  ibuff.data = (u_char *)luaL_checklstring(L, 1, &ibuff.len);
  if (ibuff.len < 2) {
    return luaL_error(L, "luaL_checklstring error: buffsize=%l", ibuff.len);
  }

  rc = diversion_decode_runtimeinfo(&ibuff);

  if (rc == NGX_ERROR) {
    return luaL_error(L, "update_runtimeinfo error");
  }
  lua_pushnumber(L, rc);
  return 1;
}

static int get_runtimeinfo(lua_State *L) {

  diversion_runtime_info_t *runtimeinfo = global_runtimeinfo;

  lua_createtable(L, 0, 1);
  lua_pushnumber(L, runtimeinfo->status);
  lua_setfield(L, -2, "status");
  lua_pushnumber(L, runtimeinfo->version);
  lua_setfield(L, -2, "ver");
  lua_pushnumber(L, runtimeinfo->id);
  lua_setfield(L, -2, "fid");
  if (runtimeinfo->src.len > 0) {
    lua_pushlstring(L, (const char *)runtimeinfo->src.data,
                    runtimeinfo->src.len);
    lua_setfield(L, -2, "src");
  }
  if (runtimeinfo->to.len > 0) {
    lua_pushlstring(L, (const char *)runtimeinfo->to.data, runtimeinfo->to.len);
    lua_setfield(L, -2, "to");
  }

  // lua_rawset(L, -1);
  return 1;
}

static int get_envhosts(lua_State *L) {
  // double *num;
  diversion_runtime_info_t *runtimeinfo;
  diversion_env_host_t *envhost;
  ngx_uint_t i;
  char buff[50];

  i = lua_gettop(L);
  if (i != 0) {
    return luaL_error(L, "expecting 0 arguments, but only seen %d", i);
  }

  runtimeinfo = global_runtimeinfo;

  if (runtimeinfo->status <= 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_createtable(L, runtimeinfo->envhosts_len, 0);
  for (i = 0; i < runtimeinfo->envhosts_len; i++) {
    envhost = &runtimeinfo->envhosts[i];
    lua_pushlstring(L, (const char *)envhost->value.data, envhost->value.len);
    ngx_memzero(buff, 50);
    ngx_copy(buff, envhost->key.data, envhost->key.len);
    lua_setfield(L, -2, buff);
    // lua_pushlstring(L, (const char *)global_runtime_info->to.data,
    // global_runtime_info->to.len);
    // lua_pushnumber(L, global_runtime_info->current);
    // lua_rawset(L, -3);
  }

  return 1;
}

static int update_policy(lua_State *L) {
  ngx_str_t ibuff;
  ngx_int_t rc;
  uint32_t ver;

  dd("update_policy");
  rc = lua_gettop(L);
  if (rc != 2) {
    return luaL_error(L, "expecting 2 arguments, but only seen %d", rc);
  }
  ibuff.data = (u_char *)luaL_checklstring(L, 1, &ibuff.len);
  ver = luaL_checknumber(L, 2);

  rc = diversion_decode_policy(&ibuff, ver);

  if (rc != 0) {
    return luaL_error(L, "diversion_decode_policy error");
  }
  return 0;
}

static int get_policyid(lua_State *L) {
  uint32_t ver; // datalen;//,pidset[] = {0, 0, 0, 0};
  ngx_int_t rc, i, j, total, policy_grouplen;
  // uint64_t num;
  diversion_runtime_info_t *runtimeinfo;
  diversion_policy_group_t *pg, *policy_group;
  diversion_policy_t *policy;
  ngx_div_conf_enum_t *conf_ptype;
  // bit_vector_t *bit_vector;

  dd("get_policyid");
  rc = lua_gettop(L);
  if (rc != 1) {
    return luaL_error(L, "expecting 1 arguments, but only seen %d", rc);
  }

  ver = luaL_checknumber(L, 1);

  runtimeinfo = global_runtimeinfo;

  if (runtimeinfo->status != 1) {
    return luaL_error(L, "Runtimeinfo State invalid[%d]",runtimeinfo->status);
  }
  if (ver != runtimeinfo->version) {

    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                  "can't fetch global_runtime_info. "
                  "{ver: %d,version: %d,status: %d}",
                  ver, runtimeinfo->version, runtimeinfo->status);
    return luaL_error(L, "get_policyid is faliure.");
  }

  policy_grouplen = runtimeinfo->policy_grouplen;
  policy_group = runtimeinfo->policy_group;

  lua_createtable(L, 4, 0);
  total = 0;
  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    for (j = 0; j < pg->policyslen; j++) {
      policy = pg->policys[j];
      conf_ptype = &ngx_http_diversion_conf_ptypes[policy->type];
      switch (conf_ptype->stype) {
      case ST_RBTREE:
        lua_pushnumber(L, (uint64_t)policy->id);
        lua_rawseti(L, -2, ++total);
        break;
      default:
        break;
      }
    }
  }

  return 1;
}

static int update_numset(lua_State *L) {
  uint32_t pid, datalen,ver;
  ngx_int_t rc, i, j, k, policy_grouplen;
  uint64_t num;
  u_char *p;
  size_t plen, plenbak;
  diversion_runtime_info_t *runtimeinfo;
  diversion_policy_group_t *pg, *policy_group;
  diversion_policy_t *policy;
  ngx_rbtree_t *rbtree;
  ngx_rbtree_node_t *node;
  ngx_slab_pool_t *shpool;
  ngx_div_conf_enum_t *conf_ptype;

  rc = lua_gettop(L);
  if (rc != 3) {
    return luaL_error(L, "expecting 3 arguments, but only seen %d", rc);
  }
  p = (u_char *)luaL_checklstring(L, 1, &plen);
  if (plen <= 0 || p == NULL)
    luaL_error(L, "1 arguments is null", rc);
  pid = luaL_checknumber(L, 2);

  ver = luaL_checknumber(L, 3);

  dd("plen:%d, pid:%d, ver:%d",plen,pid,ver);

  plenbak = plen;

  runtimeinfo = global_runtimeinfo;

  if (runtimeinfo->version != ver) {
    return luaL_error(L, "runtimeinfo version invalid");
  }

  if (runtimeinfo->status != 1) {
    return luaL_error(L, "runtimeinfo State invalid");
  }

  shpool = runtimeinfo->pgshpool;
  policy_grouplen = runtimeinfo->policy_grouplen;
  policy_group = runtimeinfo->policy_group;

  datalen = utility_mqtt_read_uint32(p, plen);
  if (datalen == NGX_UINTX_ERROR)
    return luaL_error(L, "read numset failure.");
  p += 4;
  plen -= 4;
  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    for (j = 0; j < pg->policyslen; j++) {
      policy = pg->policys[j];
      conf_ptype = &ngx_http_diversion_conf_ptypes[policy->type];
      switch (conf_ptype->stype) {
      case ST_RBTREE:
      {
        if (policy->id != pid)
          break;

        if(policy->bitsetId==1){
          bit_vector_t *bit_vector = (bit_vector_t *)policy->data;
          if (!bit_vector) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,"diversion_bitset_shm not inited at policyid:%l",pid);
            return luaL_error(L, "diversion_bitset_shm not init.");
          }
          policy->datalen += datalen;
          for (k = 0; k < datalen; k++) {
            num = utility_mqtt_read_uint64(p, plen);
            if (num == NGX_UINTX_ERROR || num > UINT_MAX) {
              ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                            "read numset[%l] failure.{datalen:%d,bufsize:%l}",
                            num, datalen, plenbak);
              return luaL_error(L, "read numset failure.");
            }

            p += 8;
            plen -= 8;

            utility_bit_vector_set(bit_vector, num);
          }
          break;
        }
        rbtree = (ngx_rbtree_t *)policy->data;
        policy->datalen += datalen;
        for (k = 0; k < datalen; k++) {
          num = utility_mqtt_read_uint64(p, plen);
          if (num == NGX_UINTX_ERROR || num > UINT_MAX) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                          "read numset[%l] failure.{datalen:%d,bufsize:%l}",
                          num, datalen, plenbak);
            return luaL_error(L, "read numset failure.");
          }

          p += 8;
          plen -= 8;
          // if (num > UINT_MAX || num<1) return luaL_error(L, "val[%l] out
          // range[1..%l]",num,UINT_MAX);
          node=(ngx_rbtree_node_t*)ngx_slab_alloc(shpool,sizeof(ngx_rbtree_node_t));
          if(node==NULL){
            return luaL_error(L, "can't alloc ngx_rbtree_node_t");
          }
          node->key = num;
          ngx_rbtree_insert(rbtree,node);
        }
        return 0;
#if defined(DDEBUG) && (DDEBUG)
        //dd("bit_vector_count: %ld", utility_bit_vector_count(bit_vector));
#endif
        break;
      }
      default:
        break;
      }
    }
  }

  return 0;
}

static int update_bitvector(lua_State *L) {
  uint32_t pid, datalen,ver;
  ngx_int_t rc, i, j, k, policy_grouplen;
  uint64_t num;
  u_char *p;
  size_t plen, plenbak;
  diversion_runtime_info_t *runtimeinfo;
  diversion_policy_group_t *pg, *policy_group;
  diversion_policy_t *policy;
  bit_vector_t *bit_vector;
  ngx_div_conf_enum_t *conf_ptype;

  rc = lua_gettop(L);
  if (rc != 3) {
    return luaL_error(L, "expecting 3 arguments, but only seen %d", rc);
  }
  p = (u_char *)luaL_checklstring(L, 1, &plen);
  if (plen <= 0 || p == NULL)
    luaL_error(L, "1 arguments is null", rc);
  pid = luaL_checknumber(L, 2);

  ver = luaL_checknumber(L, 3);

  dd("plen:%ld, pid:%ld, ver:%d",plen,pid,ver);

  plenbak = plen;

  runtimeinfo = global_runtimeinfo;

  if (runtimeinfo->version != ver) {
    return luaL_error(L, "runtimeinfo version invalid");
  }

  if (runtimeinfo->status != 1) {
    return luaL_error(L, "runtimeinfo State invalid");
  }

  policy_grouplen = runtimeinfo->policy_grouplen;
  policy_group = runtimeinfo->policy_group;

  datalen = utility_mqtt_read_uint32(p, plen);
  if (datalen == NGX_UINTX_ERROR)
    return luaL_error(L, "read numset failure.");
  p += 4;
  plen -= 4;
  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    for (j = 0; j < pg->policyslen; j++) {
      policy = pg->policys[j];
      conf_ptype = &ngx_http_diversion_conf_ptypes[policy->type];
      switch (conf_ptype->stype) {
      case ST_BIT_SET:
      {
        if (policy->id != pid)
          break;
        bit_vector = (bit_vector_t *)policy->data;
        policy->datalen = datalen;
        for (k = 0; k < policy->datalen; k++) {
          num = utility_mqtt_read_uint64(p, plen);
          if (num == NGX_UINTX_ERROR || num > UINT_MAX) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                          "read numset[%l] failure.{datalen:%d,bufsize:%l}",
                          num, datalen, plenbak);
            return luaL_error(L, "read numset failure.");
          }

          p += 8;
          plen -= 8;
          // if (num > UINT_MAX || num<1) return luaL_error(L, "val[%l] out
          // range[1..%l]",num,UINT_MAX);
          utility_bit_vector_set(bit_vector, num);
        }
#if defined(DDEBUG) && (DDEBUG)
        //dd("bit_vector_count: %ld", utility_bit_vector_count(bit_vector));
#endif
        break;
      }
      default:
        break;
      }
    }
  }

  return 0;
}

static int active_policy(lua_State *L) {
  ngx_uint_t rc;
  uint32_t ver;
  diversion_runtime_info_t *runtimeinfo;

  dd("active_policy");
  rc = lua_gettop(L);
  if (rc != 1) {
    return luaL_error(L, "expecting 1 arguments, but only seen %d", rc);
  }
  ver = luaL_checknumber(L, 1);

  runtimeinfo = global_runtimeinfo;
  ngx_shmtx_lock(runtimeinfo->mutex);

  if (runtimeinfo->status != 1 || runtimeinfo->version != ver) {
    ngx_shmtx_unlock(runtimeinfo->mutex);
    ngx_log_error(
        NGX_LOG_WARN, ngx_cycle->log, 0,
        "runtimeinfo status actived failure. {ver: %d,version: %d,status: %d}",
        ver, runtimeinfo->version, runtimeinfo->status);
    return luaL_error(L, "Runtimeinfo State invalid");
  }

  runtimeinfo->status = 2;
  ngx_shmtx_unlock(runtimeinfo->mutex);
  NLOG_INFO("runtimeinfo status actived");
  return 0;
}

static int luaopen_nginx_diversion(lua_State *L) {
  lua_createtable(L, 0, 1);

  lua_pushcfunction(L, get_env);
  lua_setfield(L, -2, "get_env");

  if (diversion_sysconf_shm_zone != NULL) {
    lua_pushcfunction(L, update_runtimeinfo);
    lua_setfield(L, -2, "update_runtimeinfo");
    lua_pushcfunction(L, get_runtimeinfo);
    lua_setfield(L, -2, "get_runtimeinfo");
    lua_pushcfunction(L, get_envhosts);
    lua_setfield(L, -2, "get_envhosts");
    lua_pushcfunction(L, exec_policy);
    lua_setfield(L, -2, "exec_policy");
    lua_pushcfunction(L, update_policy);
    lua_setfield(L, -2, "update_policy");

    if (diversion_bitset_shm_zone != NULL) {
      // return luaL_error(L, "'diversion_numset_shm' must be configured");
      lua_pushcfunction(L, update_bitvector);
      lua_setfield(L, -2, "update_bitvector");      
    }
    lua_pushcfunction(L, update_numset);
    lua_setfield(L, -2, "update_numset");
    lua_pushcfunction(L, active_policy);
    lua_setfield(L, -2, "active_policy");
    lua_pushcfunction(L, get_policyid);
    lua_setfield(L, -2, "get_policyid");
  }

  return 1;
}

static ngx_int_t ngx_lua_diversion_init(ngx_conf_t *cf) {  
  if (!ngx_http_diversion_enabled)
    return NGX_OK;

  dd("preload ngxext.diversion");

  if (ngx_http_lua_add_package_preload(cf, "ngxext.diversion",
                                       luaopen_nginx_diversion) != NGX_OK) {
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                  "preload ngxext.diversion failued");
    return NGX_ERROR;
  }

  return NGX_OK;
}

/*
struct utility_compare_eqs_char
{
    bool operator()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) == 0;

    }
};
*/

struct utility_compare_eqs_ngx_str {
  bool operator()(const ngx_str_t *s1, const ngx_str_t *s2) const {
    // dd("%s, %s",s1->data, s2->data);
    size_t min = s1->len < s2->len ? s1->len : s2->len;
    ngx_int_t m = ngx_memcmp(s1->data, s2->data, min);
    return m == 0 ? (s1->len >= s2->len) : (m > 0);
  }
};

struct utility_compare_hash_ngx_str {
  size_t operator()(const ngx_str_t *stu) const {
    unsigned long res = 0;
    char *s = (char *)stu->data;
    ngx_uint_t i;
    for (i = 0; i < stu->len; i++) {
      res = 5 * res + s[i];
    }
    return size_t(res);
  }
};
static ngx_int_t free_diversion_envhosts() {
  ngx_slab_init(global_runtimeinfo->vhshpool);
  global_runtimeinfo->envhosts_len = 0;
  return NGX_OK;
}
static ngx_int_t free_diversion_policy() {
  ngx_slab_init(global_runtimeinfo->pgshpool);
  global_runtimeinfo->policy_grouplen = 0;
  if(global_bitvector_1)
  utility_bit_vector_clear(global_bitvector_1);
  return NGX_OK;
}

static ngx_int_t diversion_decode_runtimeinfo(ngx_str_t *ibuff) {
  ngx_slab_pool_t *vhshpool;
  diversion_runtime_info_t *runtimeinfo;
  diversion_env_host_t *envhosts, *vh;
  uint32_t version, id;
  uint16_t len, status, envhosts_len;
  ngx_int_t rc, i;
  u_char *p;
  size_t plen;
  ngx_str_t key, value, src, to;

  p = ibuff->data;
  plen = ibuff->len;

  runtimeinfo = global_runtimeinfo;

  ngx_shmtx_lock(runtimeinfo->mutex);

  status = utility_mqtt_read_uint16(p, plen);
  p += 2;
  plen -= 2;
  version = utility_mqtt_read_uint32(p, plen);
  p += 4;
  plen -= 4;

  dd("version:%d, status:%d, oversion:%d, ostatus:%d", version, status,
     runtimeinfo->version, runtimeinfo->status);
  if (runtimeinfo->version == version) {
    if(runtimeinfo->status==-1)runtimeinfo->status=status;
    goto diversion_decode_runtimeinfo_success;
  } else if (runtimeinfo->version > version && version > 0) {
    goto diversion_decode_runtimeinfo_failure;
  } else if (status != 0 && status != 1 && status != 3)
    goto diversion_decode_runtimeinfo_failure;

  if (runtimeinfo->version > 0 ) {
    runtimeinfo->status = 0;
    if(runtimeinfo->lock>0){
      ngx_shmtx_unlock(runtimeinfo->mutex);
      return NGX_DECLINED;
    }
      
    /*
    while (runtimeinfo->lock > 0) {
      usleep(10);
    }
   */

    free_diversion_policy();
    free_diversion_envhosts();
    /* TODO: humphery new version notify
    ngx_lua_diversion_update_file(global_diversion->runtimeinfo1.version,
                                  ngx_cycle->log);
    ngx_shmtx_unlock(global_diversion->mutex);
    return NGX_OK;
    */
  }

  if (runtimeinfo->lock > 0)
    goto diversion_decode_runtimeinfo_failure;

  vhshpool = runtimeinfo->vhshpool;

  /*
      if (!ngx_atomic_cmp_set(&global_runtime_info->lock,0, 1)){
              goto failure;
      }*/

  rc = utility_mqtt_read_string(p, plen, &src);
  if (rc == NGX_ERROR)
    goto diversion_decode_runtimeinfo_failure;
  p += rc;
  plen -= rc;
  rc = utility_mqtt_read_string(p, plen, &to);
  if (rc == NGX_ERROR)
    goto diversion_decode_runtimeinfo_failure;
  p += rc;
  plen -= rc;
  id = utility_mqtt_read_uint32(p, plen);
  p += 4;
  plen -= 4;

  len = utility_mqtt_read_uint16(p, plen);
  p += 2;
  plen -= 2;

  dd("srclen:%d, tolen:%d, pid:%d, vhlen:%d", src.len, to.len, id,
     envhosts_len);

  envhosts_len = 0;
  envhosts = NULL;
  if (len > 0) {
    envhosts = (diversion_env_host_t *)ngx_slab_alloc(
        vhshpool, len * sizeof(diversion_env_host_t));
    if (envhosts == NULL) {
      ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                    "can't alloc 'diversion_env_host_t' memory: %d",
                    len * sizeof(diversion_env_host_t));
      goto diversion_decode_runtimeinfo_failure;
    }
    envhosts_len = len;
    for (i = 0; i < len; i++) {
      vh = &envhosts[i];
      rc = utility_mqtt_read_string(p, plen, &key);
      if (rc == NGX_ERROR)
        goto diversion_decode_runtimeinfo_failure;
      p += rc;
      plen -= rc;
      rc = utility_mqtt_read_string(p, plen, &value);
      if (rc == NGX_ERROR)
        goto diversion_decode_runtimeinfo_failure;
      p += rc;
      plen -= rc;
      vh->key.data = (u_char *)ngx_slab_alloc(vhshpool, key.len + value.len);
      if (vh->key.data == NULL) {
        vh->key.len = 0;
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                      "can't alloc 'u_char*' memory: %d", key.len + value.len);
        goto diversion_decode_runtimeinfo_failure;
      }
      vh->key.len = key.len;
      vh->value.data = vh->key.data + key.len;
      vh->value.len = value.len;

      ngx_copy(vh->key.data, key.data, key.len);
      ngx_copy(vh->value.data, value.data, value.len);
    }
  }
  runtimeinfo->id = id;
  if (src.len > 0)
    runtimeinfo->src.data = ngx_shmpool_strdup(vhshpool, &src);
  runtimeinfo->src.len = src.len;
  if (to.len > 0)
    runtimeinfo->to.data = ngx_shmpool_strdup(vhshpool, &to);
  runtimeinfo->to.len = to.len;
  runtimeinfo->envhosts_len = envhosts_len;
  runtimeinfo->envhosts = envhosts;
  runtimeinfo->status = status;

diversion_decode_runtimeinfo_success:
  runtimeinfo->version = version;
  ngx_shmtx_unlock(runtimeinfo->mutex);
  return NGX_OK;
diversion_decode_runtimeinfo_failure:
  free_diversion_envhosts();
  ngx_shmtx_unlock(runtimeinfo->mutex);
  return NGX_ERROR;
}

static ngx_int_t print_policy(diversion_policy_group_t *policy_group,
                 uint16_t policy_grouplen) {
#if defined(DDEBUG) && (DDEBUG)
  ngx_int_t i, j, plen;
  diversion_policy_group_t *pg;
  diversion_policy_t *policy;
  diversion_policy_range_t *range;
  uint32_t k, num32set;
  u_char *p;

  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    if (pg->uri.len == 0)
      continue;
    dd("***************: policy_group-%d/%p, uri: %d, action: %d, backend: %d, "
       "policy-len: %d",
       i, pg, pg->uri.len, pg->action, pg->backend.len, pg->policyslen);

    for (j = 0; j < pg->policyslen; j++) {
      policy = pg->policys[j];
      dd("***************: policy-%d/%p, type: %d, datalen: %d, id: %d", j,
         policy, policy->type, policy->datalen, policy->id);
      conf_ptype = &ngx_http_diversion_conf_ptypes[policy->type];
      switch (conf_ptype->stype) {
      case ST_RANGE:
        for (k = 0; k < policy->datalen; k++) {
          range = &((diversion_policy_range_t *)policy->data)[k];
          dd("***************: range %ld,%ld", range->start, range->end);
        }

        break;

      case ST_BIT_SET:{
        dd("***************: numset %ld",
           utility_bit_vector_count((bit_vector_t *)policy->data));
        break;
      }
      case ST_NUM_SET:
      case ST_RBTREE:
      case ST_SUFFIX:{
        p = (u_char *)policy->data;
        plen = policy->datalen * 4;
        for (k = 0; k < policy->datalen; k++) {
          num32set = utility_mqtt_read_uint32(p, plen);
          // if(num32set==NGX_UINTX_ERROR)return NGX_ERROR;
          p += 4;
          plen -= 4;
          dd("***************: v: %ld",num32set);
        }

        break;
      }
      case ST_RESERVED:
        break;
      default:
        dd("*************** no support policy type: %d", policy->type);
        return NGX_ERROR;
      }
    }
  }
#endif
  return NGX_OK;
}

static ngx_int_t diversion_decode_policy(ngx_str_t *ibuff, uint32_t ver) {
  uint16_t len, policy_grouplen,policy_len;
  ngx_int_t rc, i, j;
  u_char *p;
  size_t plen;
  ngx_str_t type;
  diversion_runtime_info_t *runtimeinfo;
  diversion_policy_group_t *pg, *policy_group;
  diversion_policy_t *policy,*policys,*tmppolicy;
  diversion_policy_range_t *range;
  uint32_t k,num32;
  ngx_slab_pool_t *shpool;
  ngx_div_conf_enum_t *conf_ptype;

  p = ibuff->data;
  plen = ibuff->len;

  runtimeinfo = global_runtimeinfo;
  shpool = runtimeinfo->pgshpool;

  if (ver != runtimeinfo->version || runtimeinfo->policy_grouplen > 0)
    return NGX_OK;

  len = utility_mqtt_read_uint16(p, plen);
  if (len == NGX_UINTX_ERROR)
    return NGX_ERROR;
  p += 2;
  plen -= 2;

  ngx_shmtx_lock(global_runtimeinfo->mutex);

  policys = (diversion_policy_t *)ngx_slab_alloc(shpool, len * DIVERSION_POLICY_T_LEN);
  if (policys == NULL) {
    NLOG_ERROR("can't alloc 'diversion_policy_t' memory: %d",
                  len * DIVERSION_POLICY_T_LEN);
    goto diversion_decode_policy_failure;
  }

  ngx_memzero(policys, len * DIVERSION_POLICY_T_LEN);

  policy_len = len;

  NLOG_DEBUG("policy len: %d", policy_len);
  for (i = 0; i < policy_len; i++) {
    policy = &policys[i];
    policy->id = utility_mqtt_read_uint32(p, plen);
    if (policy->id == NGX_UINTX_ERROR)
      goto diversion_decode_policy_failure;
    p += 4;
    plen -= 4;

    NLOG_DEBUG("id %ld", policy->id);
    rc = utility_mqtt_read_string(p, plen, &type);
    if (rc == NGX_ERROR)
      goto diversion_decode_policy_failure;
    p += rc;
    plen -= rc;
    policy->bitsetId = utility_mqtt_read_uint16(p, plen);
    p += 2;
    plen -= 2;
    policy->datalen = utility_mqtt_read_uint16(p, plen);
    // if(policy->datalen==NGX_UINTX_ERROR)goto failure;
    p += 2;
    plen -= 2;
    conf_ptype=NULL;
    for(int x = ngx_http_diversion_conf_ptypes_len-1;x>=0;x--){
      if (memcmp(type.data, ngx_http_diversion_conf_ptypes[x].name.data, type.len) == 0){
        conf_ptype = &ngx_http_diversion_conf_ptypes[x];
        policy->type = ngx_http_diversion_conf_ptypes[x].value;
        break;
      }
    }

    if(conf_ptype==NULL){
      NLOG_ERROR("no found conf_ptype: type: %s,pid: %d",type.data,policy->id);
      goto diversion_decode_policy_failure;
    }
    NLOG_DEBUG("policy-%d/%p, type: %s, datalen: %d", policy->id, policy, conf_ptype->name.data, policy->datalen);
    switch (conf_ptype->stype) {
    case ST_RANGE:
    //case DIVS_PTYPE_CID_RANGE:
    //case DIVS_PTYPE_IP_RANGE:
      if (policy->datalen == 0) {
        policy->type = DIVS_PTYPE_RESERVED;
        NLOG_ERROR("RANGE policy->datalen is 0.", policy->datalen);
        goto diversion_decode_policy_failure;
      }
      range = (diversion_policy_range_t *)ngx_slab_alloc(
          shpool, policy->datalen * sizeof(diversion_policy_range_t));
      if (range == NULL) {
        policy->type = DIVS_PTYPE_RESERVED;
        NLOG_ERROR("can't alloc 'diversion_policy_range_t' memory: %d",
                      policy->datalen * sizeof(diversion_policy_range_t));
        goto diversion_decode_policy_failure;
      }
      ngx_memzero(range, policy->datalen * sizeof(diversion_policy_range_t));
      policy->data = (void *)range;

      for (k = 0; k < policy->datalen; k++) {
        range = &((diversion_policy_range_t *)policy->data)[k];
        range->start = utility_mqtt_read_uint64(p, plen);
        if (range->start == NGX_UINTX_ERROR)
          goto diversion_decode_policy_failure;
        p += 8;
        plen -= 8;
        range->end = utility_mqtt_read_uint64(p, plen);
        if (range->end == NGX_UINTX_ERROR)
          goto diversion_decode_policy_failure;
        p += 8;
        plen -= 8;
        NLOG_DEBUG("range: %ld - %ld", range->start, range->end);
      }

      break;
    case ST_BIT_SET:
    //case DIVS_PTYPE_BIT_SET1:      
      policy->data = (void *)global_bitvector_1;
      break;
    case ST_RBTREE:
    //case DIVS_PTYPE_AID_SET:
    //case DIVS_PTYPE_CID_SET
    if(policy->bitsetId==1){
      policy->data = (void *)global_bitvector_1;      
      break;
    } else {
        ngx_rbtree_t *rbtree;
        ngx_rbtree_node_t *sentinel;
        rbtree = (ngx_rbtree_t*)ngx_slab_alloc(shpool, sizeof(ngx_rbtree_t));
        if (rbtree == NULL) {
          NLOG_ERROR("can't alloc 'ngx_rbtree_t' memory: %d",sizeof(ngx_rbtree_t));
          policy->type = DIVS_PTYPE_RESERVED;
          goto diversion_decode_policy_failure;
        }
        sentinel = (ngx_rbtree_node_t*)ngx_slab_alloc(shpool, sizeof(ngx_rbtree_node_t));
        if (sentinel == NULL) {
          NLOG_ERROR("can't alloc 'ngx_rbtree_node_t' memory: %d",sizeof(ngx_rbtree_node_t));
          policy->type = DIVS_PTYPE_RESERVED;
          goto diversion_decode_policy_failure;
        }
        ngx_rbtree_init(rbtree,sentinel,ngx_rbtree_insert_value);
        policy->data = rbtree;
        NLOG_DEBUG("datalen: %ld, plen: %ld", policy->datalen, plen);
      } 
      break;  
    case ST_NUM_SET:
      if(policy->bitsetId==1){
        policy->data = (void *)global_bitvector_1;
        for (k = 0; k < policy->datalen; k++) {
          utility_bit_vector_set(global_bitvector_1, utility_mqtt_read_uint32(p, plen));
          p += 4;
          plen -= 4;
        }
        break;
      }
    case ST_SUFFIX:
    {
      if (policy->datalen == 0) {
        policy->type = DIVS_PTYPE_RESERVED;
        NLOG_ERROR("policy[%s]->datalen is 0.", conf_ptype->name.data,policy->datalen);
        goto diversion_decode_policy_failure;
      }
      policy->data =
          ngx_slab_alloc(shpool, policy->datalen * sizeof(uint32_t));
      if (policy->data == NULL) {
        NLOG_ERROR("can't alloc 'uint32_t' memory: %d",
                      policy->datalen * sizeof(uint32_t));
        policy->type = DIVS_PTYPE_RESERVED;
        goto diversion_decode_policy_failure;
      }
      NLOG_DEBUG("datalen: %ld, plen: %ld", policy->datalen, plen);

      ngx_copy((u_char *)policy->data, p, policy->datalen * sizeof(uint32_t));
      p += policy->datalen * sizeof(uint32_t);
      plen -= policy->datalen * sizeof(uint32_t);

      NLOG_DEBUG("plen: %ld", plen);
    } break;
    default:
      NLOG_ERROR("no support policy type: %d", policy->type);
      goto diversion_decode_policy_failure;
    }
  }

  len = utility_mqtt_read_uint16(p, plen);
  if (len == NGX_UINTX_ERROR)
    goto diversion_decode_policy_failure;
  p += 2;
  plen -= 2;

  policy_group = (diversion_policy_group_t *)ngx_slab_alloc(
      shpool, len * DIVERSION_POLICY_GROUP_T_LEN);
  if (policy_group == NULL) {
    NLOG_ERROR("can't alloc 'diversion_policy_group_t' memory: %d",
                  len * DIVERSION_POLICY_GROUP_T_LEN);
    goto diversion_decode_policy_failure;
  }
  NLOG_DEBUG("%p,%p", policy_group, policy_group + 2 * DIVERSION_POLICY_GROUP_T_LEN);
  ngx_memzero(policy_group, len * DIVERSION_POLICY_GROUP_T_LEN);

  policy_grouplen = len;

  NLOG_DEBUG("policy_group len: %d", policy_grouplen);
  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    if (pg == NULL)
      goto diversion_decode_policy_failure;
    rc = utility_mqtt_read_string(p, plen, &pg->uri);
    if (rc == NGX_ERROR)
      goto diversion_decode_policy_failure;
    p += rc;
    plen -= rc;
    pg->uri.data = ngx_shmpool_strdup(shpool, &pg->uri);
    pg->action = utility_mqtt_read_uint16(p, plen);
    if (pg->action == NGX_UINTX_ERROR)
      goto diversion_decode_policy_failure;
    p += 2;
    plen -= 2;
    rc = utility_mqtt_read_string(p, plen, &pg->backend);
    if (rc == NGX_ERROR)
      goto diversion_decode_policy_failure;
    p += rc;
    plen -= rc;
    pg->backend.data = ngx_shmpool_strdup(shpool, &pg->backend);
    rc = utility_mqtt_read_string(p, plen, &pg->text);
    if (rc == NGX_ERROR)
      goto diversion_decode_policy_failure;
    p += rc;
    plen -= rc;
    pg->text.data = ngx_shmpool_strdup(shpool, &pg->text);
    pg->policyslen = utility_mqtt_read_uint16(p, plen);
    if (pg->policyslen == NGX_UINTX_ERROR)
      goto diversion_decode_policy_failure;
    p += 2;
    plen -= 2;
    
    NLOG_DEBUG("policy_group-%d/%p, uri: %s[%d], action: %d, backend: "
       "%s, "
       "policy-len: %d, %p",
       i, pg, pg->uri.data, pg->uri.len, pg->action, pg->backend.data,
       pg->policyslen, pg->uri.data);

    pg->policys = (diversion_policy_t **)ngx_slab_alloc(shpool, pg->policyslen * sizeof(diversion_policy_t*));
    if (pg->policys == NULL) {
      NLOG_ERROR("can't alloc 'diversion_policy_t*' memory: %d",
                    pg->policyslen * sizeof(diversion_policy_t*));
      pg->policyslen = 0;
      goto diversion_decode_policy_failure;
    }
    //ngx_memzero((*pg->policys), pg->policyslen * sizeof(diversion_policy_t*));

    for (j = 0; j < pg->policyslen; j++) {
      num32 = utility_mqtt_read_uint32(p, plen);
      p += 4;
      plen -= 4;
      tmppolicy=NULL;
      for (k = 0; k < policy_len; k++) {
        policy = &policys[k];
        if(policy->id==num32){
          pg->policys[j]=policy;
          tmppolicy=policy;
          break;
        }
      }
      if(tmppolicy==NULL){
        NLOG_ERROR("can't find 'diversion_policy_t' pid: %d",num32);
        pg->policyslen = 0;
        goto diversion_decode_policy_failure;
      }
    }
  }
  if (print_policy(policy_group, policy_grouplen) != NGX_OK)
    goto diversion_decode_policy_failure;

  runtimeinfo->policy_grouplen = policy_grouplen;
  runtimeinfo->policy_group = policy_group;
  runtimeinfo->version = ver;

  ngx_shmtx_unlock(runtimeinfo->mutex);
  return NGX_OK;
diversion_decode_policy_failure:
  free_diversion_policy();
  ngx_shmtx_unlock(runtimeinfo->mutex);
  return NGX_ERROR;
}

static ngx_inline bool endWith(const char *str, ngx_int_t l1, const char *end, ngx_int_t l2) {
  bool result = false;
  if (str != NULL && end != NULL) {
    if (l1 >= l2) {
      if (ngx_memcmp(str + l1 - l2, end, l2) == 0) {
        result = true;
      }
    }
  }
  return result;
}

static ngx_inline ngx_int_t diversion_proc_result(uint16_t action, ngx_str_t *to,
                                 ngx_str_t *text, diversion_cacl_res_t *out) {
  diversion_env_host_t *envhosts;
  uint16_t k;
  out->backend.data = to->data;
  out->backend.len = to->len;
  out->text.data = text->data;
  out->text.len = text->len;
  out->action = action;
  if (out->action == 2) {
    if (div_env.len == out->backend.len &&
        ngx_memcmp(div_env.data, out->backend.data, div_env.len) == 0)
      out->action = 1;
    else {

      for (k = 0; k < global_runtimeinfo->envhosts_len; k++) {
        envhosts = &global_runtimeinfo->envhosts[k];
        if (envhosts->key.len == out->backend.len &&
            ngx_memcmp(envhosts->key.data, out->backend.data,
                       envhosts->key.len) == 0) {
          out->backend.data = envhosts->value.data;
          out->backend.len = envhosts->value.len;
          break;
        }
      }
    }
  }
  return NGX_OK;
}

static ngx_inline bool find_num32(u_char *p, size_t len,uint32_t n) {
  int low,high,midle;
  uint32_t num32;

  high = len/4 - 1;
  low = 0;

  while(high >= low){
    midle=(high+low)/2;
    num32 = utility_mqtt_read_uint32(p+(midle*4), 4);
    //ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "len:%d, num32:%ld, n:%ld, low:%d, high:%d", len,num32,n,low,high);
    if (n == num32) {
      return true;
    }
    if (n < num32) {
      high = midle-1;
    } else {
      low = midle +1;
    }
   // ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "len:%d, num32:%ld, n:%ld, low:%d, high:%d", len,num32,n,low,high);
  }

  return false;
}

static ngx_inline diversion_policy_range_t *find_range(void *p, size_t len,uint64_t n) {
  int                    low,high,midle;
  //uint64_t                  num64;
  diversion_policy_range_t  *range;  
  high = len - 1;
  low = 0;

  while(high >= low){
    midle=(high+low)/2;
    range = &((diversion_policy_range_t *)p)[midle];
    //num64 = utility_mqtt_read_uint32(p+(midle*4), 4);
    //ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "len:%d, num32:%ld, n:%ld, low:%d, high:%d", len,num32,n,low,high);
    if (n >= range->start && n <= range->end) {
      return range;
    }
    if (n < range->start) {
      //if(midle<=0)return NULL;
      high = midle-1;
    } else {
      low = midle +1;
    }
   // ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "len:%d, num32:%ld, n:%ld, low:%d, high:%d", len,num32,n,low,high);
  }

  return NULL;
}
// if (ngx_atomic_cmp_set(&ring_buf->wcursor,prev, sequence))
static ngx_int_t diversion_exec_policy(ngx_div_user_info_t *userInfo,
                                  diversion_cacl_res_t *out) {
  ngx_int_t i, j;
  bool pass;
  uint16_t policy_grouplen;
  diversion_runtime_info_t *runtimeinfo;
  diversion_policy_group_t *pg, *policy_group;
  diversion_policy_t *policy;
  diversion_policy_range_t *range;
  ngx_div_conf_enum_t      *conf_ptype;
  // diversion_env_host_t *envhosts;
  bit_vector_t *bit_vector;
  uint32_t num32set, k, x;
  //set<const ngx_str_t *, utility_compare_eqs_ngx_str> *strset;
  //set<const ngx_str_t *, utility_compare_eqs_ngx_str>::iterator it;

  //char itoabuff[33];

  u_char *p;
  size_t plen;
  ngx_atomic_t *lock;

  out->action = 0;
  out->backend.len = 0;
  out->text.len = 0;

  runtimeinfo = global_runtimeinfo;

  lock = &(runtimeinfo->lock);
  
  if (runtimeinfo->status < 2)
    return NGX_ERROR;
  
  ngx_atomic_fetch_add(lock, 1);

  policy_grouplen = runtimeinfo->policy_grouplen;
  policy_group = runtimeinfo->policy_group;

  
  for (i = 0; i < policy_grouplen; i++) {
    pg = &policy_group[i];
    pass = false;
    // dd("url %d: %s", pg->uri.len, pg->uri.data);
    if (pg->uri.len <= userInfo->uri.len && ngx_memcmp(userInfo->uri.data, pg->uri.data, pg->uri.len) == 0) {
      NLOG_INFO("passed by uri: %s", pg->uri.data);
    }else{
      NLOG_DEBUG("url: %s",pg->uri.data);
      continue;
    }

    if (runtimeinfo->status == 3) {
      diversion_proc_result(pg->action, &pg->backend, &pg->text, out);
      ngx_atomic_fetch_add(lock, -1);
      return NGX_OK;
    }
        
    for (j = 0; j < pg->policyslen; j++) {
      policy = pg->policys[j];
      pass = false;
      conf_ptype = &ngx_http_diversion_conf_ptypes[policy->type];
      switch (conf_ptype->stype) {
      case ST_RANGE://DIVS_PTYPE_AID_RANGE:
        if (userInfo->req_val[conf_ptype->cvalue] <= 0) {
          pass = false;
          break;
        }
        if(policy->datalen<=0){
          NLOG_WARN("The policy(%s/%d) can't null,",conf_ptype->name.data,policy->id);
          pass = false;
          break;
        }
        /*
        for (k = 0; k < policy->datalen; k++) {
          range = &((diversion_policy_range_t *)policy->data)[k];
          if (userInfo->req_val[conf_ptype->cvalue] >= range->start && userInfo->req_val[conf_ptype->cvalue] <= range->end) {
            NLOG_INFO("passed by %s(%d/%d): %l <= %s <= %l",conf_ptype->name.data, j + 1,
                          pg->policyslen, range->start,conf_ptype->name.data, range->end);
            pass = true;
            break;
          }
        }*/
        range = find_range(policy->data,policy->datalen,userInfo->req_val[conf_ptype->cvalue]);
        if(range){
          NLOG_INFO("passed by %s(%d/%d): %l <= %s <= %l",conf_ptype->name.data, j + 1,
                          pg->policyslen, range->start,conf_ptype->name.data, range->end);
            pass = true;
        }
        
        break;
      case ST_BIT_SET://DIVS_PTYPE_BIT_SET1:
        if (userInfo->req_val[conf_ptype->cvalue] <= 0) {
          pass = false;
          break;
        }
        bit_vector = (bit_vector_t *)policy->data;
        if (!bit_vector) {
          continue;
        }
        if (utility_bit_vector_get(bit_vector, userInfo->req_val[conf_ptype->cvalue])) {
          NLOG_INFO("passed by %s(%d/%d): %l",conf_ptype->name.data, j + 1,
              pg->policyslen, userInfo->req_val[conf_ptype->cvalue]);
          pass = true;
        }
        break;
      case ST_RBTREE://DIVS_PTYPE_AID_SET:
        if (userInfo->req_val[conf_ptype->cvalue] <= 0) {
          pass = false;
          break;
        }
        if(policy->bitsetId==1){
          bit_vector = (bit_vector_t *)policy->data;
          if (!bit_vector) {
            continue;
          }
          if (utility_bit_vector_get(bit_vector, userInfo->req_val[conf_ptype->cvalue])) {
            NLOG_INFO("passed by %s(%d/%d): %l",conf_ptype->name.data, j + 1,pg->policyslen, userInfo->req_val[conf_ptype->cvalue]);
            pass = true;
          }
          break;
        } else {
          ngx_rbtree_t *rbtree;
          rbtree = (ngx_rbtree_t*)policy->data;
          pass = (ngx_rbtree_lookup_4diversion(rbtree , userInfo->req_val[conf_ptype->cvalue])!=NULL);
          if(pass){
            NLOG_INFO("passed by %s(%d/%d): %l",conf_ptype->name.data, j + 1,
              pg->policyslen,userInfo->req_val[conf_ptype->cvalue]);
          }
        }        
        break;
      case ST_NUM_SET://DIVS_PTYPE_OID_SET: DIVS_PTYPE_IP_SET
        if (userInfo->req_val[conf_ptype->cvalue] <= 0) {
          pass = false;
          break;
        }
        if(policy->bitsetId==1){
          bit_vector = (bit_vector_t *)policy->data;
          if (!bit_vector) {
            continue;
          }
          if (utility_bit_vector_get(bit_vector, userInfo->req_val[conf_ptype->cvalue])) {
            NLOG_INFO("passed by %s(%d/%d): %l",conf_ptype->name.data, j + 1,pg->policyslen, userInfo->req_val[conf_ptype->cvalue]);
            pass = true;
          }
          break;
        }
        p = (u_char *)policy->data;
        plen = policy->datalen * 4;
        if(policy->datalen<=0){
          NLOG_WARN("The policy(%s/%d) can't null,",conf_ptype->name.data,policy->id);
          pass = false;
          break;
        }
        pass = find_num32(p,plen,userInfo->req_val[conf_ptype->cvalue]);
        if(pass){
          NLOG_INFO("passed by %s(%d/%d): %l",conf_ptype->name.data, j + 1,
              pg->policyslen, userInfo->req_val[conf_ptype->cvalue]);
        }
        break;
      case ST_SUFFIX:{
        if (userInfo->req_val[conf_ptype->cvalue] <= 0) {
          pass = false;
          break;
        }
        if(policy->datalen<=0){
          NLOG_WARN("The policy(%s/%d) can't null,",conf_ptype->name.data,policy->id);
          pass = false;
          break;
        }
        p = (u_char *)policy->data;
        plen = policy->datalen * 4;
        for (k = 0; k < policy->datalen; k++) {
          num32set = utility_mqtt_read_uint32(p, plen);
          // if(num32set==NGX_UINTX_ERROR)return NGX_ERROR;
          p += 4;
          plen -= 4;
          if (userInfo->req_val[conf_ptype->cvalue] >= (num32set)) {
            x = 1;
            do {
              x *= 10;
            } while ((num32set) >= x);
            if (((uint64_t)userInfo->req_val[conf_ptype->cvalue]/x)*x == (userInfo->req_val[conf_ptype->cvalue]-num32set) ) {
              NLOG_INFO("passed by %s(%d/%d): %l.endWith(%d)",conf_ptype->name.data, j + 1,
                            pg->policyslen, userInfo->req_val[conf_ptype->cvalue], (num32set));
              pass = true;
              break;
            }
          }
        }
      } 
      break; 

      default:
        NLOG_WARN("no support ptype: %d, userinfo: %ld",conf_ptype->stype,userInfo->req_val[conf_ptype->cvalue]);
        ngx_atomic_fetch_add(lock, -1);
        return NGX_ERROR;
      }
      if (!pass) {
        break;
      }
    }
    if (pass) {
      diversion_proc_result(pg->action, &pg->backend, &pg->text, out);
      ngx_atomic_fetch_add(lock, -1);
      return NGX_OK;
    }
  }  
  ngx_atomic_fetch_add(lock, -1);
  return NGX_ERROR;
}

static ngx_inline  ngx_rbtree_node_t*  
ngx_rbtree_lookup_4diversion(ngx_rbtree_t *rbtree, uint64_t key)  
{  
    ngx_rbtree_key_t             node_key;  
    ngx_rbtree_node_t           *node;  
    ngx_rbtree_node_t           *sentinel;  
    //TestRBTreeNode           *fcn;  
  
    node_key = key;  
  
    node = rbtree->root;      //这里记录下要查找的树的根节点  
    sentinel = rbtree->sentinel;  //记录下哨兵节点  
  
    while (node != sentinel) {  
  
        if (node_key < node->key) {       //通过左右子树查找key一样的节点  
            node = node->left;  
            continue;  
        }  
  
        if (node_key > node->key) {  
            node = node->right;  
            continue;  
        }  
  
        /* node_key == node->key */  
  
        //fcn = (TestRBTreeNode *) node;    //找到节点就转成节点数据结构，返回  
  
        return node;  
    }  
  
    /* not found */  
  
    return NULL;  
}  

static ngx_inline void ngx_rbtree_clear_4diversion(ngx_rbtree_t *rbtree)  
{  
    ngx_rbtree_node_t           *node,*sentinel,*p;
    //TestRBTreeNode                  *p;  
    ngx_slab_pool_t *shpool;
  
    shpool = global_runtimeinfo->pgshpool;
    sentinel = rbtree->sentinel;  //记录下哨兵节点  
    node = rbtree->root;

    do{
        if(node->left!=sentinel)node=node->left;
        else if(node->right!=sentinel)node=node->right;
        else{
            p = node;
            //printf("free %ld\n",p->mydate);
            node=node->parent;
            if(p->key < node->key){
                node->left=sentinel;
            }else{
                node->right=sentinel;
            }
            ngx_slab_free(shpool,p);
        }
    }while (rbtree->root->left != sentinel || rbtree->root->right!=sentinel) ;
    if(rbtree->root != sentinel){
        p = node;
        rbtree->root=sentinel;
        ngx_slab_free(shpool,p);
    }
}  
#ifdef __cplusplus
}
#endif
//boost::interprocess::managed_shared_memory vector
