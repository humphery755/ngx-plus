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
	client_body_temp_path $TEST_NGINX_NGX_HOME/cbtmp;
	proxy_temp_path $TEST_NGINX_NGX_HOME/proxytmp;
	fastcgi_temp_path $TEST_NGINX_NGX_HOME/fctmp;
	uwsgi_temp_path $TEST_NGINX_NGX_HOME/uwsgitmp;
	scgi_temp_path $TEST_NGINX_NGX_HOME/scgitmp;

	lua_package_path '$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.lua;$TEST_NGINX_NGX_HOME/lib/?.lua;$TEST_NGINX_NGX_HOME/?.lua;;';
	lua_package_cpath '$TEST_NGINX_THIRD_PARTY_HOME/luajit/?.so;$TEST_NGINX_THIRD_PARTY_HOME/lualib/?.so;$TEST_NGINX_NGX_HOME/lib/?.so;;';
	upstream database {
		server 127.0.0.1:8888;
	}
	#disable_accept_worker_id 0;
	lua_shared_dict DICT_DIV_sysconf_tmp 10m;
	diversion_env AB1;
	lua_shared_dict DICT_DIVERSION_USER_UPS 2m;
	diversion_sysconf_shm DICT_DIVERSION_SYSCONFIG 100m;
	diversion_numset_shm DICT_DIVERSION_BITVECTOR 2048m;

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
		local libdiv = require "ngxext.diversion"
		local cjson = require "cjson"


local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp

local build_runtimeinfo = function (ver,status,src,to,fid)
	-- body
	local runtimeInfo={}
	runtimeInfo.ver=ver
	runtimeInfo["status"]=status
	runtimeInfo["src"]=src
	runtimeInfo["to"]=to
	runtimeInfo["fid"]=fid
	local diversionHost={}
	if status == 1 then
		local db = lcommon:mysql_open()
		local dbres, err, errcode, sqlstate = db:query("select div_env,host from t_diversion_host", 1)
		lcommon:mysql_close(db)
		if not dbres then
			ngx.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		else
			for i,res in pairs(dbres) do
				diversionHost[res.div_env]=res.host
			end
		end
	end
	local runtimeInfoBuf = lcommon.encode_runtimeInfo(runtimeInfo,diversionHost)
	-- ngx.log(ngx.INFO,"runtimeInfo: ",cjson.encode(runtimeInfo),", diversionHost: ",cjson.encode(diversionHost))
	-- libdiv.update_runtimeinfo(runtimeInfoBuf)
	dictSysconfig:set("DIV_runtimeInfo",runtimeInfoBuf)
end

dbsync = function()
	local db = lcommon:mysql_open()

	local divEnv=libdiv:get_env()
	local src,to;
	local ver=0
	local fid=0
	local dbres, err, errcode, sqlstate = db:query("select a.id,a.status,b.ver,b.src,b.to from t_diversion_flow a inner join t_diversion b on a.div_id=b.id where a.status in(1,3) and b.div_env=\'"..divEnv.."\'", 1)
	if not dbres then
		lcommon:mysql_close(db)
		build_runtimeinfo(0,0,nil,nil,fid);
		ngx.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		return
	end
	if #dbres == 0 then
		build_runtimeinfo(0,0,nil,nil,fid);
		return;
	end

	local flow_id = dbres[1].id
	ver=dbres[1].ver
	src=dbres[1].src
	to=dbres[1].to
	fid=dbres[1].id
	local status=dbres[1].status

	lcommon:mysql_close(db)

	build_runtimeinfo(ver,status,src,to,fid);
end

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
	location ~ ^/admin/action/([-_a-zA-Z0-9/]+) {
	      set $path $1;
	      content_by_lua_file $TEST_NGINX_NGX_HOME/lib/admin/$path.lua;
	}

	location ~ ^/admin/policy/([-_a-zA-Z0-9/]+) {
	      set $path $1;
	      content_by_lua_file $TEST_NGINX_NGX_HOME/lib/diversion/admin/policy/$path.lua;
	}
	location ~ ^/admin/runtime/([-_a-zA-Z0-9/]+) {
	      set $path $1;
	      content_by_lua_file $TEST_NGINX_NGX_HOME/lib/diversion/admin/runtime/$path.lua;
	}
    location /echo_body {
        lua_need_request_body on;
        content_by_lua_block {
		local libdiv = require "ngxext.diversion"
		local cjson = require "cjson"
		local i=1
		for i=1, 4, 1 do
		dbsync()
		
		local runtimeInfo = libdiv.get_runtimeinfo()
		runtimeInfo.ver = runtimeInfo.ver+i
		local envhosts = libdiv.get_envhosts()
		ngx.log(ngx.INFO,"runtimeInfo: ",cjson.encode(runtimeInfo),cjson.encode(envhosts))
		local runtimeInfoBuf = lcommon.encode_runtimeInfo(runtimeInfo,envhosts)

		ngx.req.set_header("Content-Type", "application/octet-stream");
		
		local res = ngx.location.capture("/admin/runtime/set",
			 { method=ngx.HTTP_POST, body=runtimeInfoBuf }
		     )
		
		if res and res.status ~= 200 then
			ngx.log(ngx.INFO,"rutime/set: ",cjson.encode(res))
			ngx.exit(500)
		end

		ngx.req.set_header("Accept", "application/octet-stream");
		local res = ngx.location.capture("/admin/action/get_policy",
			 { args = { fid = runtimeInfo.fid},method=ngx.HTTP_GET}
		     )
		
		if res and res.status ~= 200 then
			ngx.log(ngx.INFO,"action/get_policy: ",cjson.encode(res))
			ngx.exit(500)
		end

		if res then
			ngx.log(ngx.INFO,"get_policy: ",cjson.encode(res))
			local pids = nil
			
			ngx.log(ngx.INFO,cjson.encode(lcommon.decode_policy(res.body)))
			
			
				
				--runtimeInfoBuf = lcommon.encode_runtimeInfo(runtimeInfo,envhosts)
				--libdiv.update_runtimeinfo(runtimeInfoBuf)

				libdiv.update_policy(res.body,runtimeInfo.ver)
				pids = libdiv.get_policyid(runtimeInfo.ver)
				ngx.log(ngx.INFO,cjson.encode(pids))
				if pids then
					
					for i,pid in pairs(pids) do
						local resx = ngx.location.capture("/admin/action/get_numset",
							{ args = { pid = pid },method=ngx.HTTP_GET}
							)
						if resx then
							ngx.log(ngx.INFO,"get_numset: ",cjson.encode(resx))
							local r=libdiv.update_bitvector(resx.body,pid)
							resx.body=nil
							resx=nil
						end
					end
				end
				pids=nil
				libdiv.active_policy(runtimeInfo.ver)
				ngx.log(ngx.INFO,"runtimeInfo: ",cjson.encode(libdiv.get_runtimeinfo()))
				local backend,text,action = libdiv.exec_policy(runtimeInfo.ver)
				ngx.log(ngx.INFO,"action=",action,", backend=",backend,", text=",text)
			
		end
		runtimeInfo = nil
		envhosts=nil
		runtimeInfoBuf=nil

		end

		ngx.print(ngx.var.request_body)
		
		--ngx.log(ngx.ERR,ngx.var.request_body)  X-Forwarded-For  X-Real-IP X-Forwarded-For: 192.168.1.10, 192.168.1.11
        }
    }
--- request eval
"POST /echo_body
hello\x00\x01\x02
world\x03\x04\xff"
--- more_headers
Cookie: aid=100
Cookie: cid=1117
Cookie: BAIDUID=239EE5485DB0510055467D6E376C2EAF:FG=1; BIDUPSID=239EE5485DB0510055467D6E376C2EAF; PSTM=1489113740; ispeed_lsm=2; BD_UPN=12314353; H_PS_645EC=1ecejjllBSGn8B8s3v5m13o0TWIZRqFSOvjGrHPeEMPQdvlAf%2FwZdeSXI9w; BDORZ=B490B5EBF6F3CD402E515D22BCDA1598; BD_CK_SAM=1; PSINO=6; BDSVRTM=98; H_PS_PSSID=22295_1469_21126_22037_21670_22157
--- response_body eval
"hello\x00\x01\x02
world\x03\x04\xff"
--- timeout: 50000ms
