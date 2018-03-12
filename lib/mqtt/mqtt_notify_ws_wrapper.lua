local mqtt_notify = require "mqtt.mqtt_notify_module"

local method_name = ngx.req.get_method()
if(method_name == "HEAD") then
	if ngx.var.uri==("/notify/"..MQTT.upsName) then
		local serverId=MQTT.get(MQTT.GVAR.AREA_DEFAULT,"serverId")
		ngx.header["MQTT-REGIST"]=MQTT.upsName..";"..MQTT.configure.SRV_REGIST_NOTIFY_ADDR..";"..MQTT.configure.SRV_REGIST_BROKER_ADDR..";"..serverId
		ngx.say("")
		ngx.flush()
	end
	ngx.exit(ngx.HTTP_OK)
end

local args = ngx.req.get_uri_args()
ngx.req.read_body()
local buffer = ngx.req.get_body_data()
if buffer == nil then
	libmqttkit.log(ngx.ERR,"ngx.req.get_body_data() is null")
	ngx.exit(444)
end
ngx.print(mqtt_notify:process(args,buffer))
