#include <ngx_event.h>
#include <ngx_core.h>
#include <ngx_config.h>
#include "ddebug.h"
#include <pthread.h>
#include "ngx_http_lua_shdict.h"
#include "ngx_http_lua_util.h"
#include "ngx_http_lua_api.h"
#include <time.h>

static void  *ngx_proc_luashm_create_main_conf(ngx_conf_t *cf);
static char  *ngx_proc_luashm_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_proc_luashm_backup_create_conf(ngx_conf_t *cf);
static char *ngx_proc_luashm_backup_merge_conf(ngx_conf_t *cf, void *parent,void *child);
static ngx_int_t ngx_proc_luashm_backup_prepare(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_luashm_backup_process_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_luashm_backup_loop(ngx_cycle_t *cycle);
static void ngx_proc_luashm_backup_exit_process(ngx_cycle_t *cycle);
static int ngx_proc_luashm_backup_backup(ngx_cycle_t *cycle,ngx_shm_zone_t *zone);

//static volatile ngx_thread_t  ngx_threads[1];

typedef struct {
	uint8_t 					 value_type;
	u_short                      key_len;
    uint32_t                     value_len;
    uint32_t                     user_flags;
	uint64_t                     expires;
    u_char                       data[1];
} ngx_http_lua_shdict_node_ext_t;

typedef struct {
    ngx_str_t     	path;
	ngx_str_t		settimestamp;
	ngx_array_t     *shdict_names;
	ngx_conf_file_t *conf_file;
	ngx_pool_t      *pool;
    ngx_socket_t    fd;
	u_char			running;
} ngx_proc_luashm_backup_conf_t;


static ngx_command_t ngx_proc_luashm_backup_commands[] = {

    { ngx_string("backup_path"),
      NGX_PROC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_PROC_CONF_OFFSET,
      offsetof(ngx_proc_luashm_backup_conf_t, path),
      NULL },
	{ ngx_string("backup_shdict_names"),
      NGX_PROC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_PROC_CONF_OFFSET,
      offsetof(ngx_proc_luashm_backup_conf_t, shdict_names),
      NULL },
    { ngx_string("backup_settimer"),
      NGX_PROC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_PROC_CONF_OFFSET,
      offsetof(ngx_proc_luashm_backup_conf_t, settimestamp),
      NULL },
      ngx_null_command
};


static ngx_proc_module_t ngx_proc_luashm_backup_module_ctx = {
    ngx_string("lua_shdict_backup"),
    NULL,
    NULL,
    ngx_proc_luashm_backup_create_conf,
    ngx_proc_luashm_backup_merge_conf,
    ngx_proc_luashm_backup_prepare,
    ngx_proc_luashm_backup_process_init,
    ngx_proc_luashm_backup_loop,
    ngx_proc_luashm_backup_exit_process
};

ngx_module_t ngx_proc_luashm_backup_module = {
    NGX_MODULE_V1,
    &ngx_proc_luashm_backup_module_ctx,
    ngx_proc_luashm_backup_commands,
    NGX_PROC_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static void  *ngx_proc_luashm_create_main_conf(ngx_conf_t *cf){
	return NULL;
}
static char  *ngx_proc_luashm_init_main_conf(ngx_conf_t *cf, void *conf){
	return NGX_CONF_OK;
}

static void *ngx_proc_luashm_backup_create_conf(ngx_conf_t *cf)
{
    ngx_proc_luashm_backup_conf_t  *pbcf;

    pbcf = ngx_pcalloc(cf->pool, sizeof(ngx_proc_luashm_backup_conf_t));

    if (pbcf == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "luashm_backup create proc conf error");
        return NULL;
    }
	pbcf->shdict_names=ngx_array_create(cf->pool,1, sizeof(ngx_str_t));
    //pbcf->path.data = NGX_CONF_UNSET;

    return pbcf;
}

static char *ngx_proc_luashm_backup_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_proc_luashm_backup_conf_t  *prev = parent;
    ngx_proc_luashm_backup_conf_t  *conf = child;

    //ngx_conf_merge_uint_value(conf->port, prev->port, 0);
    ngx_conf_merge_str_value(conf->path, prev->path, NULL);
	ngx_conf_merge_str_value(conf->settimestamp, prev->settimestamp, NULL);
	ngx_conf_merge_ptr_value(conf->shdict_names, prev->shdict_names, NGX_CONF_UNSET_PTR);

    return NGX_CONF_OK;
}

static ngx_int_t ngx_proc_luashm_backup_prepare(ngx_cycle_t *cycle)
{
    ngx_proc_luashm_backup_conf_t  *pbcf;
	ngx_shm_zone_t   				*zone;
	ngx_str_t 						*name;
	ngx_uint_t 						i,max_size;

    pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_luashm_backup_module);

	if (pbcf == NULL) {
        return NGX_DECLINED;
    }
	if (pbcf->path.data == NULL) {
        return NGX_DECLINED;
    }
	if (pbcf->shdict_names->nelts == 0) {
        return NGX_DECLINED;
    }

	i = 0;
	max_size=0;
	for (; i < pbcf->shdict_names->nelts;i++)
	{
		name = ((ngx_str_t*)pbcf->shdict_names->elts)  + i;
		zone = ngx_http_lua_find_zone(name->data,name->len);
		if(max_size < zone->shm.size)max_size=zone->shm.size;
	}
	max_size+=65536*2;
	dd("create pool size: %ld", max_size);
	pbcf->pool = ngx_create_pool(max_size, cycle->log);
	if(pbcf->pool==NULL){
		return NGX_DECLINED;
	}
    return NGX_OK;
}

static int64_t GetTick(char * settimer)
{
	time_t t;
	struct tm *lt;

    t = time(NULL);
    lt = localtime(&t);

	//lt->tm_year = lt->tm_year-1900;
	//lt->tm_mon = lt->tm_mon-1;

	lt->tm_hour = atoi(settimer);
	lt->tm_min = atoi(settimer+3);
	lt->tm_sec = atoi(settimer+6);

    /*printf("%d-%0d-%0d %0d:%0d:%0d\n", iY, iM, iD, iH, iMin, iS);*/

    return mktime(lt);
}

static void ngx_proc_luashm_backup_thread_cycle(void *data)
{
	ngx_proc_luashm_backup_conf_t  	*pbcf;
    ngx_cycle_t 					*cycle;
    ngx_shm_zone_t   				*zone;
	ngx_str_t 						*name;
	ngx_uint_t 						i;
	ngx_time_t                  	*tp;
    int64_t                     	now,settimestamp;

	cycle = data;
	pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_luashm_backup_module);
	if(pbcf->settimestamp.data == NULL){
		pthread_exit((void *)0);
		return;
	}

	while(pbcf->running){
		ngx_time_update();
		if(ngx_strstr(pbcf->settimestamp.data,":")==NULL){
			settimestamp=ngx_atoi(pbcf->settimestamp.data,pbcf->settimestamp.len);
		}else{
			tp = ngx_timeofday();
	    	now = (int64_t) tp->sec * 1000 + tp->msec;
			settimestamp=GetTick((char*)pbcf->settimestamp.data)*1000;
			ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, "========= now:%lu, settimestamp:%lu =========",now,settimestamp);
			if(settimestamp<now)
				settimestamp=(24*60*60*1000)-(now-settimestamp);
			else
				settimestamp=settimestamp-now;

			if(settimestamp==0)settimestamp=1;
		}
		ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, "========= backup time:%s, sleeptime:%lu =========",pbcf->settimestamp.data,settimestamp);

		ngx_msleep(settimestamp);
		if(!pbcf->running)break;

		i = 0;
		for (; i < pbcf->shdict_names->nelts;i++)
		{
			name = ((ngx_str_t*)pbcf->shdict_names->elts)  + i;
			zone = ngx_http_lua_find_zone(name->data,name->len);
			dd("shm.name:%s,shm.name_len:%ld,shm.size:%ld  ", name->data,name->len,zone->shm.size);
			ngx_proc_luashm_backup_backup(cycle,zone);
		}
		ngx_log_error(NGX_LOG_DEBUG, cycle->log, 0, "luashm_backup %V",&ngx_cached_http_time);
		ngx_sleep(60);
		if(!pbcf->running)break;

	}

	pthread_exit((void *)0);
}

static int ngx_http_lua_shdict_expire(ngx_http_lua_shdict_ctx_t *ctx, ngx_uint_t n)
{
    ngx_time_t                  *tp;
    uint64_t                     now;
    ngx_queue_t                 *q;
    int64_t                      ms;
    ngx_rbtree_node_t           *node;
    ngx_http_lua_shdict_node_t  *sd;
    int                          freed = 0;

    tp = ngx_timeofday();

    now = (uint64_t) tp->sec * 1000 + tp->msec;

    /*
     * n == 1 deletes one or two expired entries
     * n == 0 deletes oldest entry by force
     *        and one or two zero rate entries
     */

    while (n < 3) {

        if (ngx_queue_empty(&ctx->sh->lru_queue)) {
            return freed;
        }

        q = ngx_queue_last(&ctx->sh->lru_queue);

        sd = ngx_queue_data(q, ngx_http_lua_shdict_node_t, queue);

        if (n++ != 0) {

            if (sd->expires == 0) {
                return freed;
            }

            ms = sd->expires - now;
            if (ms > 0) {
                return freed;
            }
        }

        ngx_queue_remove(q);

        node = (ngx_rbtree_node_t *)
                   ((u_char *) sd - offsetof(ngx_rbtree_node_t, color));

        ngx_rbtree_delete(&ctx->sh->rbtree, node);

        ngx_slab_free_locked(ctx->shpool, node);

        freed++;
    }

    return freed;
}

static int ngx_proc_luashm_backup_recover(ngx_http_lua_shdict_ctx_t *ctx,ngx_http_lua_shdict_node_ext_t *extsd){
    ngx_http_lua_shdict_node_t  *sd;
	ngx_rbtree_node_t           *node;
	int                          i, n;
	uint32_t                     hash;
	int32_t                      user_flags = 0;
	u_char                      *p,*key,*value;
	int 						key_len,value_len;
	int  						value_type;
	double						*num_value;

	key = extsd->data;
	key_len=extsd->key_len;
	value = extsd->data+key_len;
	value_len=extsd->value_len;
	value_type=extsd->value_type;
	user_flags=extsd->user_flags;

	switch (extsd->value_type) {
	case LUA_TSTRING:
		//dd("key:%s, key_len:%d,value:%s,value_len:%d,exptime:%lu",key,key_len,value,value_len,extsd->expires);
		break;
	case LUA_TNUMBER:
		num_value=(double*)value;
		//dd("key:%s, key_len:%d,value:%f,value_len:%d,exptime:%lu",key,key_len,*num_value,value_len,extsd->expires);
		break;
	case LUA_TBOOLEAN:
		//dd("key:%s, key_len:%d,value:%c,value_len:%d,exptime:%lu",key,key_len,value[0],value_len,extsd->expires);
		break;
	}

	hash = ngx_crc32_short(key, key_len);
	    /* rc == NGX_DECLINED or value size unmatch */

    //dd("lua shared dict set: creating a new entry");

    n = offsetof(ngx_rbtree_node_t, color)
        + offsetof(ngx_http_lua_shdict_node_t, data)
        + key_len
        + value_len;

	ngx_shmtx_lock(&ctx->shpool->mutex);

    node = ngx_slab_alloc_locked(ctx->shpool, n);

    if (node == NULL) {

        dd("lua shared dict set: overriding non-expired items "
                       "due to memory shortage for entry \"%s\"", key);

        for (i = 0; i < 30; i++) {
            if (ngx_http_lua_shdict_expire(ctx, 0) == 0) {
                break;
            }

            node = ngx_slab_alloc_locked(ctx->shpool, n);
            if (node != NULL) {
                goto allocated;
            }
        }

        ngx_shmtx_unlock(&ctx->shpool->mutex);
		ngx_log_error(NGX_LOG_EMERG, ctx->log, 0, "no memory");
        return NGX_ERROR;
    }

allocated:

    sd = (ngx_http_lua_shdict_node_t *) &node->color;

    node->key = hash;
    sd->key_len = (u_short) key_len;

    sd->expires = extsd->expires;

    sd->user_flags = user_flags;
    sd->value_len = (uint32_t) value_len;
    //dd("setting value type to %d", value_type);
    sd->value_type = (uint8_t) value_type;
    p = ngx_copy(sd->data, key, key_len);
    ngx_memcpy(p, value, value_len);
    ngx_rbtree_insert(&ctx->sh->rbtree, node);
    ngx_queue_insert_head(&ctx->sh->lru_queue, &sd->queue);
    ngx_shmtx_unlock(&ctx->shpool->mutex);

    return NGX_OK;
}

static int ngx_proc_luashm_backup_backup(ngx_cycle_t *cycle,ngx_shm_zone_t *zone)
{
	ngx_proc_luashm_backup_conf_t  	*pbcf;
    ngx_queue_t                 	*q, *prev;
    ngx_http_lua_shdict_node_t  	*sd;
	ngx_http_lua_shdict_node_ext_t 	*extsd;
    ngx_http_lua_shdict_ctx_t   	*ctx;
    ngx_time_t                  	*tp;
    int                          	total = 0;
    uint64_t                     	now;
	uint32_t						content_size;
	size_t                       	len;
	ngx_buf_t 						*buff;
	double							*num_value;
	ngx_file_t 		 				file;
	u_char							*shm_file_path;

	pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_luashm_backup_module);


    ctx = zone->data;
	buff = ngx_create_temp_buf(pbcf->pool, zone->shm.size+65536);
	if (buff==NULL) {
		ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno," calloc mem failed:%ld",zone->shm.size+65536);
		return NGX_ERROR;
	}
	buff->last += sizeof(uint32_t);

	tp = ngx_timeofday();

    now = (uint64_t) tp->sec * 1000 + tp->msec;

    ngx_shmtx_lock(&ctx->shpool->mutex);

    if (ngx_queue_empty(&ctx->sh->lru_queue)) {
        ngx_shmtx_unlock(&ctx->shpool->mutex);
        return 1;
    }

    /* first run through: get total number of elements we need to allocate */

    /* second run through: add keys to table */

    total = 0;
    q = ngx_queue_last(&ctx->sh->lru_queue);

    while (q != ngx_queue_sentinel(&ctx->sh->lru_queue)) {
        prev = ngx_queue_prev(q);

        sd = ngx_queue_data(q, ngx_http_lua_shdict_node_t, queue);

        if (sd->expires == 0 || ((int64_t)(sd->expires - now)) > 60000) {
            //lua_pushlstring(L, (char *) sd->data, sd->key_len);
            //lua_rawseti(L, -2, ++total);
			extsd = (ngx_http_lua_shdict_node_ext_t*)buff->last;
			extsd->key_len=sd->key_len;
			extsd->expires=sd->expires;
			extsd->value_type=sd->value_type;
			extsd->user_flags=sd->user_flags;

			len = sd->key_len+sd->value_len;
			ngx_memcpy(extsd->data,sd->data,len);
			buff->last += sizeof(ngx_http_lua_shdict_node_ext_t)+len;

			switch (sd->value_type) {
			case LUA_TSTRING:
				extsd->value_len=sd->value_len;
				break;
			case LUA_TNUMBER:
				extsd->value_len=sizeof(double);
				num_value = (double*)extsd->data+extsd->key_len;
				//ngx_memcpy(num_value, extsd->data+extsd->key_len, extsd->value_len);
				break;
			case LUA_TBOOLEAN:
				extsd->value_len=sizeof(u_char);
				break;
			}
			//dd("key:%s, key_len:%d,value:%f,value_len:%d,expires:%lu,now:%lu",extsd->data,extsd->key_len,*num_value,extsd->value_len,extsd->expires,now);

        }

        q = prev;
    }

    ngx_shmtx_unlock(&ctx->shpool->mutex);

	shm_file_path=ngx_palloc(pbcf->pool,pbcf->path.len + 6+ zone->shm.name.len);
	ngx_memset(shm_file_path,0,pbcf->path.len + 6+ zone->shm.name.len);
	ngx_sprintf(shm_file_path,"%s/%s.dat",pbcf->path.data,zone->shm.name.data);
	content_size = buff->last-buff->pos;
	ngx_log_error(NGX_LOG_INFO,cycle->log,0,"save file:%s, content_size:%l bytes, nowtime:%l", shm_file_path,content_size,now);
	if(content_size > sizeof(uint32_t))
		ngx_memcpy(buff->pos,&content_size,sizeof(uint32_t));
	file.fd = open((char*)shm_file_path, O_TRUNC|O_RDWR, S_IWUSR|S_IWGRP|S_IRGRP|S_IRUSR);
	if (file.fd == NGX_INVALID_FILE) {
		//ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,ngx_open_file_n " \"%s\" failed",filename->data);
		ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,ngx_open_file_n " \"%s\" failed",shm_file_path);
		ngx_pfree(pbcf->pool,buff);
		ngx_pfree(pbcf->pool,shm_file_path);
		return NGX_ERROR;
	}
	file.name.len = pbcf->path.len;
	file.name.data = pbcf->path.data;
	file.offset = 0;
	file.log = cycle->log;
	file.offset=0;
	ngx_write_file(&file,buff->pos,content_size,0);
	ngx_close_file(file.fd);

    /* table is at top of stack */
	ngx_pfree(pbcf->pool,shm_file_path);
	ngx_pfree(pbcf->pool,buff->start);
	ngx_pfree(pbcf->pool,buff);
    return 1;
}

static ngx_int_t ngx_proc_luashm_backup_process_init(ngx_cycle_t *cycle)
{
    ngx_proc_luashm_backup_conf_t  		*pbcf;
	ssize_t							n;
	ngx_uint_t						i;
	ngx_http_lua_shdict_node_ext_t  *extsd;
	ngx_shm_zone_t              	*zone;
	ngx_http_lua_shdict_ctx_t   	*sdctx;
	ngx_file_t 		 				file;
	ngx_str_t  						*name;
	ngx_buf_t 						*buff;
	u_char							*shm_file_path;
	ngx_time_t                  	*tp;
    uint64_t                     	now;
	uint32_t						content_size;

    pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_luashm_backup_module);
	if (pbcf == NULL) {
        return NGX_ERROR;
    }
	pbcf->running=1;

	tp = ngx_timeofday();
    now = (uint64_t) tp->sec * 1000 + tp->msec;

	i = 0;
	for (; i < pbcf->shdict_names->nelts;i++)
	{
		name = ((ngx_str_t*)pbcf->shdict_names->elts)  + i;
		zone = ngx_http_lua_find_zone(name->data,name->len);
		dd("shm.name:%s,shm.name_len:%ld,shm.size:%ld  ", name->data,name->len,zone->shm.size);
		sdctx = zone->data;

		shm_file_path=ngx_palloc(pbcf->pool,pbcf->path.len + 6+ zone->shm.name.len);
		ngx_memset(shm_file_path,0,pbcf->path.len + 6+ zone->shm.name.len);
		ngx_sprintf(shm_file_path,"%s/%s.dat",pbcf->path.data,zone->shm.name.data);

		file.fd = open((char*)shm_file_path, O_RDONLY|O_CREAT, S_IWUSR|S_IWGRP|S_IRGRP|S_IRUSR);
		if (file.fd == NGX_INVALID_FILE) {
			//ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,ngx_open_file_n " \"%s\" failed",filename->data);
			ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,ngx_open_file_n " \"%s\" failed",shm_file_path);
			ngx_pfree(pbcf->pool,shm_file_path);
			return NGX_ERROR;
		}
		file.name.len = pbcf->path.len;
		file.name.data = pbcf->path.data;
		file.offset = 0;
		file.log = cycle->log;
		file.offset=0;

		buff = ngx_create_temp_buf(pbcf->pool, zone->shm.size+65536);
		n = ngx_read_file(&file,buff->pos,buff->end-buff->start,0);
		ngx_close_file(file.fd);
		if(n>0){
			ngx_memcpy(&content_size,buff->pos,sizeof(uint32_t));
			ngx_log_error(NGX_LOG_INFO,cycle->log,0,"read file:%s, content_size:%l bytes, file_size:%z bytes, nowtime:%l", shm_file_path,content_size,n,now);
			buff->last = content_size==n?(buff->last+n):(content_size>n?0:buff->last+content_size);
			buff->pos += sizeof(uint32_t);
			while(buff->last - buff->pos >0){
				extsd=(ngx_http_lua_shdict_node_ext_t*)buff->pos;
				if(extsd->expires == 0 || ((int64_t)(extsd->expires-now) > 60000))
					ngx_proc_luashm_backup_recover(sdctx,extsd);

				buff->pos+=sizeof(ngx_http_lua_shdict_node_ext_t)+extsd->key_len+extsd->value_len;
			}
		}
		ngx_pfree(pbcf->pool,shm_file_path);
		ngx_pfree(pbcf->pool,buff->start);
		ngx_pfree(pbcf->pool,buff);
	}

	pthread_t id;
	int ret;
	ngx_log_error(NGX_LOG_INFO,cycle->log,0,"create thread by luashm_backup");
	ret=pthread_create(&id,NULL,(void *)ngx_proc_luashm_backup_thread_cycle,cycle);
	if(ret!=0){
		perror ("Create pthread error!\n");
		exit (1);
	}

    return NGX_OK;
}

static ngx_int_t ngx_proc_luashm_backup_loop(ngx_cycle_t *cycle)
{
	ngx_log_error(NGX_LOG_INFO,cycle->log,0,"ngx_proc_luashm_backup_loop");
    return NGX_OK;
}

static void ngx_proc_luashm_backup_exit_process(ngx_cycle_t *cycle)
{
    ngx_proc_luashm_backup_conf_t 		*pbcf;
	ngx_shm_zone_t   				*zone;
	ngx_str_t 						*name;
	ngx_uint_t 						i;

    pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_luashm_backup_module);
	if (pbcf == NULL) {
        return;
    }
	pbcf->running = 0;
    //ngx_close_socket(pbcf->fd);

	i = 0;
	for (; i < pbcf->shdict_names->nelts;i++)
	{
		name = ((ngx_str_t*)pbcf->shdict_names->elts)  + i;
		zone = ngx_http_lua_find_zone(name->data,name->len);
		if(zone != NULL){
			ngx_log_error(NGX_LOG_INFO,cycle->log,0,"shm.name:%s,shm.name_len:%l,shm.size:%l ", name->data,name->len,zone->shm.size);
			ngx_proc_luashm_backup_backup(cycle,zone);
		}
	}

	ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "exit luashm_backup %V",&ngx_cached_http_time);
}

