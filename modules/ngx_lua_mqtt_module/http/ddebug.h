
/*
 * Copyright (C) Yichun Zhang (agentzh)
 */


#ifndef _DDEBUG_H_INCLUDED_
#define _DDEBUG_H_INCLUDED_


#include <ngx_config.h>
#include <nginx.h>
#include <ngx_core.h>
#define DDEBUG 0

#if defined(DDEBUG) && (DDEBUG)

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define dd(...) fprintf(stderr, "mqtt *** %s: ", __func__); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)

#   else

#include <stdarg.h>
#include <stdio.h>

#include <stdarg.h>

static void dd(const char *fmt, ...) {
}

#    endif

#else

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define dd(...)

#   else

#include <stdarg.h>

static void dd(const char *fmt, ...) {
}

#   endif

#endif

#if defined(DDEBUG) && (DDEBUG)

#define dd_check_read_event_handler(r)   \
    dd("r->read_event_handler = %s", \
        r->read_event_handler == ngx_http_block_reading ? \
            "ngx_http_block_reading" : \
        r->read_event_handler == ngx_http_test_reading ? \
            "ngx_http_test_reading" : \
        r->read_event_handler == ngx_http_request_empty_handler ? \
            "ngx_http_request_empty_handler" : "UNKNOWN")

#define dd_check_write_event_handler(r)   \
    dd("r->write_event_handler = %s", \
        r->write_event_handler == ngx_http_handler ? \
            "ngx_http_handler" : \
        r->write_event_handler == ngx_http_core_run_phases ? \
            "ngx_http_core_run_phases" : \
        r->write_event_handler == ngx_http_request_empty_handler ? \
            "ngx_http_request_empty_handler" : "UNKNOWN")

#else

#define dd_check_read_event_handler(r)
#define dd_check_write_event_handler(r)

#endif


#if defined(DDEBUG) && (DDEBUG)
#define dd_bin(p,len){																							\
	int i_ddb________;	u_char out_ddb________[(len)*8+1];														\
	for(i_ddb________=0;i_ddb________<len;i_ddb________++){														\
			out_ddb________[i_ddb________*8]=(((p)[i_ddb________]>>7)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+1]=(((p)[i_ddb________]>>6)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+2]=(((p)[i_ddb________]>>5)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+3]=(((p)[i_ddb________]>>4)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+4]=(((p)[i_ddb________]>>3)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+5]=(((p)[i_ddb________]>>2)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+6]=(((p)[i_ddb________]>>1)&0x1)==0x1?'1':'0';						\
			out_ddb________[i_ddb________*8+7]=(((p)[i_ddb________]>>0)&0x1)==0x1?'1':'0';						\
	}	out_ddb________[(len)*8]='\0';																			\
	fprintf(stderr, "mqtt *** %s: %s at %s line %d.\n", __func__,out_ddb________, __FILE__, __LINE__);   }

#else
#define dd_bin(p,len)
#endif


#endif /* _DDEBUG_H_INCLUDED_ */

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
