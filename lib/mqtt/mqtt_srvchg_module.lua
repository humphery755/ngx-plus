
local new_timer = ngx.timer.at
local utility = require "Utility"
local http = require "resty.http"
local libnew_timer = require "new_timer"

local _M = {
    _VERSION = '0.13'
}


local mt = { __index = _M }

local serverstatus
local httpclient

local function gen_chash(upsname,schema)
	local buff = serverstatus:get(upsname)
	if buff == nil then
		MQTT.chash:clean_upstream(upsname)
		return
	end

	local oldver = MQTT.configure.get(MQTT.configure.GVAR.AREA_DEFAULT,"/VER/"..upsname)
	local oldsyntime = MQTT.configure.get(MQTT.configure.GVAR.AREA_DEFAULT,"/syntime/"..upsname)
	local syntime = serverstatus:get("/_syntime_/")
	if syntime~=oldsyntime then
		oldver=nil
	end

	local buff_len=utility.readUint32(buff,1)
	local index=5
	local newver = utility.readUint16(buff,index)
	if oldver~=nil and newver==oldver then
		return
	end
	index=index+2
	local new_ups_name,_len=utility.decode_utf8(buff,index)
	index=index+2+_len

	MQTT.chash:clean_upstream(upsname)
	local srvKeyList={}
	repeat
		local new_seq = utility.readUint16(buff,index)
		table.insert(srvKeyList, tostring(new_seq));
		index=index+2
	until (index >= buff_len)
	MQTT.configure.set(MQTT.configure.GVAR.AREA_DEFAULT,"/VER/"..upsname,newver)
	MQTT.chash:add_server(upsname,srvKeyList)
	MQTT.configure.set(MQTT.configure.GVAR.AREA_DEFAULT,"/syntime/"..upsname,syntime)
	libmqttkit.log(ngx.WARN,"syntime: ",syntime,", ",upsname,": ",utility.tableprint(srvKeyList))
end

local function do_build_chash(ctx)
	gen_chash(ctx.UPSTREAM_INFO)
end



-----------------------------------------------------------------------------------

local function get_srvchg(url,upsName)
	local ok, code, headers, status, body  = httpclient:request {
        url = url.."?upsName="..upsName,
        method = "GET"
    }
    if ok and code==200 then
		return body,headers["syntime"]
	else
		libmqttkit.log(ngx.ERR, "get_srvchg failed, upsName: ",upsName,", code: ",code,", url: ",url)
		return nil
	end
end

local function do_sync(ctx)
	local buf,syntime = get_srvchg(ctx.SRV_STATUS_GET_URL.."/mqtt_srvchg/get",ctx.UPSTREAM_INFO)
	if buf~=nil then
		serverstatus:set("/_syntime_/",syntime)
		serverstatus:set(ctx.UPSTREAM_INFO,buf)
	end
end

function _M.startup(self, limitWId)
	local new_timer=libnew_timer:new{
		interval=3,
		callback_fn=do_build_chash,
		UPSTREAM_INFO=self.SRV_STATUS_UPSTREAM_INFO
	}


	if(ngx.worker.id() == 0) then
		local new_timer1=libnew_timer:new{
			interval=3,
			callback_fn=do_sync,
			UPSTREAM_INFO=self.SRV_STATUS_UPSTREAM_INFO,
			SRV_STATUS_GET_URL=self.SRV_STATUS_GET_URL
		}
	end
end


function _M.new(self, opts)
	httpclient = http:new()
	serverstatus=ngx.shared[opts.SRV_STATUS_SHM_NAME]
    return setmetatable({
        httpclient = httpclient,
		serverstatus = serverstatus,
		SRV_STATUS_UPSTREAM_INFO=opts.SRV_STATUS_UPSTREAM_INFO,
		SRV_STATUS_GET_URL=opts.SRV_STATUS_GET_URL
    }, mt)
end

return _M
