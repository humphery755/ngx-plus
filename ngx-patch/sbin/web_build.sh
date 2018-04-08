#!/bin/bash
export THIRD_PARTY_HOME=/usr/local/thirdparty
export PCRE_HOME=$THIRD_PARTY_HOME/pcre-8.36
export LUAJIT_HOME=$THIRD_PARTY_HOME/luajit
export LUAJIT_LIB=$LUAJIT_HOME/lib
export LUAJIT_INC=$LUAJIT_HOME/include/luajit-2.1
# --with-debug"-g -O0"
CFLAGS= ./configure --with-debug --with-http_v2_module --with-http_ssl_module --with-openssl=$HOME/src/openssl-1.0.2k --with-stream --with-stream_ssl_module --with-http_realip_module --with-http_degradation_module --with-http_slice_module \
	--add-module=modules/nginx-http-footer-filter-master \
	--with-pcre=/root/src/pcre-8.40 --with-pcre-jit --with-http_stub_status_module \
	--with-cc-opt="-Wno-unused-function"	\
	--add-module=modules/lua-ssl-nginx-module --add-module=modules/ngx_devel_kit --add-module=modules/srcache-nginx-module --add-module=modules/nginx-sticky-module-master \
	--add-module=modules/stream-lua-nginx-module --add-module=modules/stream-echo-nginx-module --add-module=modules/nginx_upstream_check_module --add-module=modules/headers-more-nginx-module \
	--add-dynamic-module=modules/echo-nginx-module \
	--add-module=modules/lua-nginx-module	\
	--add-dynamic-module=modules/ngx_lua_mqtt_kit_module/http \
	--add-dynamic-module=modules/ngx_lua_diversion_module/http \
	--add-module=modules/nginx_ajp_module \
	--add-dynamic-module=modules/stream-socks5-nginx-module

#	--add-module=modules/modsecurity-2.9.1/nginx/modsecurity --add-dynamic-module=modules/ngx_http_dyups_module --add-dynamic-module=modules/ngx_proc_luashm_module/http \

cp Makefile.in Makefile
cp conf/nginx.web.conf conf/nginx.conf
