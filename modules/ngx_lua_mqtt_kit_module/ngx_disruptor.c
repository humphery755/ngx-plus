
#include <ngx_core.h>
#include <ngx_config.h>
#include "ngx_disruptor.h"
#include "ddebug.h"
#include<pthread.h>

ngx_uint_t ngx_disruptor_next(ngx_disruptor_t *disruptor)
{
   ngx_atomic_t 		current;
   ngx_atomic_t 		next;
   ngx_ring_buffer_t   	*ring_buf;

   if(disruptor->runing == '0')return NGX_ERROR;

   ring_buf = disruptor->ring_buf;

   do
   {
	   current = ring_buf->wnext;

	   if(current==ring_buf->rcursor+ring_buf->slots){
			sleep(1);
			ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "apply sequence be blocked",ring_buf->slots);
			if(disruptor->runing)continue;
			break;
		}
	   next = current + 1;

	   if (ngx_atomic_cmp_set(&ring_buf->wnext,current, next))
	   {
	   	   dd("apply sequence by wnext=%zd, next=%zd",current,next);
		   break;
	   }
   }
   while (1);

   return next;
}

int ngx_disruptor_publish(ngx_disruptor_t *disruptor,ngx_uint_t sequence,void *data,size_t len){
   	ngx_atomic_t 		prev;
	u_char 				*p;
	ngx_ring_buffer_t	*ring_buf;

	if(disruptor->runing == '0')return NGX_ERROR;

	ring_buf = disruptor->ring_buf;

	if(len>ring_buf->slot_size)return NGX_ERROR;
	p = ring_buf->start + (sequence & (ring_buf->slots-1)) * ring_buf->slot_size;
	ngx_memcpy(p,data,len);
	do
	{
	   prev = sequence - 1;
	   if (ngx_atomic_cmp_set(&ring_buf->wcursor,prev, sequence))
	   {
	   		dd("publish by wcursor=%zd, next=%zd, ring_buf->pos=%p",prev,sequence,p);
		   break;
	   }
	}
   while (1);
   return NGX_OK;
}

static void ngx_disruptor_check_thread(ngx_disruptor_thread_t *disruptor_thread){
	ngx_time_t                  *tp;
    uint64_t                     now,ms,expires;
	ngx_int_t					i;
	ngx_disruptor_t 			*disruptor;

	disruptor = disruptor_thread[0].disruptor;
	expires = 1000*60;

	do{
		sleep(1000);
		tp = ngx_timeofday();
    	now = (uint64_t) tp->sec * 1000 + tp->msec;

		for(i=0;i<disruptor->work_threads;i++){
			tp = disruptor_thread[i].time;
			ms = (uint64_t) tp->sec * 1000 + tp->msec;
			ms = now - ms;
			if (ms > expires) {

			}
		}



	}while(disruptor->runing);

	pthread_exit((void *)0);
}
static void ngx_disruptor_consumer_thread(ngx_disruptor_thread_t *disruptor_thread){
	uint64_t 				current;
   	uint64_t 				next;
	void 					*data;
	ngx_ring_buffer_t   	*ring_buf;
	//ngx_int_t				deviation;
	ngx_disruptor_t 		*disruptor;
	ngx_int_t				offset;

	disruptor=disruptor_thread->disruptor;

	ring_buf = disruptor->ring_buf;
	ngx_atomic_fetch_add(&disruptor->_runing_threads, 1);
	do{
		//check

		// prepare
		do{
			current = ring_buf->rnext;
			if(current==ring_buf->wcursor){
				usleep(100);
				if(disruptor->runing)continue;
				goto gotoend;
			}
 			next = current+disruptor->batch_size;
			if(next>ring_buf->wcursor){
				next=ring_buf->wcursor;
				if((next-current)>(uint64_t)disruptor->batch_size)next = current+(uint64_t)disruptor->batch_size;
			}
			dd("prepare by rnext=%zd, wcursor=%zd,next=%zd",current,ring_buf->wcursor,next);
			if (ngx_atomic_cmp_set(&ring_buf->rnext,current, next))
			{
				disruptor_thread->time = ngx_timeofday();
			   	break;
			}
			usleep(1);
		}while(1);

		// callback
		offset = ring_buf->slot_size*((current+1) & (ring_buf->slots-1));
		data = ring_buf->start+offset;
		dd("callback by offset=%zd, ring_buf->pos=%p",offset,data);
		disruptor->on_consumer(disruptor->ctx,data,next-current);

		// commit
		do{
			if (ngx_atomic_cmp_set(&ring_buf->rcursor,current, next))
			{
			   break;
			}
			usleep(1);
		}while(disruptor->runing);
		dd("commit by current=%zd, next=%zd",current,next);
	}while(disruptor->runing);
gotoend:
	ngx_atomic_fetch_add(&disruptor->_runing_threads, -1);
	pthread_exit((void *)0);
}

ngx_disruptor_t *ngx_disruptor_create(ngx_int_t slots,size_t slot_size,ngx_int_t batch_size,ngx_int_t work_threads,ngx_disruptor_callback_pt on_consumer){
	ngx_disruptor_t 	*disruptor;
	ngx_ring_buffer_t	*ring_buf;
	u_char				*buf;
	size_t				len;
	ngx_int_t 			_slots;

	_slots=1;
	do{
		_slots<<=1;
	}while(_slots<slots);
	if(_slots!=slots){
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "ring_buffer's slots{%d} must be a non-zero power of 2",slots);
		return NULL;
	}

	if(batch_size<1){
		ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "ring_buffer's batch_size{%d} must > 0",batch_size);
		return NULL;
	}
	disruptor = calloc(1,sizeof(ngx_disruptor_t));
	disruptor->work_threads = work_threads;
	disruptor->on_consumer = on_consumer;
	disruptor->_runing_threads = 0;
	disruptor->batch_size = batch_size;

	len = offsetof(ngx_ring_buffer_t,start)+slots*slot_size;
	buf = calloc(1,len);
	ring_buf = (ngx_ring_buffer_t*)buf;
	ring_buf->end = buf+len-1;
	ring_buf->slots = slots;
	ring_buf->slot_size = slot_size;
	ring_buf->rcursor = ring_buf->rnext=ring_buf->wcursor=ring_buf->wnext=0;

	disruptor->ring_buf = ring_buf;
	return disruptor;
}

int ngx_disruptor_startup(ngx_disruptor_t *disruptor){
	ngx_int_t                   i,ret;
	pthread_t 					tid;
	ngx_disruptor_thread_t		*disruptor_threads;

	if(disruptor->_runing_threads > 0)return NGX_ERROR;

	disruptor->runing = '1';
	disruptor_threads = (ngx_disruptor_thread_t*)calloc(disruptor->work_threads,sizeof(ngx_disruptor_thread_t));
	if(disruptor_threads == NULL){
		perror ("calloc error!\n");
		exit (1);
	}
	for(i=0;i<disruptor->work_threads;i++){
		disruptor_threads[i].disruptor=disruptor;
		disruptor_threads[i].slot=i;
		disruptor_threads[i].time=ngx_timeofday();
		ret=pthread_create(&tid,NULL,(void *)ngx_disruptor_consumer_thread,&disruptor_threads[i]);
		if(ret!=0){
			perror ("Create pthread error!\n");
			exit (1);
		}
		disruptor_threads[i].tid=tid;
	}

	ret=pthread_create(&tid,NULL,(void *)ngx_disruptor_check_thread,disruptor_threads);
	if(ret!=0){
		perror ("Create pthread error!\n");
		exit (1);
	}
	return NGX_OK;
}

int ngx_disruptor_destroy(ngx_disruptor_t *disruptor,ngx_int_t timeout){
	ngx_time_t                  *tp;
    int64_t                     star,now;

	if(disruptor == NULL)return NGX_OK;
	disruptor->runing = '0';
	tp = ngx_timeofday();
    star = (int64_t) tp->sec * 1000 + tp->msec;
	do{
		sleep(1);
		tp = ngx_timeofday();
		now = (int64_t) tp->sec * 1000 + tp->msec;
		if((now-star) > timeout)break;
	}while(disruptor->_runing_threads>0);
	free(disruptor->ring_buf);
	free(disruptor);
	return NGX_OK;
}


