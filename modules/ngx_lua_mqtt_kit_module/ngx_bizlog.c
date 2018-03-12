#include <ngx_http.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ngx_bizlog.h"
#include "http/ngx_lua_mqtt_kit_module.h"

NLOG_LEVEL g_NlogLevel = NL_NONE;

ngx_open_file_t* g_biz_logger = NULL;
ngx_open_file_t* g_biz_debuger = NULL;

inline unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
} 

NLOG_LEVEL NInt2LogLevel(int logLevel){
	NLOG_LEVEL level;
	if(logLevel <= NL_NONE){
		level = NL_NONE;
	}else if(logLevel >= NL_ALL){
		level = NL_ALL;
	}else{
		level = (NLOG_LEVEL)logLevel;
	}

	return level;
}

const char* NGetFileName(const char* fullname)
{
	const char* pFilename = strrchr((char*)fullname, '/');
	if(pFilename == NULL){
		pFilename = fullname;
	}else{	
		pFilename++;
	}

	return pFilename;	
}

inline void NWriteLog(const char* log, int size){
	if(g_biz_logger!=NULL && g_biz_logger->fd > 0){
		ngx_write_fd(g_biz_logger->fd, (void*)log,size);
	}
}

inline void NWriteDebugLog(const char* log, int size){
	if(g_biz_debuger!=NULL && g_biz_debuger->fd > 0){
		ngx_write_fd(g_biz_debuger->fd, (void*)log,size);
	}
}

char* ngx_http_bizlog_init(ngx_conf_t *cf, ngx_lua_mqtt_kit_svr_conf_t *cscf)
{
	//BlSetLogLevel(BlInt2LogLevel(cscf->log_level));
	//BlSetLogCb(&WriteLog, &WriteDebugLog);

   	g_biz_logger = ngx_conf_open_file(cf->cycle,&cscf->logfile);
	if(g_biz_logger == NULL){
		return NGX_CONF_ERROR;
	}
	
	g_biz_debuger = ngx_conf_open_file(cf->cycle,&cscf->debugfile);
	if(g_biz_debuger == NULL){
		return NGX_CONF_ERROR;
	}
	
	
	
	g_NlogLevel = NInt2LogLevel(cscf->log_level);
	
	//cf->cycle->conf_ctx[ngx_http_bizlog_module.index] = (void***)cscf;


   	return NGX_CONF_OK;
}

 
ngx_int_t  ngx_http_bizlog_init_process(ngx_cycle_t *cycle)
{
	if(g_biz_logger != NULL){
		dup2(g_biz_logger->fd, fileno(stderr));
	}
	if(g_biz_debuger != NULL){
		dup2(g_biz_debuger->fd, fileno(stdout));	
	}
	
	return 0;
}


#define NLOG_TF "%02d-%02d %02d:%02d:%02d "
#define NLOG_BUF_LEN (1024*2)

void NPrint(NLogCb LogCb,const char* LEVEL, const char* funcName, 
			const char* fileName, int line,  const char* format,  ...){
#define buf_rest(buf, p) (buf+NLOG_BUF_LEN-p-1)
	u_char logbuf[NLOG_BUF_LEN];	
	memset(logbuf, 0, NLOG_BUF_LEN);
	u_char* p = logbuf; 
	//����ʾ��־ʱ�䡣
	time_t timep;
	struct tm *ptm, mytm;
	timep = ngx_time();
	ptm = localtime_r(&timep, &mytm); 
	
	p = ngx_snprintf(p, buf_rest(logbuf, p), NLOG_TF "%s %s[%s:%d] ", 
			1+ptm->tm_mon, ptm->tm_mday,  
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec, 
			LEVEL, funcName, fileName, line); 

	va_list   args;
	va_start(args,format); 
	p = ngx_vslprintf(p ,  logbuf+NLOG_BUF_LEN-2, format, args);
	va_end(args); 
	if(buf_rest(logbuf, p) > 0){ 
 		p = ngx_snprintf(p, buf_rest(logbuf, p), "\n");
	}
	logbuf[NLOG_BUF_LEN-1] = 0;
	if(LogCb == NULL){
		fprintf(stderr, "%.*s", (int)(p-logbuf), logbuf);
	}else{
 		LogCb((const char*)logbuf, p-logbuf);
 	}
}

void NCPrint(NLogCb LogCb,const char* LEVEL,const char* format,  ...){
#define buf_rest(buf, p) (buf+NLOG_BUF_LEN-p-1)
	u_char logbuf[NLOG_BUF_LEN];	
	memset(logbuf, 0, NLOG_BUF_LEN);
	u_char* p = logbuf; 
	//����ʾ��־ʱ�䡣
	time_t timep;
	struct tm *ptm, mytm;
	timep = ngx_time();
	ptm = localtime_r(&timep, &mytm); 
	
	p = ngx_snprintf(p, buf_rest(logbuf, p), NLOG_TF "%s ", 
			1+ptm->tm_mon, ptm->tm_mday,  
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec, 
			LEVEL); 

	va_list   args;
	va_start(args,format); 
	p = ngx_vslprintf(p ,  logbuf+NLOG_BUF_LEN-2, format, args);
	va_end(args); 
	if(buf_rest(logbuf, p) > 0){ 
 		p = ngx_snprintf(p, buf_rest(logbuf, p), "\n");
	}
	logbuf[NLOG_BUF_LEN-1] = 0;
	if(LogCb == NULL){
		fprintf(stderr, "%.*s", (int)(p-logbuf), logbuf);
	}else{
 		LogCb((const char*)logbuf, p-logbuf);
 	}
}
 
void NPrintBig(NLogCb LogCb,const char* LEVEL, const char* funcName, 
			const char* fileName, int line,  const char* format,  ...){
#define NLOG_BIGBUF_LEN (1024*32)
#define big_buf_rest(buf, p) (buf+NLOG_BIGBUF_LEN-p-1)
	u_char logbuf[NLOG_BIGBUF_LEN];	
	memset(logbuf, 0, NLOG_BIGBUF_LEN);
	u_char* p = logbuf; 
	//����ʾ��־ʱ�䡣
	time_t timep;
	struct tm *ptm, mytm;
	timep = ngx_time();
	ptm = localtime_r(&timep, &mytm); 
	
	p = ngx_snprintf(p, big_buf_rest(logbuf, p), NLOG_TF "%s %s[%s:%d] ", 
			1+ptm->tm_mon, ptm->tm_mday,  
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec, 
			LEVEL, funcName, fileName, line); 

	va_list   args;
	va_start(args,   format); 
	p = ngx_vslprintf(p ,  logbuf+NLOG_BIGBUF_LEN-2, format , args);
	va_end(args); 
	if(big_buf_rest(logbuf, p) > 0){ 
 		p = ngx_snprintf(p, big_buf_rest(logbuf, p), "\n");
	}
	logbuf[NLOG_BIGBUF_LEN-1] = 0;
	if(LogCb == NULL){
		fprintf(stderr, "%.*s", (int)(p-logbuf), logbuf);
	}else{
 		LogCb((const char*)logbuf, p-logbuf);
 	}
}

void NCPrintBig(NLogCb LogCb,const char* LEVEL, const char* format,  ...){
#define NLOG_BIGBUF_LEN (1024*32)
#define big_buf_rest(buf, p) (buf+NLOG_BIGBUF_LEN-p-1)
	u_char logbuf[NLOG_BIGBUF_LEN];	
	memset(logbuf, 0, NLOG_BIGBUF_LEN);
	u_char* p = logbuf; 
	//����ʾ��־ʱ�䡣
	time_t timep;
	struct tm *ptm, mytm;
	timep = ngx_time();
	ptm = localtime_r(&timep, &mytm); 
	
	p = ngx_snprintf(p, big_buf_rest(logbuf, p), NLOG_TF "%s ", 
			1+ptm->tm_mon, ptm->tm_mday,  
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec, 
			LEVEL); 

	va_list   args;
	va_start(args,   format); 
	p = ngx_vslprintf(p ,  logbuf+NLOG_BIGBUF_LEN-2, format , args);
	va_end(args); 
	if(big_buf_rest(logbuf, p) > 0){ 
 		p = ngx_snprintf(p, big_buf_rest(logbuf, p), "\n");
	}
	logbuf[NLOG_BIGBUF_LEN-1] = 0;
	if(LogCb == NULL){
		fprintf(stderr, "%.*s", (int)(p-logbuf), logbuf);
	}else{
 		LogCb((const char*)logbuf, p-logbuf);
 	}
}