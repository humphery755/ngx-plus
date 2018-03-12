
local _M = {
    _VERSION = '0.13'
}

local bit = require"bit"

local mt = { __index = _M }

function _M.process(self,args,buffer)

	local clientStore = ngx.shared.MQTT_CLIENT_DICT
	local topicStore = ngx.shared.MQTT_TOPIC_DICT
	if args["ext"]~=nil then
		topicStore = ngx.shared.MQTT_TOPIC_EXT_DICT
	end

	local message_type_flags = string.byte(buffer, 1)
	if message_type_flags==nil then return "failured" end
	local qos = (bit.rshift(bit.band(message_type_flags,0x0006) , 1))

	local index = 1
	local message_type = bit.rshift(message_type_flags, 4)
	local remaining_len = 0
	local multiplier = 1
	local offset

	repeat
	  index = index + 1
	  local digit = string.byte(buffer, index)
	  if(digit == nil)then return end
	  remaining_len = remaining_len + ((digit % 128) * multiplier)
	  multiplier = multiplier * 128
	until digit < 128

	if (#buffer < remaining_len) then
		libmqttkit.log(ngx.ERR, "Request body[",#buffer,"] < remaining_len[",remaining_len,"]")
		return "failured"
	end
	local payload = string.sub(buffer, index + 1, index + remaining_len)

	local workId=0 --string.byte(payload,1)
	index = 1
	offset = MQTT.Utility.readUint64(payload, index)
	index = index+8
	local topic,topic_len  = MQTT.protocol.decode_utf8(payload, index)
	index = index+topic_len

	local indexOf = ffi.C.lua_utility_indexOf(topic,"P/")

	ffi.C.ngx_ffi_mqtt_offset_process("MQTT_MSG_STORE_DICT",buffer,#buffer)
	local notifyData=MQTT.Utility.uint64ToBytes(offset)..MQTT.protocol.encode_utf8(topic)

	if(indexOf == 0 ) then
		local tmpClientStore = ngx.shared.MQTT_CLIENT_TMP_DICT
		local sessionBtyes=tmpClientStore:get(topic)
		if(sessionBtyes==nil) then
			libmqttkit.log(ngx.WARN,topic, " offline!")
		else
			index=1
			workId = string.byte(sessionBtyes,index)
			libmqttkit.log(ngx.DEBUG, "topic:",topic,", message_type:",message_type_flags,", remaining_len:",remaining_len,", offset:",offset,", workId:",workId)
			if(ffi.C.ngx_lua_ffi_shdict_lpush("MQTT_TOPIC_QUEUE_DICT","WORK-"..workId,notifyData,#notifyData)==0) then
				return "success"
			else
				libmqttkit.log(ngx.ERR, "ffi_shdict_lpush(MQTT_TOPIC_QUEUE_DICT",", WORK-",workId,") failure, topic:",topic)
				return "failured"
			end
		end
	else
		local count = ngx.worker.count()-1
		libmqttkit.log(ngx.DEBUG, "topic:",topic,", message_type:",message_type_flags,", remaining_len:",remaining_len,", offset:",offset,", workId:1-",count)
		for workId=1,count,1 do
			if(ffi.C.ngx_lua_ffi_shdict_lpush("MQTT_TOPIC_QUEUE_DICT","WORK-"..workId,notifyData,#notifyData)~=0) then
				libmqttkit.log(ngx.ERR, "ffi_shdict_lpush(MQTT_TOPIC_QUEUE_DICT",", WORK-",workId,") failure, topic:",topic)
				return "failured"
			end
		end
	end
	-- payload = string.sub(buffer, index + 1)
	return "success"
end


return _M



