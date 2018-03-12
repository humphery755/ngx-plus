local mqtt_notify = require "mqtt.mqtt_notify_module"

local sock = assert(ngx.req.socket(true))
local readline = sock:receiveuntil("\r\n")

local args = {}

local buffer
local first_line = readline()

local indexOf = ffi.C.lua_utility_indexOf(first_line,"HEAD /notify") --Transfer-Encoding:chunked
if(indexOf == 0 ) then
	ngx.print("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 0\r\n")
	indexOf = ffi.C.lua_utility_indexOf(first_line,"HEAD /notify/"..MQTT.upsName.." HTTP/1.")
	if(indexOf == 0 ) then
		local serverId=MQTT.get(MQTT.GVAR.AREA_DEFAULT,"serverId")
		ngx.print("MQTT-REGIST: "..MQTT.upsName..";"..MQTT.configure.SRV_REGIST_NOTIFY_ADDR..";"..MQTT.configure.SRV_REGIST_BROKER_ADDR..";"..serverId.."\r\n")
	end
	ngx.print("\r\n")
	ngx.flush(true)
	ngx.exit(0)
end

local tmpbuf = nil
local isheader = 1
local clen=0
tmpbuf=readline()
while(tmpbuf ~= nil ) do
	indexOf = ffi.C.lua_utility_indexOf(tmpbuf,"Content-Length:") --Transfer-Encoding:chunked
	if(indexOf == 0 ) then
		clen=tonumber(string.sub(tmpbuf,16,#tmpbuf))
	end

	-- ngx.log(ngx.ERR,tmpbuf)
	if isheader==0 then
		if(buffer==nil) then
			buffer=tmpbuf
		else
			buffer=buffer..tmpbuf
		end
	end

	if isheader and tmpbuf == "" then
		isheader=0
		buffer=sock:receive(clen)
		break
	end
	tmpbuf=readline()
end
local body
if buffer == nil then
	libmqttkit.log(ngx.ERR,"ngx.req.get_body_data() is null")
	body="failured"
	ngx.print("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "..#body.."\r\n\r\n")
	ngx.print(body)
	ngx.flush(true)
	ngx.exit(0)
end


body = mqtt_notify:process(args,buffer)
ngx.print("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "..#body.."\r\n\r\n")
ngx.print(body)
ngx.flush(true)
ngx.exit(0)
-- ngx.eof()

