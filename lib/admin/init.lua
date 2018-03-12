local libnew_timer = require "lib.new_timer"
local http = require "resty.http"
local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

if ngx.worker.id() ~= 0 then
	return
end

local hc = http:new()

local check_timer
local monitor_timer

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
			libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		else
			for i,res in pairs(dbres) do
				diversionHost[res.div_env]=res.host
			end
		end
	end
	local runtimeInfoBuf = lcommon.encode_runtimeInfo(runtimeInfo,diversionHost)
	if runtimeInfoBuf==nil then
		return nil
	end
	-- ngx.log(ngx.INFO,"runtimeInfo: ",cjson.encode(runtimeInfo),", diversionHost: ",cjson.encode(diversionHost))
	-- libdiv.update_runtimeinfo(runtimeInfoBuf)
	dictSysconfig:set("DIV_runtimeInfo",runtimeInfoBuf)
end

local dbsync = function()
	local db = lcommon:mysql_open()

	local divEnv=libdiv:get_env()
	local src,to;
	local ver=0
	local fid=0
	local dbres, err, errcode, sqlstate = db:query("select a.id,a.status,b.ver,b.src,b.to from t_diversion_flow a inner join t_diversion b on a.div_id=b.id where a.status in(1,3) and b.div_env=\'"..divEnv.."\'", 1)
	if not dbres then
		lcommon:mysql_close(db)
		--build_runtimeinfo(0,0,nil,nil,fid);
		libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
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

local monitor = function()
	for _,k in pairs(dictSysconfig:get_keys()) do
		if lutility.startswith(k,"alive.servers.") then
			local o=dictSysconfig:get(k);
			if o ~= nil then
				o = cjson.decode(o)
				local host=o["address"]
				local ver=o["ver"]
				local fstatus=o["status"]
				local ok, code, headers, status, body  = hc:request {
					url = "http://"..host.."/stat/"..ngx.time(),
					method = "GET", -- POST or GET
					headers = { ["Content-Type"] = "application/x-www-form-urlencoded",["Accept"] = "application/json" }
				}
				if ok and code == 200 and body ~= nil then
					o=cjson.decode(body)
					o["address"]=host
					o["ver"]=ver
					o["status"]=fstatus
					dictSysconfig:set("monitor.status."..host,cjson.encode(o),10)
				end
			end			
		end    
	end
end

local check = function()
	dbsync()
end

check_timer=libnew_timer:new{
	interval=1,
	callback_fn=check,
}

monitor_timer=libnew_timer:new{
	interval=1,
	callback_fn=monitor,
}

