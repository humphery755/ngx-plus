# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_process_enabled(1);
log_level('info'); # to ensure any log-level can be outputted

repeat_each(1);

plan tests => repeat_each() * (blocks() * 2);

#$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;  # make this env take a default value

$ENV{TEST_NGINX_NGX_HOME} ||= "/root/src/mqtt-nginx-1.12.0";
$ENV{TEST_NGINX_THIRD_PARTY_HOME} ||= "/usr/local/thirdparty";

our $main_config = <<'_EOC_';
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_http_echo_module.so;
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_lua_mqtt_kit_module.so;
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_lua_diversion_module.so;

	
_EOC_

our $http_config = <<'_EOC_';

	diversion_env TEST1;
	lua_package_path '$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.lua;$TEST_NGINX_NGX_HOME/lib/?.lua;$TEST_NGINX_NGX_HOME/?.lua;;';
	lua_package_cpath '$TEST_NGINX_THIRD_PARTY_HOME/luajit/?.so;$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.so;$TEST_NGINX_NGX_HOME/lib/?.so;;';
	upstream database {
		server 127.0.0.1:8888;
	}

	lua_shared_dict DICT_DIVERSION_USER_UPS 2m;
	lua_shared_dict DICT_DIVERSION_SYSCONFIG 1m;

	init_by_lua '
	      require "resty.core"
	      lrucache = require "resty.lrucache"
	      libconf = require "config"
	      libcommon = require "diversion.common"
	      lconfig = libconf:new()
	      lcommon = libcommon:new()
	      policyCache=lrucache.new(10)
	    
	';
	init_worker_by_lua_block {
		local cjson = require "cjson"
		local file = io.open("test.json", "r")
		local strjson = file:read();
		file:close()
		jsonObj=cjson.decode(strjson)
	}
_EOC_

no_shuffle();
#no_diff();
#no_long_string();
run_tests();

__DATA__


=== TEST 2: test reading request body
--- main_config eval: $::main_config
--- http_config eval: $::http_config
--- config
    location /echo_body {
        lua_need_request_body on;
        content_by_lua_block {
		local libdiv = require "ngxext.diversion"
		local cjson = require "cjson"
		
		local dictSysconfig = ngx.shared.DICT_DIVERSION_SYSCONFIG
		ngx.log(ngx.ERR,cjson.encode(jsonObj));
		local buff = lcommon.encode_policy(jsonObj)
		dictSysconfig:set("DIV_RULES",buff)
		local tmp=lcommon.decode_policy(buff)
		ngx.log(ngx.ERR,cjson.encode(tmp));

		local runtimeInfo={ver=1,on_off=1}
		local diversionHost={TEST1="http://127.0.0.1"}
		local rInfoBuff = lcommon.encode_runtimeInfo(runtimeInfo,diversionHost)
		dictSysconfig:set("DIV_runtimeInfo",rInfoBuff)

		rInfoBuff = dictSysconfig:get("DIV_runtimeInfo")
		runtimeInfo,diversionHost = lcommon.decode_runtimeInfo(rInfoBuff)

		local userInfo = lcommon.get_userInfo()
		userInfo.ver = runtimeInfo.ver
		local backend,action = lcommon.exec_policy(userInfo)
		ngx.log(ngx.ERR,"action: ",action,", backend: ",backend);
		
		local backend,action = libdiv.exec_policy(runtimeInfo.ver)
		ngx.log(ngx.ERR,"action: ",action,", backend: ",backend);

		local libipparser = require "diversion.userinfo.ipParser"
		ngx.log(ngx.ERR,"ip: ",libipparser.ip2long("127.0.0.1"));
            ngx.print(ngx.var.request_body)
	    --ngx.log(ngx.ERR,ngx.var.request_body)  X-Forwarded-For  X-Real-IP X-Forwarded-For: 192.168.1.10, 192.168.1.11
        }
    }
--- request eval
"POST /echo_body
hello\x00\x01\x02
world\x03\x04\xff"
--- more_headers
Cookie: aid=1000
Cookie: cid=1117
Cookie: BAIDUID=239EE5485DB0510055467D6E376C2EAF:FG=1; BIDUPSID=239EE5485DB0510055467D6E376C2EAF; PSTM=1489113740; ispeed_lsm=2; BD_UPN=12314353; H_PS_645EC=1ecejjllBSGn8B8s3v5m13o0TWIZRqFSOvjGrHPeEMPQdvlAf%2FwZdeSXI9w; BDORZ=B490B5EBF6F3CD402E515D22BCDA1598; BD_CK_SAM=1; PSINO=6; BDSVRTM=98; H_PS_PSSID=22295_1469_21126_22037_21670_22157
--- response_body eval
"hello\x00\x01\x02
world\x03\x04\xff"

