
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>
#include "ngx_socks_protocol.h"

typedef struct {
    ngx_str_t                      uname;
    ngx_str_t                      passwd;
} ngx_stream_socks5_auth_t;

typedef struct {
    ngx_addr_t                      *addr;
    ngx_stream_complex_value_t      *value;
#if (NGX_HAVE_TRANSPARENT_socks5)
    ngx_uint_t                       transparent; /* unsigned  transparent:1; */
#endif
} ngx_stream_upstream_local_t;

typedef struct {
    ngx_flag_t                       enabled;
    ngx_msec_t                       connect_timeout;
    ngx_msec_t                       timeout;
    ngx_msec_t                       next_upstream_timeout;
    size_t                           buffer_size;
    size_t                           upload_rate;
    size_t                           download_rate;
    ngx_uint_t                       responses;
    ngx_uint_t                       next_upstream_tries;
    ngx_flag_t                       next_upstream;
    ngx_flag_t                       socks5_protocol;
    ngx_stream_upstream_local_t     *local;

    ngx_stream_upstream_srv_conf_t  *upstream;
    ngx_array_t                      *passwd_list;
} ngx_stream_socks5_srv_conf_t;

typedef struct {
    ngx_pool_t              *pool;
    ngx_buf_t		        *in;		/* request from client */
	ngx_buf_t			    *out;		/* response from remote */
    struct sockaddr_in      r_addr;
	ngx_int_t               r_addr_len;
    ngx_flag_t              authed;    
}ngx_stream_socks5_ctx_t;

static void ngx_socks5_send(ngx_event_t *wev);
static void ngx_stream_socks5_handler(ngx_stream_session_t *s);
static void ngx_stream_socks5_process_auth_handler(ngx_event_t *ev);
static ngx_int_t ngx_stream_socks5_eval(ngx_stream_session_t *s,   ngx_stream_socks5_srv_conf_t *pscf);
static ngx_int_t ngx_stream_socks5_set_local(ngx_stream_session_t *s,  ngx_stream_upstream_t *u, ngx_stream_upstream_local_t *local);
static void ngx_stream_socks5_connect(ngx_stream_session_t *s);
static void ngx_stream_socks5_init_upstream(ngx_stream_session_t *s);
static void ngx_stream_socks5_resolve_handler(ngx_resolver_ctx_t *ctx);
static void ngx_stream_socks5_upstream_handler(ngx_event_t *ev);
static void ngx_stream_socks5_downstream_handler(ngx_event_t *ev);
static void ngx_stream_socks5_process_connection(ngx_event_t *ev,   ngx_uint_t from_upstream);
static void ngx_stream_socks5_connect_handler(ngx_event_t *ev);
static ngx_int_t ngx_stream_socks5_test_connect(ngx_connection_t *c);
static void ngx_stream_socks5_process(ngx_stream_session_t *s,   ngx_uint_t from_upstream, ngx_uint_t do_write);
static void ngx_stream_socks5_next_upstream(ngx_stream_session_t *s);
static void ngx_stream_socks5_finalize(ngx_stream_session_t *s, ngx_uint_t rc);
static u_char *ngx_stream_socks5_log_error(ngx_log_t *log, u_char *buf,   size_t len);

static void *ngx_stream_socks5_create_srv_conf(ngx_conf_t *cf);
static char *ngx_stream_socks5_merge_srv_conf(ngx_conf_t *cf, void *parent,  void *child);
static char *ngx_stream_socks5_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_conf_set_str_pwd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
u_char *ngx_socks5_protocol_write(ngx_connection_t *c, u_char *buf, u_char *last);
static ngx_int_t ngx_stream_socks5_get_peer(ngx_peer_connection_t *pc, void *data);
static void ngx_stream_socks5_free_peer(ngx_peer_connection_t *pc, void *data,ngx_uint_t state);
static ngx_int_t ngx_stream_socks5_init_peer(ngx_stream_session_t *s, ngx_stream_upstream_srv_conf_t *us);

static ngx_conf_deprecated_t  ngx_conf_deprecated_socks5_downstream_buffer = {
    ngx_conf_deprecated, "socks5_downstream_buffer", "socks5_buffer_size"
};

static ngx_conf_deprecated_t  ngx_conf_deprecated_socks5_upstream_buffer = {
    ngx_conf_deprecated, "socks5_upstream_buffer", "socks5_buffer_size"
};


static ngx_command_t  ngx_stream_socks5_commands[] = {

    { ngx_string("socks5_pass"),
      NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_stream_socks5_pass,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("socks5_connect_timeout"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_socks5_srv_conf_t, connect_timeout),
      NULL },

    { ngx_string("socks5_timeout"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_socks5_srv_conf_t, timeout),
      NULL },

    { ngx_string("socks5_buffer_size"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_socks5_srv_conf_t, buffer_size),
      NULL },

    { ngx_string("socks5_downstream_buffer"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_socks5_srv_conf_t, buffer_size),
      &ngx_conf_deprecated_socks5_downstream_buffer },

    { ngx_string("socks5_upstream_buffer"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_socks5_srv_conf_t, buffer_size),
      &ngx_conf_deprecated_socks5_upstream_buffer },

    { ngx_string("password_path"),
      NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_pwd,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t  ngx_stream_socks5_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_stream_socks5_create_srv_conf,      /* create server configuration */
    ngx_stream_socks5_merge_srv_conf        /* merge server configuration */
};


ngx_module_t  ngx_stream_socks5_module = {
    NGX_MODULE_V1,
    &ngx_stream_socks5_module_ctx,          /* module context */
    ngx_stream_socks5_commands,             /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
ngx_stream_socks5_handler(ngx_stream_session_t *s)
{
    ngx_connection_t                 *c;
    ngx_stream_upstream_t            *u;
    ngx_stream_socks5_srv_conf_t     *pscf;
    ngx_stream_upstream_srv_conf_t   *uscf;
    ngx_stream_socks5_ctx_t          *ctx;
    ngx_pool_t                       *pool;

    s->upstream=NULL;
    c = s->connection;

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                   "socks5 connection handler");

    

    c->write->handler = ngx_stream_socks5_process_auth_handler;//ngx_stream_socks5_downstream_handler;
    c->read->handler = ngx_stream_socks5_process_auth_handler;//ngx_stream_socks5_downstream_handler;

    pool = ngx_create_pool(12*1024,c->log);
    ctx = ngx_pcalloc(pool, sizeof(ngx_stream_socks5_ctx_t));
    if (ctx == NULL) {
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }
    ctx->pool = pool;
    ctx->in = ngx_create_temp_buf(pool,NGX_SOCKS_AUTH_BUFFER_LEN);
    if(ctx->in==NULL){
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }
    ctx->out = ngx_create_temp_buf(pool,NGX_SOCKS_AUTH_BUFFER_LEN);
    if(ctx->out==NULL){
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }
    ctx->authed=0;
    ctx->in->last=ctx->in->pos=ctx->in->start;
    ctx->out->last=ctx->out->pos=ctx->out->start;
    ngx_stream_set_ctx(s, ctx, ngx_stream_socks5_module);

    u = s->upstream;
    if(u == NULL){
        u = ngx_pcalloc(ctx->pool, sizeof(ngx_stream_upstream_t));
        if (u == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        s->upstream = u;

        s->log_handler = ngx_stream_socks5_log_error;

        u->peer.log = c->log;
        u->peer.log_error = NGX_ERROR_ERR;
        u->resolved=NULL;

        if (ngx_stream_socks5_set_local(s, u, pscf->local) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        u->peer.type = c->type;
        u->start_sec = ngx_time();
        s->upstream_states = ngx_array_create(ctx->pool, 1, sizeof(ngx_stream_upstream_state_t));
        if (s->upstream_states == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        if (c->type == SOCK_STREAM) {
            u_char *p = ngx_pnalloc(ctx->pool, pscf->buffer_size);
            if (p == NULL) {
                ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
                return;
            }

            u->downstream_buf.start = p;
            u->downstream_buf.end = p + pscf->buffer_size;
            u->downstream_buf.pos = p;
            u->downstream_buf.last = p;

            if (c->read->ready) {
                ngx_post_event(c->read, &ngx_posted_events);
            }
        }

        uscf = pscf->upstream;
        if (uscf == NULL) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0, "no upstream configuration");
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        u->upstream = uscf;

        if (uscf->peer.init(s, uscf) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
        //s->upstream->peer.notify = ngx_stream_upstream_notify_round_robin_peer;
        u->peer.start_time = ngx_current_msec;

        if (pscf->next_upstream_tries
            && u->peer.tries > pscf->next_upstream_tries)
        {
            u->peer.tries = pscf->next_upstream_tries;
        }
    }
    ngx_stream_socks5_process_auth_handler(c->read);
    
}

static void ngx_stream_socks5_process_auth_handler(ngx_event_t *ev)
{
    ngx_connection_t             *c;
    ngx_stream_session_t         *s;
    ngx_stream_upstream_t        *u;
    ssize_t                      n;
    ngx_stream_core_srv_conf_t    *cscf;
    ngx_stream_socks5_srv_conf_t  *pscf;
    socks5_request_t              *socks5_req;
    socks5_response_t             *socks5_res;
    ngx_stream_socks5_ctx_t       *ctx;
    ngx_resolver_ctx_t            *resolver_ctx;

    c = ev->data;
    s = c->data;

    c = s->connection;
    ctx =  (ngx_stream_socks5_ctx_t*)ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);
    if (ev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "client connection timed out");
        ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
        return;
    }
    if (ev->ready) {
        //ngx_log_error(NGX_ERROR_ERR,c->log,0," >>>>>>>>> recv %d.",NGX_SOCKS_AUTH_BUFFER_LEN - (ctx->in->last-ctx->in->pos));	
		n = c->recv(c, ctx->in->last,NGX_SOCKS_AUTH_BUFFER_LEN - (ctx->in->last-ctx->in->pos));
        if(n>0){
            ctx->in->last=ctx->in->last+n;
        }
	} else {
		n = NGX_AGAIN;
	}

    if (n == 0 || n == NGX_ERROR) {
		c->error = 1;
		ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
		return ;
	}
    
    if (n == NGX_AGAIN){
        if (!ev->timer_set) {
			ngx_add_timer(c->read, pscf->timeout);
		}

		if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
			ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
		}
        return;
    }   

    socks5_req = (socks5_request_t*)ctx->in->pos;
    socks5_res = (socks5_response_t*)ctx->out->pos;
    socks5_res->ver = SOCKS5_VERSION;
    socks5_res->cmd = SOCKS5_REP_SUCCEED;
    socks5_res->rsv = 0;
    socks5_res->atype = SOCKS5_IPV4;

    if(!ctx->authed){
        if((ctx->in->last-ctx->in->pos) < 2){
            if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        socks5_res->cmd=SOCKS5_AUTH_USERPASS;
        if(1 == socks5_req->ver){//"server [%d]: wrong subnegotiation version need to be 0x01"
            socks5_auth_req_t *req=(socks5_auth_req_t*)ctx->in->pos;
            ngx_memcpy(&(req->plen), ctx->in->pos + 2 + ((int)req->ulen), 2);
            ngx_memcpy(req->passwd, ctx->in->pos + 3 +((int)req->ulen), (int)req->plen);
            
            if(pscf->passwd_list == NGX_CONF_UNSET_PTR){
                ctx->authed=1;
            }else{
                ngx_stream_socks5_auth_t *auth=pscf->passwd_list->elts;
                size_t i=0;
                for(; i<pscf->passwd_list->nelts; ++i)
                {
                    auth=auth+i;
                    if((int)req->ulen == auth->uname.len && (int)req->plen == auth->passwd.len
                        && ngx_strncmp(auth->uname.data,req->uname,(int)req->ulen)==0 
                        && ngx_strncmp(auth->passwd.data,req->passwd,(int)req->plen)==0){
                        ctx->authed=1;
                        break;
                    }
                }
            }
            socks5_res->ver = 0x01;
        }else{
            if(pscf->passwd_list != NGX_CONF_UNSET_PTR){
                ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
		        return ;
            }
            ctx->authed=1;
        }
        socks5_res->cmd=(ctx->authed) ? 0x00 : 0xFF;

        ctx->out->last=ctx->out->pos + 2;
		ctx->in->last=ctx->in->pos=ctx->in->start;
        ngx_socks5_send(c->write);
        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }
        
        return;
    }

    if((ctx->in->last-ctx->in->pos) < (int)sizeof(socks5_request_t)) {
        if (!ev->timer_set) {
            ngx_add_timer(c->read, pscf->timeout);
        }

        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }
        return;
    }


    if (SOCKS5_VERSION != socks5_req->ver
			|| SOCKS5_CMD_CONNECT != socks5_req->cmd)
	{
		socks5_res->cmd = SOCKS5_REP_CMD_NOT_SUPPORTED;
		ctx->out->last=ctx->out->pos + 4;        
		ctx->in->last=ctx->in->pos=ctx->in->start;
        ngx_socks5_send(c->write);

        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }
		return;
	}

    if (SOCKS5_IPV4 == socks5_req->atype)
	{
		/*bzero((char *)&s->r_addr, sizeof(struct sockaddr_in));
		s->r_addr.sin_family = AF_INET;

		ngx_memcpy(&(s->r_addr.sin_addr.s_addr), start + 4, 4);
		ngx_memcpy(&(s->r_addr.sin_port), start + 8, 2);
		ngx_log_error(NGX_ERROR_ERR,c->log,0,"remote IP:PORT = %s:%d.",inet_ntoa(s->r_addr.sin_addr), htons(s->r_addr.sin_port));		

		s->in.data -= s->in.len;
		s->in.len = 0;*/
        ctx->r_addr_len = sizeof(struct sockaddr_in);
        bzero((char *)&ctx->r_addr, ctx->r_addr_len);
		ctx->r_addr.sin_family = AF_INET;
		ngx_memcpy(&(ctx->r_addr.sin_addr.s_addr), ctx->in->pos + 4, 4);
		ngx_memcpy(&(ctx->r_addr.sin_port), ctx->in->pos + 8, 2);
		ngx_log_error(NGX_ERROR_ERR,c->log,0,"remote IP:PORT = %s:%d.",inet_ntoa(ctx->r_addr.sin_addr), htons(ctx->r_addr.sin_port));	

        ctx->out->last=ctx->out->pos + 4;
        ngx_memcpy(ctx->out->last, ctx->in->pos + 4, 4);
        ctx->out->last += 4;
        ngx_memcpy(ctx->out->last, ctx->in->pos + 8, 2);
        ctx->out->last += 2;
		ctx->in->last=ctx->in->pos=ctx->in->start;


        ngx_socks5_send(c->write);

        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
	}else if (SOCKS5_DOMAIN == socks5_req->atype)
	{
        cscf = ngx_stream_get_module_srv_conf(s, ngx_stream_core_module);

        resolver_ctx = ngx_resolve_start(cscf->resolver, NULL);//
        if (resolver_ctx == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        if (resolver_ctx == NGX_NO_RESOLVER) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0, "no resolver defined to resolve ");//%V, host);
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        ngx_memcpy(&(resolver_ctx->name.len), ctx->in->pos + 4, 1);
        resolver_ctx->name.data = ctx->in->pos+5;
		ngx_memcpy(&(ctx->r_addr.sin_port), ctx->in->pos + 5 + resolver_ctx->name.len, 2);

        resolver_ctx->handler = ngx_stream_socks5_resolve_handler;
        resolver_ctx->data = s;
        resolver_ctx->timeout = cscf->resolver_timeout;

        u = s->upstream;
        u->resolved = ngx_pcalloc(ctx->pool,sizeof(ngx_stream_upstream_resolved_t));
        if (u->resolved == NULL) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0, "can't alloc memory: %zd",sizeof(ngx_stream_upstream_resolved_t));
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
        u->resolved->ctx=resolver_ctx;

        ngx_log_debug2(NGX_LOG_DEBUG_STREAM, c->log, 0,
                   "remote domain = %s:%d",resolver_ctx->name.data,htons(ctx->r_addr.sin_port));

        if (ngx_resolve_name(resolver_ctx) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
		

		/*

		ngx_memcpy(&(s->r_addr.sin_addr.s_addr), start + 4, 4);
		ngx_memcpy(&(s->r_addr.sin_port), start + 8, 2);
		*/
		

		ctx->in->last=ctx->in->pos=ctx->in->start;

        
        /*
        if (url.addrs) {
            u->resolved->sockaddr = cscf->resolver->sock.sockaddr;
            u->resolved->socklen = url.addrs[0].socklen;
            u->resolved->name = url.addrs[0].name;
            u->resolved->naddrs = 1;
        }

        u->resolved->host = url.host;
        u->resolved->port = url.port;
        u->resolved->no_port = url.no_port;
        */
        return;
	}else {
		socks5_res->cmd = SOCKS5_REP_CMD_NOT_SUPPORTED;
		ctx->out->last=ctx->out->pos + 4;
		ctx->in->last=ctx->in->pos=ctx->in->start;
        //ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
        ngx_socks5_send(c->write);

        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }
        return;
	}

    //if(1==1)return;
    c->write->handler = ngx_stream_socks5_downstream_handler;
    c->read->handler = ngx_stream_socks5_downstream_handler;
    ngx_stream_socks5_connect(s);
}

static ngx_int_t
ngx_stream_socks5_set_local(ngx_stream_session_t *s, ngx_stream_upstream_t *u,
    ngx_stream_upstream_local_t *local)
{
    ngx_int_t    rc;
    ngx_str_t    val;
    ngx_addr_t  *addr;

    if (local == NULL) {
        u->peer.local = NULL;
        return NGX_OK;
    }

#if (NGX_HAVE_TRANSPARENT_socks5)
    u->peer.transparent = local->transparent;
#endif

    if (local->value == NULL) {
        u->peer.local = local->addr;
        return NGX_OK;
    }

    if (ngx_stream_complex_value(s, local->value, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    if (val.len == 0) {
        return NGX_OK;
    }

    addr = ngx_palloc(s->connection->pool, sizeof(ngx_addr_t));
    if (addr == NULL) {
        return NGX_ERROR;
    }

    rc = ngx_parse_addr_port(s->connection->pool, addr, val.data, val.len);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "invalid local address \"%V\"", &val);
        return NGX_OK;
    }

    addr->name = val;
    u->peer.local = addr;

    return NGX_OK;
}


static void
ngx_stream_socks5_connect(ngx_stream_session_t *s)
{
    ngx_int_t                       rc;
    ngx_connection_t                *c, *pc;
    ngx_stream_upstream_t           *u;
    ngx_stream_socks5_srv_conf_t    *pscf;
    //ngx_stream_upstream_main_conf_t *umcf;

    c = s->connection;

    c->log->action = "connecting to upstream";

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    u = s->upstream;
    // create upstream
    //if(1==1)return;
    
    

    u->connected = 0;
    u->proxy_protocol = pscf->socks5_protocol;

    if (u->state) {
        u->state->response_time = ngx_current_msec - u->state->response_time;
    }else{
        u->state = ngx_array_push(s->upstream_states);
        if (u->state == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
        ngx_memzero(u->state, sizeof(ngx_stream_upstream_state_t));
        u->state->connect_time = (ngx_msec_t) -1;
        u->state->first_byte_time = (ngx_msec_t) -1;
        u->state->response_time = ngx_current_msec;
    }    

    

    rc = ngx_event_connect_peer(&u->peer);

    ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, "socks5 connect: %i", rc);

    if (rc == NGX_ERROR) {
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    u->state->peer = u->peer.name;

    if (rc == NGX_BUSY) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0, "no live upstreams");
        ngx_stream_socks5_finalize(s, NGX_STREAM_BAD_GATEWAY);
        return;
    }

    if (rc == NGX_DECLINED) {
        ngx_stream_socks5_next_upstream(s);
        //ngx_stream_socks5_finalize(s, NGX_STREAM_BAD_GATEWAY);
        return;
    }

    /* rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE */

    pc = u->peer.connection;

    pc->data = s;
    pc->log = c->log;
    pc->pool = c->pool;
    pc->read->log = c->log;
    pc->write->log = c->log;

    if (rc != NGX_AGAIN) {
        ngx_stream_socks5_init_upstream(s);
        return;
    }

    pc->read->handler = ngx_stream_socks5_connect_handler;
    pc->write->handler = ngx_stream_socks5_connect_handler;

    ngx_add_timer(pc->write, pscf->connect_timeout);
}


static void
ngx_stream_socks5_init_upstream(ngx_stream_session_t *s)
{
    int                             tcp_nodelay;
    u_char                          *p;
    ngx_chain_t                     *cl;
    ngx_connection_t                *c, *pc;
    ngx_log_handler_pt              handler;
    ngx_stream_upstream_t           *u;
    ngx_stream_core_srv_conf_t      *cscf;
    ngx_stream_socks5_srv_conf_t    *pscf;
    ngx_stream_socks5_ctx_t         *ctx;

    u = s->upstream;
    pc = u->peer.connection;

    cscf = ngx_stream_get_module_srv_conf(s, ngx_stream_core_module);
    ctx = ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);

    if (pc->type == SOCK_STREAM
        && cscf->tcp_nodelay
        && pc->tcp_nodelay == NGX_TCP_NODELAY_UNSET)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_STREAM, pc->log, 0, "tcp_nodelay");

        tcp_nodelay = 1;

        if (setsockopt(pc->fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int)) == -1)
        {
            ngx_connection_error(pc, ngx_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed");
            ngx_stream_socks5_next_upstream(s);
            //ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            
            return;
        }

        pc->tcp_nodelay = NGX_TCP_NODELAY_SET;
    }

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    c = s->connection;

    if (c->log->log_level >= NGX_LOG_INFO) {
        ngx_str_t  str;
        u_char     addr[NGX_SOCKADDR_STRLEN];

        str.len = NGX_SOCKADDR_STRLEN;
        str.data = addr;

        if (ngx_connection_local_sockaddr(pc, &str, 1) == NGX_OK) {
            handler = c->log->handler;
            c->log->handler = NULL;

            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          "%ssocks5 %V connected to %V",
                          pc->type == SOCK_DGRAM ? "udp " : "",
                          &str, u->peer.name);

            c->log->handler = handler;
        }
    }

    u->state->connect_time = ngx_current_msec - u->state->response_time;

    if (u->peer.notify) {
        u->peer.notify(&u->peer, u->peer.data,
                       NGX_STREAM_UPSTREAM_NOTIFY_CONNECT);
    }

    c->log->action = "socks5ing connection";

    if (u->upstream_buf.start == NULL) {
        p = ngx_pnalloc(ctx->pool, pscf->buffer_size);
        if (p == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        u->upstream_buf.start = p;
        u->upstream_buf.end = p + pscf->buffer_size;
        u->upstream_buf.pos = p;
        u->upstream_buf.last = p;
    }

    if (c->buffer && c->buffer->pos < c->buffer->last) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0,
                       "stream socks5 add preread buffer: %uz",
                       c->buffer->last - c->buffer->pos);

        cl = ngx_chain_get_free_buf(ctx->pool, &u->free);
        if (cl == NULL) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        *cl->buf = *c->buffer;

        cl->buf->tag = (ngx_buf_tag_t) &ngx_stream_socks5_module;
        cl->buf->flush = 1;
        cl->buf->last_buf = (c->type == SOCK_DGRAM);

        cl->next = u->upstream_out;
        u->upstream_out = cl;
    }


    if (c->type == SOCK_DGRAM && pscf->responses == 0) {
        pc->read->ready = 0;
        pc->read->eof = 1;
    }

    u->connected = 1;

    pc->read->handler = ngx_stream_socks5_upstream_handler;
    pc->write->handler = ngx_stream_socks5_upstream_handler;

    if (pc->read->ready || pc->read->eof) {
        ngx_post_event(pc->read, &ngx_posted_events);
    }

    ngx_stream_socks5_process(s, 0, 1);
}


static void
ngx_stream_socks5_downstream_handler(ngx_event_t *ev)
{
    ngx_stream_socks5_process_connection(ev, ev->write);
}


static void
ngx_stream_socks5_resolve_handler(ngx_resolver_ctx_t *rctx)
{
    ngx_stream_session_t            *s;
    ngx_stream_upstream_t           *u;
    //ngx_stream_socks5_srv_conf_t     *pscf;
    ngx_stream_upstream_resolved_t  *ur;
    ngx_stream_socks5_ctx_t         *ctx;
    ngx_uint_t                      i;

    s = rctx->data;

    u = s->upstream;
    ur = u->resolved;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "stream upstream resolve");

    if (rctx->state) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "%V could not be resolved (%i: %s)",
                      &rctx->name, rctx->state,
                      ngx_resolver_strerror(rctx->state));

        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    //ngx_log_error(NGX_ERROR_INFO, s->connection->log, 0,">>>>>>>>>>> %p",s->connection->log),

    ur->naddrs = rctx->naddrs;
    ur->addrs = rctx->addrs;

#if (NGX_DEBUG)
    {
    u_char      text[NGX_SOCKADDR_STRLEN];
    ngx_str_t   addr;
    ngx_uint_t  i;

    addr.data = text;

    for (i = 0; i < rctx->naddrs; i++) {
        addr.len = ngx_sock_ntop(ur->addrs[i].sockaddr, ur->addrs[i].socklen,
                                 text, NGX_SOCKADDR_STRLEN, 0);

        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "name was resolved to %V", &addr);
    }
    }
#endif

    

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);    

    ctx->out->last=ctx->out->pos + 4;
    ctx->r_addr_len = sizeof(struct sockaddr_in);
    ctx->r_addr.sin_family = AF_INET;

    for (i = 0; i < rctx->naddrs; i++) {
        ngx_memcpy(&(ctx->r_addr.sin_addr.s_addr), &(((struct sockaddr_in*)rctx->addrs[i].sockaddr)->sin_addr), 4);
        memcpy(ctx->out->last + 4, &(ctx->r_addr.sin_addr.s_addr), 4);
        ctx->out->last += 4;
        memcpy(ctx->out->last, &(ctx->r_addr.sin_port), 2);
        ctx->out->last += 2;
        //ctx->in->last=ctx->in->pos=ctx->in->start;
        ngx_log_error(NGX_ERROR_INFO,s->connection->log,0,"name was resolved to %s:%d.",inet_ntoa(ctx->r_addr.sin_addr), htons(ctx->r_addr.sin_port));
        goto found;
    }
    ngx_resolve_name_done(rctx);
    ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
    return;

found:

    ngx_socks5_send(s->connection->write);
    /*
    if (ngx_handle_read_event(s->connection->read, 0) != NGX_OK) {
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }*/

    
    u->peer.start_time = ngx_current_msec;
    /*
    if (ngx_stream_socks5_create_round_robin_peer(s, ur) != NGX_OK) {
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }*/

    ngx_resolve_name_done(rctx);
    ur->ctx = NULL;
    //ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
    /*
    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    if (pscf->next_upstream_tries
        && u->peer.tries > pscf->next_upstream_tries)
    {
        u->peer.tries = pscf->next_upstream_tries;
    }*/

    s->connection->write->handler = ngx_stream_socks5_downstream_handler;
    s->connection->read->handler = ngx_stream_socks5_downstream_handler;
    
    ngx_stream_socks5_connect(s);
}


static void
ngx_stream_socks5_upstream_handler(ngx_event_t *ev)
{
    ngx_stream_socks5_process_connection(ev, !ev->write);
}


static void
ngx_stream_socks5_process_connection(ngx_event_t *ev, ngx_uint_t from_upstream)
{
    ngx_connection_t             *c, *pc;
    ngx_stream_session_t         *s;
    ngx_stream_upstream_t        *u;
    ngx_stream_socks5_srv_conf_t  *pscf;

    c = ev->data;
    s = c->data;
    u = s->upstream;

    c = s->connection;
    pc = u->peer.connection;

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    if (ev->timedout) {
        ev->timedout = 0;

        if (ev->delayed) {
            ev->delayed = 0;

            if (!ev->ready) {
                if (ngx_handle_read_event(ev, 0) != NGX_OK) {
                    ngx_stream_socks5_finalize(s,
                                              NGX_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }

                if (u->connected && !c->read->delayed && !pc->read->delayed) {
                    ngx_add_timer(c->write, pscf->timeout);
                }

                return;
            }

        } else {
            if (s->connection->type == SOCK_DGRAM) {
                if (pscf->responses == NGX_MAX_INT32_VALUE) {

                    /*
                     * successfully terminate timed out UDP session
                     * with unspecified number of responses
                     */

                    pc->read->ready = 0;
                    pc->read->eof = 1;

                    ngx_stream_socks5_process(s, 1, 0);
                    return;
                }

                if (u->received == 0) {
                    //ngx_stream_socks5_next_upstream(s);
                    ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }
            }

            ngx_connection_error(c, NGX_ETIMEDOUT, "connection timed out");
            ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
            return;
        }

    } else if (ev->delayed) {

        ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                       "stream connection delayed");

        if (ngx_handle_read_event(ev, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    if (from_upstream && !u->connected) {
        return;
    }

    ngx_stream_socks5_process(s, from_upstream, ev->write);
}


static void
ngx_stream_socks5_connect_handler(ngx_event_t *ev)
{
    ngx_connection_t      *c;
    ngx_stream_session_t  *s;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ERR, c->log, NGX_ETIMEDOUT, "upstream timed out");
        ngx_stream_socks5_next_upstream(s);
        //ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_del_timer(c->write);

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                   "stream socks5 connect upstream");

    if (ngx_stream_socks5_test_connect(c) != NGX_OK) {
        ngx_stream_socks5_next_upstream(s);
        //ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_stream_socks5_init_upstream(s);
}


static ngx_int_t
ngx_stream_socks5_test_connect(ngx_connection_t *c)
{
    int        err;
    socklen_t  len;


    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        if (err) {
            (void) ngx_connection_error(c, err, "connect() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static void
ngx_stream_socks5_process(ngx_stream_session_t *s, ngx_uint_t from_upstream,
    ngx_uint_t do_write)
{
    off_t                           *received, limit;
    size_t                          size, limit_rate;
    ssize_t                         n;
    ngx_buf_t                       *b;
    ngx_int_t                       rc;
    ngx_uint_t                      flags;
    ngx_msec_t                      delay;
    ngx_chain_t                     *cl, **ll, **out, **busy;
    ngx_connection_t                *c, *pc, *src, *dst;
    ngx_log_handler_pt              handler;
    ngx_stream_upstream_t           *u;
    ngx_stream_socks5_srv_conf_t    *pscf;
    ngx_stream_socks5_ctx_t         *ctx;

    u = s->upstream;

    c = s->connection;
    pc = u->connected ? u->peer.connection : NULL;
    ctx = ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);

    if (c->type == SOCK_DGRAM && (ngx_terminate || ngx_exiting)) {

        /* socket is already closed on worker shutdown */

        handler = c->log->handler;
        c->log->handler = NULL;

        ngx_log_error(NGX_LOG_INFO, c->log, 0, "disconnected on shutdown");

        c->log->handler = handler;

        ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
        return;
    }

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    if (from_upstream) {
        src = pc;
        dst = c;
        b = &u->upstream_buf;
        limit_rate = pscf->download_rate;
        received = &u->received;
        out = &u->downstream_out;
        busy = &u->downstream_busy;

    } else {
        src = c;
        dst = pc;
        b = &u->downstream_buf;
        limit_rate = pscf->upload_rate;
        received = &s->received;
        out = &u->upstream_out;
        busy = &u->upstream_busy;
    }

    for ( ;; ) {

        if (do_write && dst) {

            if (*out || *busy || dst->buffered) {
                rc = ngx_stream_top_filter(s, *out, from_upstream);

                if (rc == NGX_ERROR) {
                    if (c->type == SOCK_DGRAM && !from_upstream) {
                        //ngx_stream_socks5_next_upstream(s);
                        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
                        return;
                    }

                    ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
                    return;
                }

                ngx_chain_update_chains(ctx->pool, &u->free, busy, out,
                                      (ngx_buf_tag_t) &ngx_stream_socks5_module);

                if (*busy == NULL) {
                    b->pos = b->start;
                    b->last = b->start;
                }
            }
        }

        size = b->end - b->last;

        if (size && src->read->ready && !src->read->delayed
            && !src->read->error)
        {
            if (limit_rate) {
                limit = (off_t) limit_rate * (ngx_time() - u->start_sec + 1)
                        - *received;

                if (limit <= 0) {
                    src->read->delayed = 1;
                    delay = (ngx_msec_t) (- limit * 1000 / limit_rate + 1);
                    ngx_add_timer(src->read, delay);
                    break;
                }

                if ((off_t) size > limit) {
                    size = (size_t) limit;
                }
            }

            n = src->recv(src, b->last, size);

            if (n == NGX_AGAIN) {
                break;
            }

            if (n == NGX_ERROR) {
                if (c->type == SOCK_DGRAM && u->received == 0) {
                    ngx_stream_socks5_next_upstream(s);
                    //ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }

                src->read->eof = 1;
                n = 0;
            }

            if (n >= 0) {
                if (limit_rate) {
                    delay = (ngx_msec_t) (n * 1000 / limit_rate);

                    if (delay > 0) {
                        src->read->delayed = 1;
                        ngx_add_timer(src->read, delay);
                    }
                }

                if (from_upstream) {
                    if (u->state->first_byte_time == (ngx_msec_t) -1) {
                        u->state->first_byte_time = ngx_current_msec
                                                    - u->state->response_time;
                    }
                }

                if (c->type == SOCK_DGRAM && ++u->responses == pscf->responses)
                {
                    src->read->ready = 0;
                    src->read->eof = 1;
                }

                for (ll = out; *ll; ll = &(*ll)->next) { /* void */ }

                cl = ngx_chain_get_free_buf(ctx->pool, &u->free);
                if (cl == NULL) {
                    ngx_stream_socks5_finalize(s,
                                              NGX_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }

                *ll = cl;

                cl->buf->pos = b->last;
                cl->buf->last = b->last + n;
                cl->buf->tag = (ngx_buf_tag_t) &ngx_stream_socks5_module;

                cl->buf->temporary = (n ? 1 : 0);
                cl->buf->last_buf = src->read->eof;
                cl->buf->flush = 1;

                *received += n;
                b->last += n;
                do_write = 1;

                continue;
            }
        }

        break;
    }
    //ngx_log_error(NGX_LOG_INFO, c->log, 0,"#################### \t%s",b->pos);
    if (src->read->eof && dst && (dst->read->eof || !dst->buffered)) {
        handler = c->log->handler;
        c->log->handler = NULL;

        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "%s%s disconnected"
                      ", bytes from/to client:%O/%O"
                      ", bytes from/to upstream:%O/%O",
                      src->type == SOCK_DGRAM ? "udp " : "",
                      from_upstream ? "upstream" : "client",
                      s->received, c->sent, u->received, pc ? pc->sent : 0);

        c->log->handler = handler;

        ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
        return;
    }

    flags = src->read->eof ? NGX_CLOSE_EVENT : 0;

    if (!src->shared && ngx_handle_read_event(src->read, flags) != NGX_OK) {
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (dst) {
        if (!dst->shared && ngx_handle_write_event(dst->write, 0) != NGX_OK) {
            ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        if (!c->read->delayed && !pc->read->delayed) {
            ngx_add_timer(c->write, pscf->timeout);

        } else if (c->write->timer_set) {
            ngx_del_timer(c->write);
        }
    }
}


static void
ngx_stream_socks5_next_upstream(ngx_stream_session_t *s)
{
    ngx_msec_t                    timeout;
    ngx_connection_t             *pc;
    ngx_stream_upstream_t        *u;
    ngx_stream_socks5_srv_conf_t  *pscf;

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "stream socks5 next upstream");

    u = s->upstream;
    pc = u->peer.connection;

    if (u->upstream_out || u->upstream_busy || (pc && pc->buffered)) {
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "pending buffers on next upstream");
        ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, NGX_PEER_FAILED);
        u->peer.sockaddr = NULL;
    }

    pscf = ngx_stream_get_module_srv_conf(s, ngx_stream_socks5_module);

    timeout = pscf->next_upstream_timeout;

    if (u->peer.tries == 0
        || !pscf->next_upstream
        || (timeout && ngx_current_msec - u->peer.start_time >= timeout))
    {
        ngx_stream_socks5_finalize(s, NGX_STREAM_BAD_GATEWAY);
        return;
    }

    if (pc) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "close socks5 upstream connection: %d", pc->fd);

        u->state->bytes_received = u->received;
        u->state->bytes_sent = pc->sent;

        ngx_close_connection(pc);
        u->peer.connection = NULL;
    }

    ngx_stream_socks5_connect(s);
}


static void
ngx_stream_socks5_finalize(ngx_stream_session_t *s, ngx_uint_t rc)
{
    //if(s->connection==NULL)return;
    ngx_connection_t        *pc;
    ngx_stream_upstream_t   *u;
    ngx_stream_socks5_ctx_t *ctx;
    ngx_pool_t          *p;
    //ngx_log_error(NGX_LOG_ERR,s->connection->log, 0, "ngx_destroy_pool =>: \"%p\"", s->connection->pool);
    //for (p = s->connection->pool, n = s->connection->pool->d.next; /* void */p && n; p = n, n = n->d.next) {
        //ngx_log_error(NGX_LOG_ERR,s->connection->log, 0, "\tpool =>: \"%p\"", *p);
    //}
    ngx_log_debug1(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "finalize stream socks5: %i", rc);

    
    //if(s->connection->pool==NULL)return;

    u = s->upstream;

    if (u == NULL) {
        goto noupstream;
    }

    if (u->resolved && u->resolved->ctx) {
        ngx_resolve_name_done(u->resolved->ctx);
        u->resolved->ctx = NULL;
    }

    pc = u->peer.connection;

    if (u->state) {
        u->state->response_time = ngx_current_msec - u->state->response_time;

        if (pc) {
            u->state->bytes_received = u->received;
            u->state->bytes_sent = pc->sent;
        }
    }

    if (u->peer.free && u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, 0);
        u->peer.sockaddr = NULL;
    }

    if (pc) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "close stream socks5 upstream connection: %d", pc->fd);


        ngx_close_connection(pc);
        u->peer.connection = NULL;
    }

noupstream:
    ctx = ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);
    if (ctx) {
        p = ctx->pool;
        ngx_stream_delete_ctx(s,ngx_stream_socks5_module);
        ngx_destroy_pool(p);
    }

    ngx_stream_finalize_session(s, rc);

}


static u_char *
ngx_stream_socks5_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char                 *p;
    ngx_connection_t       *pc;
    ngx_stream_session_t   *s;
    ngx_stream_upstream_t  *u;

    s = log->data;

    u = s->upstream;

    p = buf;

    if (u->peer.name) {
        p = ngx_snprintf(p, len, ", upstream: \"%V\"", u->peer.name);
        len -= p - buf;
    }

    pc = u->peer.connection;

    p = ngx_snprintf(p, len,
                     ", bytes from/to client:%O/%O"
                     ", bytes from/to upstream:%O/%O",
                     s->received, s->connection->sent,
                     u->received, pc ? pc->sent : 0);

    return p;
}


static void *
ngx_stream_socks5_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_socks5_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_socks5_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->timeout = NGX_CONF_UNSET_MSEC;
    conf->next_upstream_timeout = NGX_CONF_UNSET_MSEC;
    conf->buffer_size = NGX_CONF_UNSET_SIZE;
    conf->upload_rate = NGX_CONF_UNSET_SIZE;
    conf->download_rate = NGX_CONF_UNSET_SIZE;
    conf->responses = NGX_CONF_UNSET_UINT;
    conf->next_upstream_tries = NGX_CONF_UNSET_UINT;
    conf->next_upstream = NGX_CONF_UNSET;
    conf->socks5_protocol = NGX_CONF_UNSET;
    conf->local = NGX_CONF_UNSET_PTR;
    conf->passwd_list = NGX_CONF_UNSET_PTR;

    return conf;
}


static char *
ngx_stream_socks5_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_stream_socks5_srv_conf_t *prev = parent;
    ngx_stream_socks5_srv_conf_t *conf = child;

    ngx_conf_merge_msec_value(conf->connect_timeout,   prev->connect_timeout, 60000);

    ngx_conf_merge_msec_value(conf->timeout,           prev->timeout, 10 * 60000);

    ngx_conf_merge_msec_value(conf->next_upstream_timeout,                              prev->next_upstream_timeout, 0);

    ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size, 16384);

    ngx_conf_merge_size_value(conf->upload_rate,prev->upload_rate, 0);

    ngx_conf_merge_size_value(conf->download_rate, prev->download_rate, 0);

    ngx_conf_merge_uint_value(conf->responses,  prev->responses, NGX_MAX_INT32_VALUE);

    ngx_conf_merge_uint_value(conf->next_upstream_tries,        prev->next_upstream_tries, 0);

    ngx_conf_merge_value(conf->next_upstream, prev->next_upstream, 1);

    ngx_conf_merge_value(conf->socks5_protocol, prev->socks5_protocol, 0);

    ngx_conf_merge_ptr_value(conf->local, prev->local, NULL);
    ngx_conf_merge_ptr_value(conf->passwd_list, prev->passwd_list, NGX_CONF_UNSET_PTR);

    return NGX_CONF_OK;
}


static char *
ngx_stream_socks5_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_socks5_srv_conf_t *pscf = conf;

    //ngx_url_t                            u;
    //ngx_str_t                           *value;

    ngx_stream_core_srv_conf_t          *cscf;


    if (pscf->upstream) {
        return "is duplicate";
    }

    cscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_core_module);

    cscf->handler = ngx_stream_socks5_handler;
    

    //value = cf->args->elts;

    //url = &value[1];


    pscf->upstream = ngx_pcalloc(cf->pool, sizeof(ngx_stream_upstream_srv_conf_t));
    if (pscf->upstream == NULL) {
        return NGX_CONF_ERROR;
    }
    pscf->upstream->peer.init=ngx_stream_socks5_init_peer;
    return NGX_CONF_OK;
}

static void del_tail(u_char *str,int len){
	u_char *p;	
	for(p=str+(len-1); *p==' ' || *p=='\t'|| *p=='\n' ; --p)*p='\0';	
}

static char *parse_properties(ngx_pool_t *pool,const char *pathname,ngx_array_t **list){
    ngx_stream_socks5_auth_t *auth;
    FILE *file;
	int buffSize=1024*2;
    char _line[buffSize];
	char *line=_line;
	int length = 0, equal = 1; //equal will record the location of the '='  
    char *begin;
	
    file = fopen(pathname, "r");
	if(file==NULL)
	{
		perror("can't open configuration file");
		return NGX_CONF_ERROR;
	}
    while(fgets(line, buffSize, file)){
        if (!strncmp("#", line,1)){
			continue;
		}
        length++;
    }
    fclose(file);
    if(length==0)return NGX_CONF_OK;

    *list = ngx_array_create(pool,length,sizeof(ngx_stream_socks5_auth_t));
    if(*list==NULL)return NGX_CONF_ERROR;
    file = fopen(pathname, "r");
    while(fgets(line, buffSize, file)){
        if (!strncmp("#", line,1)){
			continue;
		}
		length = 0, equal = 1;
        length = strlen(line);
        for(begin = line; *begin != '=' && equal <= length; begin ++){  
                equal++;  
        }
        auth = ngx_array_push(*list);

        auth->uname.len = equal-1;
        auth->uname.data = ngx_pcalloc(pool, auth->uname.len);
        if(auth->uname.data==NULL)return NGX_CONF_ERROR;
        strncpy((char*)auth->uname.data, line, auth->uname.len);
        
        line+=equal;
        auth->passwd.len = length-equal-1;
        auth->passwd.data = ngx_pcalloc(pool, auth->passwd.len);
        if(auth->passwd.data==NULL)return NGX_CONF_ERROR;
        strncpy((char*)auth->passwd.data, line, auth->passwd.len);        
        //conf->workdir=(char*)malloc(length-equal);
        //strncpy(conf->workdir, line, length - equal);
        del_tail(auth->passwd.data,auth->passwd.len);       
    }
    fclose(file);
    return NGX_CONF_OK;
}

static char *ngx_conf_set_str_pwd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_socks5_srv_conf_t *pscf = conf;
    ngx_str_t                    *value;

    if (pscf->passwd_list!=NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    return parse_properties(cf->pool,(char*)value[1].data,&pscf->passwd_list);
}

u_char *
ngx_socks5_protocol_write(ngx_connection_t *c, u_char *buf, u_char *last)
{
    ngx_uint_t  port, lport;

    if (last - buf < NGX_PROXY_PROTOCOL_MAX_HEADER) {
        return NULL;
    }

    if (ngx_connection_local_sockaddr(c, NULL, 0) != NGX_OK) {
        return NULL;
    }

    switch (c->sockaddr->sa_family) {

    case AF_INET:
        buf = ngx_cpymem(buf, "PROXY TCP4 ", sizeof("PROXY TCP4 ") - 1);
        break;

#if (NGX_HAVE_INET6)
    case AF_INET6:
        buf = ngx_cpymem(buf, "PROXY TCP6 ", sizeof("PROXY TCP6 ") - 1);
        break;
#endif

    default:
        return ngx_cpymem(buf, "PROXY UNKNOWN" CRLF,
                          sizeof("PROXY UNKNOWN" CRLF) - 1);
    }

    buf += ngx_sock_ntop(c->sockaddr, c->socklen, buf, last - buf, 0);

    *buf++ = ' ';

    buf += ngx_sock_ntop(c->local_sockaddr, c->local_socklen, buf, last - buf,
                         0);

    port = ngx_inet_get_port(c->sockaddr);
    lport = ngx_inet_get_port(c->local_sockaddr);

    return ngx_slprintf(buf, last, " %ui %ui" CRLF, port, lport);
}


static void ngx_socks5_send(ngx_event_t *wev)
{
    ngx_int_t                  n;
    ngx_connection_t          *c;
    ngx_stream_session_t        *s;
    ngx_stream_socks5_srv_conf_t  *pscf;
    ngx_stream_socks5_ctx_t       *ctx;

    c = wev->data;
    s = c->data;

    if (wev->timedout) {
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "send timeout client");
        ngx_stream_socks5_finalize(s, NGX_STREAM_OK);
        return;
    }

    ctx =  (ngx_stream_socks5_ctx_t*)ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);

    if (ctx->out->last-ctx->out->pos == 0) {
        if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
			ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    n = c->send(c, ctx->out->pos, ctx->out->last-ctx->out->pos);

    if (n > 0) {
        ctx->out->pos=ctx->out->pos+n;

		if (ctx->out->last-ctx->out->pos != 0) {
			goto again;
		}

        if (wev->timer_set) {
            ngx_del_timer(wev);
        }

        return;
    }

    if (n == NGX_ERROR) {
		ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    /* n == NGX_AGAIN */

again:
    pscf = ngx_stream_conf_get_module_srv_conf(s, ngx_stream_socks5_module);

    ngx_add_timer(c->write, pscf->timeout);

    if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
		ngx_stream_socks5_finalize(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }
}


static ngx_stream_upstream_rr_peer_t *
ngx_stream_upstream_get_peer(ngx_stream_upstream_rr_peer_data_t *rrp)
{
    time_t                          now;
    uintptr_t                       m;
    ngx_int_t                       total;
    ngx_uint_t                      i, n, p;
    ngx_stream_upstream_rr_peer_t  *peer, *best;

    now = ngx_time();

    best = NULL;
    total = 0;

#if (NGX_SUPPRESS_WARN)
    p = 0;
#endif

    for (peer = rrp->peers->peer, i = 0;
         peer;
         peer = peer->next, i++)
    {
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << i % (8 * sizeof(uintptr_t));

        if (rrp->tried[n] & m) {
            continue;
        }

        if (peer->down) {
            continue;
        }

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
            continue;
        }

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            continue;
        }

        peer->current_weight += peer->effective_weight;
        total += peer->effective_weight;

        if (peer->effective_weight < peer->weight) {
            peer->effective_weight++;
        }

        if (best == NULL || peer->current_weight > best->current_weight) {
            best = peer;
            p = i;
        }
    }

    if (best == NULL) {
        return NULL;
    }

    rrp->current = best;

    n = p / (8 * sizeof(uintptr_t));
    m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

    rrp->tried[n] |= m;

    best->current_weight -= total;

    if (now - best->checked > best->fail_timeout) {
        best->checked = now;
    }

    return best;
}

static ngx_int_t ngx_stream_socks5_get_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_stream_session_t            *s;
    ngx_stream_socks5_ctx_t         *ctx;

    s = (ngx_stream_session_t*)data;
    ctx =  (ngx_stream_socks5_ctx_t*)ngx_stream_get_module_ctx(s, ngx_stream_socks5_module);

    pc->sockaddr = (struct sockaddr*)&ctx->r_addr;
    pc->socklen = ctx->r_addr_len;
    pc->name = ngx_pcalloc(s->connection->pool, sizeof(ngx_str_t));
    if(pc->name==NULL)return NGX_ERROR;
    ngx_str_set(pc->name,inet_ntoa(ctx->r_addr.sin_addr));

    return NGX_OK;

    //return NGX_BUSY;
}


static void ngx_stream_socks5_free_peer(ngx_peer_connection_t *pc, void *data,ngx_uint_t state)
{

}

static ngx_int_t ngx_stream_socks5_init_peer(ngx_stream_session_t *s, ngx_stream_upstream_srv_conf_t *us)
{

    s->upstream->peer.data=s;

    s->upstream->peer.get = ngx_stream_socks5_get_peer;
    s->upstream->peer.free = ngx_stream_socks5_free_peer;
    //s->upstream->peer.data = s;
    //s->upstream->peer.notify = ngx_stream_upstream_notify_round_robin_peer;
    //s->upstream->peer.tries = ngx_stream_upstream_tries(rrp->peers);

    return NGX_OK;
}
