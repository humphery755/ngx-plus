
#ifndef DISRUPTORC_H
#define DISRUPTORC_H

typedef int (*ngx_disruptor_callback_pt)(void *ctx,void *data,ngx_int_t slots);

typedef struct {
	ngx_atomic_t 				wcursor;
	ngx_atomic_t 				wnext;
	ngx_atomic_t 				rcursor;
	ngx_atomic_t 				rnext;
	ngx_int_t  		slots;
	size_t   		slot_size;
    u_char          *end;           /* end of buffer */
	u_char          start[1];       /* start of buffer */
} ngx_ring_buffer_t;

typedef struct {
    ngx_ring_buffer_t			*ring_buf;
	ngx_int_t  					batch_size;		/* default val 1 */
	void						*ctx;
	ngx_atomic_t 				_runing_threads;
	ngx_int_t					work_threads;
	ngx_disruptor_callback_pt 	on_consumer;
	u_char						runing;
} ngx_disruptor_t;

typedef struct {
	ngx_disruptor_t				*disruptor;
	ngx_int_t					slot;
    pthread_t 					tid;
	ngx_time_t                  *time;
} ngx_disruptor_thread_t;

ngx_disruptor_t *ngx_disruptor_create(ngx_int_t slots,size_t slot_size,ngx_int_t batch_size,ngx_int_t work_threads,ngx_disruptor_callback_pt on_consumer);
ngx_uint_t ngx_disruptor_next(ngx_disruptor_t *disruptor);
int ngx_disruptor_publish(ngx_disruptor_t *disruptor,ngx_uint_t sequence,void *data,size_t len);
int ngx_disruptor_startup(ngx_disruptor_t *disruptor);
int ngx_disruptor_destroy(ngx_disruptor_t *disruptor,ngx_int_t timeout);


#endif //  DISRUPTORC_H
