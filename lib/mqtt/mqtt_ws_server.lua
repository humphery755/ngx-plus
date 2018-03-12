local mqtt_util = require "nginx.mqtt"

local wb, err = MQTT.wsserver:new{
    timeout = 240000,  -- in milliseconds
    max_payload_len = 65535,
}

if not wb then
  libmqttkit.log(ngx.ERR, "failed to new websocket: ", err)
  return ngx.exit(444)
end

function MQTT.protocol:auth(username,passwd)
	libmqttkit.log(ngx.DEBUG, self.clientId)
	return true
end

local mqtt_protocol = MQTT.protocol.create(wb)

mqtt_protocol.topicStore = ngx.shared.MQTT_TOPIC_DICT
mqtt_protocol.topicExtStore = ngx.shared.MQTT_TOPIC_EXT_DICT
mqtt_protocol.clientStore = ngx.shared.MQTT_CLIENT_DICT
mqtt_protocol.clientTmpStore = ngx.shared.MQTT_CLIENT_TMP_DICT
mqtt_protocol.clientMsgIdStore = ngx.shared.MQTT_MSG_SEQUENCE_DICT
mqtt_protocol.msgNotifyStore = ngx.shared.MQTT_MSG_STORE_DICT
mqtt_protocol.outstanding = ngx.shared.MQTT_OUTSTANDING_STORE_DICT
mqtt_protocol.topicQueueStore = ngx.shared.MQTT_TOPIC_QUEUE_DICT
mqtt_protocol.last_ping_time = 0

local mqtt_server = MQTT.customsvr:new{
	mqtt_protocol = mqtt_protocol,
}

local function exit(is_ws)
  libmqttkit.log(ngx.ERR, "failed to on_abort: ", is_ws)
  if is_ws == nil then ngx.eof() end
  if mqtt_protocol == nil then ngx.eof() return end
  mqtt_protocol:destroy()
  mqtt_protocol=nil
  wb=nil
  ngx.exit(444)
end
local ok, err = ngx.on_abort(exit)
if err then return end

local function _process_ws_mqtt()
	while mqtt_protocol.connected do

	  local data, typ, err = wb:recv_frame()
	  if wb.fatal then
			libmqttkit.log(ngx.DEBUG, mqtt_protocol.clientId, ", ",err)
		break
	  end

	  if not data then
	  	if(mqtt_protocol.last_ping_time > 0) then
	  		libmqttkit.log(ngx.ERR, mqtt_protocol.clientId, ", doPing Timed out: ",ngx.time()-mqtt_protocol.last_ping_time,"s")
	  		break
	  	end
	  	local err = mqtt_protocol:message_write(MQTT.message.TYPE_PINGREQ, nil,0)
		if err then
		  libmqttkit.log(ngx.ERR, mqtt_protocol.clientId, ", failed to send pong: ", err)
		  break
		end
		mqtt_protocol.last_ping_time = ngx.time()


	  	if(mqtt_protocol.subscribeFlg > 0) then
			local subscribeMapObj=MQTT.get(MQTT.GVAR.AREA_SUBSCRIBEMAP,mqtt_protocol.clientId)
			if subscribeMapObj.ismore==1 then
				subscribeMapObj.semaphore:post()
			end
		end
	  elseif typ == "close" then
	  	libmqttkit.log(ngx.DEBUG, mqtt_protocol.clientId, ", ws recv close")
		break
	  elseif typ == "ping" then
			libmqttkit.log(ngx.DEBUG, mqtt_protocol.clientId, ", ws recv ping")
	  elseif typ == "pong" then
			libmqttkit.log(ngx.DEBUG, "client ponged")
	  elseif typ == "text" then
	  else
		mqtt_protocol:handler(data)
	  end
	  mqtt_protocol.last_ping_time = 0
	end
	mqtt_protocol.eventNotify(mqtt_protocol,MQTT.message.TYPE_DISCONNECT)
end

local function myerror_handler( err )
	libmqttkit.log(ngx.ERR, mqtt_protocol.clientId,", ", err)
end

-- xpcall(_process_ws_mqtt, myerror_handler )
_process_ws_mqtt()
if mqtt_protocol == nil then ngx.eof() end
mqtt_protocol:destroy()
mqtt_protocol=nil
wb=nil