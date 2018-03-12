# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_process_enabled(1);
log_level('info'); # to ensure any log-level can be outputted

repeat_each(1);

plan tests => repeat_each() * (blocks() * 1);

#$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;  # make this env take a default value

$ENV{TEST_NGINX_NGX_HOME} ||= "/root/src/mqtt-nginx-1.12.0";
$ENV{TEST_NGINX_THIRD_PARTY_HOME} ||= "/usr/local/thirdparty";

our $main_config = <<'_EOC_';
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_http_echo_module.so;
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_lua_mqtt_kit_module.so;
	load_module $TEST_NGINX_NGX_HOME/lib/ngx_lua_diversion_module.so;

	
_EOC_

our $http_config = <<'_EOC_';
	client_body_temp_path $TEST_NGINX_NGX_HOME/cbtmp;
	proxy_temp_path $TEST_NGINX_NGX_HOME/proxytmp;
	fastcgi_temp_path $TEST_NGINX_NGX_HOME/fctmp;
	uwsgi_temp_path $TEST_NGINX_NGX_HOME/uwsgitmp;
	scgi_temp_path $TEST_NGINX_NGX_HOME/scgitmp;

	diversion_env DEV-AB2;
	lua_package_path '$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.lua;$TEST_NGINX_NGX_HOME/lib/?.lua;$TEST_NGINX_NGX_HOME/?.lua;;';
	lua_package_cpath '$TEST_NGINX_THIRD_PARTY_HOME/luajit/?.so;$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.so;$TEST_NGINX_NGX_HOME/lib/?.so;;';
	upstream database {
		server 127.0.0.1:8888;
	}

	diversion_sysconf_shm DICT_DIVERSION_SYSCONFIG 100m;
	diversion_numset_shm DICT_DIVERSION_BITVECTOR 1536m;
	lua_shared_dict DICT_DIVERSION_USER_UPS 2m;

	init_by_lua_block {
	      require "resty.core"
	      lrucache = require "resty.lrucache"
	      libconf = require "config"
	      libcommon = require "diversion.common"
		  lutility = require "Utility"
		  libhttp = require "resty.http"
		  local cjson = require "cjson"
		  
	      lconfig = libconf:new()
	      lcommon = libcommon:new()
	      policyCache=lrucache.new(10)
	    
ngx.log(ngx.ERR,"test *************************************"..lconfig.DIV_config_admin_url)

get_runtimeinfo = function()
	local hc = libhttp:new()
	local ok, code, headers, status, body  = hc:request {
		url = lconfig.DIV_config_admin_url.."/admin/action/runtime/get",
		--- timeout = 3000,
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
ngx.log(ngx.INFO,"code: ",code,", status: ",status)
	return body;
end

get_and_update_policy_group = function(ver)
	local libdiv = require "ngxext.diversion"
	local hc = libhttp:new()
	local ok, code, headers, status, body  = hc:request {
		url = lconfig.DIV_config_admin_url.."/admin/action/get_policy?ver="..ver,
		--- timeout = 3000,
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
	if ok and code == 200 and body ~= nil then
		if headers["content-type"]=="application/octet-stream" then
			local divRules = lcommon.decode_policy(body)
			ngx.log(ngx.INFO,cjson.encode(divRules))
			libdiv.update_policy(body,ver)
			ngx.log(ngx.INFO,"success of update_policy: ",ver)
			return ver
		end
	end
	return nil;
end

get_and_update_policy_bitvector = function(ver,pid,pIdx)
	local libdiv = require "ngxext.diversion"
	local hc = libhttp:new()
	local datalen=0
	local ok, code, headers, status, body  = hc:request {
		url = lconfig.DIV_config_admin_url.."/admin/action/get_numset?pid="..pid.."&pIdx="..pIdx,
		--- timeout = 3000,
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
	--ngx.log(ngx.INFO,code,cjson.encode(headers))
	if ok and code == 200 and body ~= nil then
		if headers["content-type"]=="application/octet-stream" then
			datalen = lutility.readUint32(body,1);
			libdiv.update_bitvector(body,pid,ver)
			if datalen <= 50000 then
				return 1
			end
			return 2;
		end
	else
		ngx.log(ngx.WARN,code,cjson.encode(headers))
	end
	return nil;
end
	}

	init_worker_by_lua_block {
		
	}

	upstream divs_a_ups {
		sticky name=access_token ;
		server 120.24.82.71:10080;
		keepalive 1000;
	}
	upstream divs_b_ups {
		sticky name=access_token ;
#		server www.uhomecp.com:80;
		server 120.24.82.71:10080;
		keepalive 1000;
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
	location /uhome-cas {
		
		set $backend 'default_ups';
		set $suffix '';
		set $action 0;
		set $plugin '';
		rewrite_by_lua '
			local cjson = require "cjson"
			local libdiv = require "ngxext.diversion"
			local hc = libhttp:new()
			local buffer = get_runtimeinfo()
			ngx.log(ngx.INFO,buffer)
			libdiv.update_runtimeinfo(buffer)
			
			local runtimeinfo = libdiv.get_runtimeinfo()
			ngx.log(ngx.INFO,cjson.encode(runtimeinfo))
			local res = get_and_update_policy_group(runtimeinfo.ver)
			local pids = libdiv.get_policyid(runtimeinfo.ver)
			for k,pid in pairs(pids) do
				local pIdx = 0;
				while(1)
				do
					local ismore = get_and_update_policy_bitvector(runtimeinfo.ver,pid,pIdx)
					if not ismore then
						ngx.log(ngx.ERR,"failed to update_bitvector: ")
						return;
					elseif ismore == 1 then
						break;
					end
					pIdx = pIdx+1;
				end
			end
			libdiv.active_policy(runtimeinfo.ver)
		';
		#
		access_by_lua_file $TEST_NGINX_NGX_HOME/lib/diversion/diversion.lua;
		proxy_pass http://$backend;
		header_filter_by_lua_file $TEST_NGINX_NGX_HOME/lib/diversion/plugins_header_filter.lua; # 
	}

	location /test {
	
		content_by_lua_block{


			ngx.print("success")
		}
	}

--- request eval
"POST /uhome-cas/login?service=http://120.24.82.71:10080/uhomecp-patrol/shiro-cas
lt:LT-45010-CO2BM3tD2en2tfLre3q4pp3fazw4EO
execution:e1s1
_eventId:submit
username:18989991565
password:821018"
--- more_headers
Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
Accept-Encoding:gzip, deflate
Accept-Language:en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4
Cache-Control:no-cache
Connection:keep-alive
Content-Length:113
Content-Type:application/x-www-form-urlencoded
Cookie:__utmt=1; __utma=2546864.2143157114.1490243433.1490335952.1490343414.5; __utmb=2546864.4.10.1490343414; __utmc=2546864; __utmz=2546864.1490243433.1.1.utmcsr=(direct)|utmccn=(direct)|utmcmd=(none); oid=67			
Host:120.24.82.71:10080
Origin:http://120.24.82.71:10080
Pragma:no-cache
Referer:http://120.24.82.71:10080/uhome-cas/login?service=http://120.24.82.71:10080/uhomecp-patrol/shiro-cas
Upgrade-Insecure-Requests:1
User-Agent:Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36
Cookie: aid=452613
Cookie: cid=1117
X-Forwarded-For: 192.168.1.216, 192.168.1.111
--- response_headers
