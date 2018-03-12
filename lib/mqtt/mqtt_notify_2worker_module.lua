local libnew_timer = require "new_timer"

local _M = {
    _VERSION = '0.13'
}

local mt = { __index = _M }


MQTT.semaphoreCache=lrucache.new(100000)

local WROK_ID = ngx.worker.id()
local process_subscribe_handler_warpper

local function storeSemaphoreCache(cid,topic,offset)
	local subsMap=MQTT.semaphoreCache:get(cid)
	if subsMap==nil then
		subsMap={}
	end
	subsMap[topic]=offset
	MQTT.semaphoreCache:set(cid,subsMap,180)
end

local buf_out = ffi.new("ngx_str_t[?]",1)
local function process_subscribe_handler()

	local topicQueueStore = "MQTT_TOPIC_QUEUE_DICT" --ngx.shared.MQTT_TOPIC_QUEUE_DICT

	local cacheL1={}

	for i = 1,MQTT.configure.MQTT_TOPIC_SUB_PAGESIZE+1000,1 do
		--local buff=topicQueueStore:rpop("WORK-"..WROK_ID)
		ffi.C.ngx_lua_ffi_shdict_rpop(topicQueueStore,"WORK-"..WROK_ID, buf_out)
		if buf_out[0].len == 0 then
			break
		end
		local buff=ffi.string(buf_out[0].data,buf_out[0].len)
		local index = 1
		local offset = MQTT.Utility.readUint64(buff, index)
		index = index+8
		local topic,topic_len  = MQTT.protocol.decode_utf8(buff, index)
		index = index+topic_len+2

		local tmpObj = cacheL1[topic]
		if tmpObj == nil then
			tmpObj = {o=offset,b=buff,i=index}
			cacheL1[topic]=tmpObj
		end

		if tmpObj.o>MQTT.INT_MAX_RING2 and offset<MQTT.INT_MAX_RING1 then
			cacheL1[topic]={o=offset,b=buff,i=index}
		elseif tmpObj.o<offset then
			cacheL1[topic]={o=offset,b=buff,i=index}
		end

	end

	local allSubscribeMap=MQTT.get(MQTT.GVAR.AREA_SUBSCRIBEMAP)
	if allSubscribeMap==nil then
		return
	end

	for topic, obj in pairs(cacheL1) do
		local buff=obj.b
		local offset = obj.o
		local index = obj.i
		local indexOf = ffi.C.lua_utility_indexOf(topic,"P/")
		if(indexOf == 0 ) then
			local tmpClientStore = ngx.shared.MQTT_CLIENT_TMP_DICT
			local sessionBtyes=tmpClientStore:get(topic)
			if(sessionBtyes==nil) then
				libmqttkit.log(ngx.WARN,topic, " offline!")
				goto continue
			else
				index=1
				local wId = string.byte(sessionBtyes,index)
				index=index+1
				local old_connect,oc_len = MQTT.protocol.decode_utf8(sessionBtyes,index)
				index=index+oc_len+2
				local srvId,srv_len = MQTT.protocol.decode_utf8(sessionBtyes,index)
				if WROK_ID ~= wId then
					if(ffi.C.ngx_http_ffi_shdict_lpush("MQTT_TOPIC_QUEUE_DICT","WORK-"..wId,buff,#buff)~=0) then
						libmqttkit.log(ngx.ERR, topic,", ngx_http_ffi_shdict_lpush failured, offset:",offset,", topic:",topic)
						return
					end
					libmqttkit.log(ngx.WARN,topic, " relogin to :"..wId)
					return
				else
					libmqttkit.log(ngx.DEBUG, topic,", wId:",wId,", srvId:",srvId,", offset:",offset,", topic:",topic)
				end
			end
			local obj = allSubscribeMap[topic]
			storeSemaphoreCache(topic,topic,offset)
			local seam=obj.semaphore
			seam:post()
		else
			local offsetTable
			for cId, obj in pairs(allSubscribeMap) do
				local subsMap=obj.subsMap

				if subsMap ~= nil then
					-- ngx.log(ngx.DEBUG, cId,", topic:",topic,", offset:",offset)
					if subsMap[topic]~=nil then
						local seam=obj.semaphore
						storeSemaphoreCache(cId,topic,offset)
						seam:post()
						libmqttkit.log(ngx.DEBUG, cId,", semaphore:post(), offset:",offset,", topic:",topic)
					elseif obj.iswildcard==1 then
						for topic_wildcard, Subscribe in pairs(subsMap) do
							if ffi.C.ngx_mqtt_topic_wildcard_match(topic,#topic,topic_wildcard,#topic_wildcard) then
								local seam=obj.semaphore
								storeSemaphoreCache(cId,topic,offset)
								seam:post()
								libmqttkit.log(ngx.DEBUG, cId,", semaphore:post(), offset:",offset,", topic:",topic)
								break
							end
						end
					end
				else
					libmqttkit.log(ngx.ERR, cId,", subsMap is nil, topic:",topic,", offset:",offset)
				end
			end
		end
		::continue::
	end
end




function _M.startup(self, limitWId)
	--if(ngx.worker.id() == 0) then
	local new_timer1=libnew_timer:new{
		interval=1,
		callback_fn=process_subscribe_handler,
	}
	--end
end

function _M.new(self, opts)
    return setmetatable({

    }, mt)
end

return _M