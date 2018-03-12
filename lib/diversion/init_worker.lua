local libnew_timer = require "lib.new_timer"
local http = require "resty.http"
local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local stat = function()
	libstat.flush()
end

if ngx.worker.id() ~= 0 then
	libstat.init()
	libnew_timer:new{
		interval=10,
		callback_fn=stat,
	}
	return
end

local register_addr,server_addr=libmqttkit.get_register_addr()
local hc = http:new()

local prever = -1
local get_runtimeinfo_ver = function()	
	local ver = -1
	local fid = 0
	local status = 0
	local runtimeinfo = libdiv.get_runtimeinfo()
	if prever == -1 and runtimeinfo.status >= 2 then
		ver = runtimeinfo.ver
	else
		ver = prever
	end
	fid = runtimeinfo.fid
	status = runtimeinfo.status

	local ok, code, headers, status, body  = hc:request {
		url = register_addr.."/admin/action/runtime/get?ver="..ver.."&fid="..fid.."&status="..status.."&address="..server_addr,
		method = "GET", -- POST or GET
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
	if ok and code == 200 and body ~= nil and headers["content-type"]=="application/octet-stream" then
		--local rtInfo,envhosts = lcommon.decode_runtimeInfo(body)
		-- libmqttkit.log(ngx.INFO,cjson.encode(rtInfo),", ",cjson.encode(envhosts));
		while libdiv.update_runtimeinfo(body) == -5 do
			libmqttkit.log(ngx.ERR,"update_runtimeinfo failure");
			ngx.sleep(1);
		end
		
		local runtimeInfo = libdiv.get_runtimeinfo()
		local envhosts = libdiv.get_envhosts()
		if runtimeInfo == nil then
			libmqttkit.log(ngx.ERR,"runtime info set failure, body is null ");
			return nil;
		end
		if envhosts then
			libmqttkit.log(ngx.INFO,"runtime info set success, body: ",cjson.encode(runtimeInfo),cjson.encode(envhosts));
		else
			libmqttkit.log(ngx.INFO,"runtime info set success, body: ",cjson.encode(runtimeInfo));
		end

		return 1
	elseif code ~= 204 then
		libmqttkit.log(ngx.ERR,code);
	end	
	return nil;
end

local get_and_update_policy_group = function(ver)
	local ok, code, headers, status, body  = hc:request {
		url = register_addr.."/admin/action/get_policy?ver="..ver,
		--- timeout = 3000,
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
	if ok and code == 200 and body ~= nil and headers["content-type"]=="application/octet-stream" then
		libmqttkit.log(ngx.INFO,"decode_policy...")
		--local divRules = lcommon.decode_policy(body)
		--libmqttkit.log(ngx.DEBUG,cjson.encode(divRules))
		libdiv.update_policy(body,ver)
		libmqttkit.log(ngx.INFO,"success of update_policy: ",ver)
		return ver
	end
	return nil;
end

local get_and_update_policy_numset = function(ver,pid,pIdx)
	local datalen=0
	local ok, code, headers, status, body  = hc:request {
		url = register_addr.."/admin/action/get_numset?pid="..pid.."&pIdx="..pIdx,
		--- timeout = 3000,
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/octet-stream" }
	}
	--ngx.log(ngx.INFO,code,cjson.encode(headers))
	if ok and code == 200 and body ~= nil then
		if headers["content-type"]=="application/octet-stream" then
			datalen = lutility.readUint32(body,1);
			libdiv.update_numset(body,pid,ver)
			if datalen <= 40000 then
				return 1
			end
			return 2;
		end
	else
		libmqttkit.log(ngx.ERR,code,cjson.encode(headers))
	end
	return nil;
end

local sync_policy = function()
	local ver = get_runtimeinfo_ver()
	if ver == nil then
		return;
	end
	local runtimeinfo = libdiv.get_runtimeinfo()
	if runtimeinfo.status == 0 then
		prever=0;
		return;
	end
	ver = runtimeinfo.ver
	if ver == prever then
		return;
	end

	local res = get_and_update_policy_group(ver)
	if res == nil then
		libmqttkit.log(ngx.ERR,"failed to update_policy_group")
		return;
	end
	local pids = libdiv.get_policyid(ver)
	if pids == nil then
		libmqttkit.log(ngx.ERR,"failed to get_policyid")
		return;
	end
	libmqttkit.log(ngx.INFO,"update_numset: ",cjson.encode(pids))
	
	for k,pid in pairs(pids) do
		local pIdx = 0;
		while(1)
		do
			local ismore = get_and_update_policy_numset(ver,pid,pIdx)
			if not ismore then
				libmqttkit.log(ngx.ERR,"failed to update_numset: ")
				return;
			elseif ismore == 1 then
				break;
			end
			pIdx = pIdx+1;
		end
	end
	libdiv.active_policy(ver)

	prever = ver;
	
end

check_timer=libnew_timer:new{
	interval=1,
	callback_fn=sync_policy,
}
