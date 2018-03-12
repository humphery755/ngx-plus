local mqtt_util = require "nginx.mqtt"
local libredis = require "redis3"
local http = require "resty.http"
local cjson = require "cjson"

local _M = {
    _VERSION = '0.13'
}


local mt = { __index = _M }

local function buildUpQueueName(clientId)
	local hash = ngx.crc32_long(clientId)
	local upQueueName = MQTT.configure.MQTT_UP_QUEUE_PREFIX..(hash % MQTT.configure.MQTT_UP_MQ_NUM)
	return upQueueName
end

local function store_pub_msg(mqtt_protocol,msg)
	local db = libredis:new(redisCluster.cache)
	db:set_timeout(30000)
	local key = buildUpQueueName(mqtt_protocol.clientId)
	ngx.log(ngx.DEBUG,mqtt_protocol.clientId,",key:",key)
	local ok, err = db:connect(key)
	if not ok then
		ngx.log(ngx.ERR,mqtt_protocol.clientId,", failed to connect: redis_bakend:" ,key,", ",err)
		db:close()
		local _times=0
		while(_times<3) do
			ngx.sleep(10)
			_times=_times+1
			ok, err = db:connect(key)
			if not ok then
				db:close()
			else
				break
			end
		end
		ngx.log(ngx.ERR,mqtt_protocol.clientId,", try connect redis_bakend:" ,key," by ".._times," times")
		if _times>=3 then
			mqtt_protocol:destroy()
		end
	end

	ok, err = db:rpush(key, msg)
	if not ok then
		ngx.log(ngx.ERR,mqtt_protocol.clientId,", failed to rpush: ", err)
	end
	db:set_keepalive(10000, 5000)
	--db:close()
	return ok, err

end

local function eventNotify(mqtt_protocol,message_type,msg)

	if mqtt_protocol.clientId == nil then
		return
	end

	if message_type == MQTT.message.TYPE_UNSUBSCRIBE then
		local index = 3
		local remaining_len=#msg
		local subTopicKey
		while (index < remaining_len) do
			local topic_len = string.byte(msg, index) * 256 + string.byte(msg, index+1)
			local topic  = string.sub(msg, index+2, index+1+topic_len)
			index = index + topic_len + 2
			mqtt_protocol:storeConsumerOffset(topic,nil)
			ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", unsubTopicKey=",topic)
		end
	elseif message_type == MQTT.message.TYPE_CONNECT then
		local session=mqtt_protocol.clientTmpStore:get(mqtt_protocol.clientId)
		local db = libredis:new(redisCluster.cache)
		db:set_timeout(30000)
		local ok, err = db:connect(mqtt_protocol.clientId)
		db:set(mqtt_protocol.clientId, session)
		db:set_keepalive(10000, 5000)
	end
	-- ngx.log(ngx.DEBUG, "clientid=",mqtt_protocol.clientId,",subscribeFlg=",mqtt_protocol.subscribeFlg)

	if message_type ~= MQTT.message.TYPE_RESERVED then
		return
	end

	local timestamp = ngx.now()*1000 --tostring(ngx.now()*1000)
	local payload = MQTT.Utility.uint64ToBytes(timestamp) --MQTT.protocol.encode_utf8(timestamp)
	payload = payload .. MQTT.protocol.encode_utf8(mqtt_protocol.clientId)
	if msg~=nil then
		payload=payload..msg
	end

	local remaining_len = #payload

	local connflgs = string.char(bit.lshift(message_type, 4))
	local message = connflgs .. MQTT.Utility.encode_remaining_length(remaining_len)

    message = message .. payload
	-- ngx.localtime()
	store_pub_msg(mqtt_protocol,message)
	ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", timestamp=",timestamp,",message_type=",message_type)
end

function MQTT.protocol:parse_message_publish(                     -- Internal API
  message_type_flags,  -- byte
  remaining_len,    -- integer
  payload)
	local mqtt_protocol = self
	local indexOf = ffi.C.lua_utility_indexOf(mqtt_protocol.clientId,"P/")
	if(indexOf ~= 0 ) then
		ngx.log(ngx.ERR,"User:",mqtt_protocol.clientId," Not authorized!")
		mqtt_protocol:disconnect()
		return
	end

	local topic,topic_len  = MQTT.protocol.decode_utf8(payload, 1)

	local _ver = libutility.uint16ToBytes(1)
	local timestamp = (ngx.now()+120)*1000
	local _timestamp = MQTT.Utility.uint64ToBytes(timestamp)
	--MQTT.protocol.encode_utf8(timestamp)  data = ffi . cast ( "const unsigned char *" , data )
	-- local tmp = ffi.C.utility_lua_read_int64(_payload);
	--ngx.log(ngx.DEBUG, "timestamp=",tmp) --MQTT.Utility.readUint64(ffi.string(_payload),1))
	local tId = ""

	_payload = _ver .. _timestamp .. MQTT.protocol.encode_utf8(tId) .. MQTT.protocol.encode_utf8(mqtt_protocol.clientId)
	_payload = _payload..payload
	local remaining_len = #_payload

	local message = string.char(message_type_flags) -- string.char(MQTT.Utility.shift_left(message_type_flags, 4))

    repeat
      local digit = remaining_len % 128
      remaining_len = math.floor(remaining_len / 128)
      if (remaining_len > 0) then digit = digit + 128 end -- continuation bit
      message = message .. string.char(digit)
    until remaining_len == 0

    message = message .. _payload
    -- TODO: XXX
	local ok, err = store_pub_msg(mqtt_protocol,message)
	ngx.log(ngx.INFO, mqtt_protocol.clientId,", timestamp=",timestamp,", topic=",topic)
end

local function _multi_hget4bl(clientId,topic,min,pagesize)
	local tdb = libredis:new(redisCluster.cache)
	tdb:set_timeout(30000)
	local ok, err = tdb:connect(topic,1)
	if not ok then
		ngx.log(ngx.ERR,clientId,", failed to connect: ", topic,":redis_bakend, ",err)
		tdb:close()
		while(true) do
			ngx.sleep(10)
			ok, err = tdb:connect(topic, 1)
			if not ok then
				ngx.log(ngx.ERR,clientId,",failed to connect: ", topic,":redis_bakend, ",err)
				tdb:close()
			else
				break
			end
		end
	end

	--local vals, err = tdb:zrangebyscore(topic,min,max)
	local vals=tdb:eval("return mqtt_zrangebyscore(KEYS[1],ARGV[1],ARGV[2],ARGV[3],ARGV[4])",1,topic,min,pagesize,(ngx.now()*1000),MQTT.configure.MQTT_TOPIC_SUB_MAX_RANGE)
	max=vals[2]
	err=vals[3]
	vals=vals[1]
	if err~=nil then
        tdb:close()
        local msg=clientId..", failed to mqtt_zrangebyscore("..topic..","..min..","..pagesize..","..MQTT.configure.MQTT_TOPIC_SUB_MAX_RANGE.."): "..err
        error(msg)
    end
	tdb:set_keepalive(10000, 5000)
	ngx.log(ngx.DEBUG,clientId,", mqtt_zrangebyscore(",topic,",",min,",",pagesize,"): ",max)
	return vals,max
end

local function processQueue(buffer,mqtt_protocol,s_qos)
	-- ngx.log(ngx.DEBUG, "message_type_flags:",message_type_flags,", buffer:",buffer)
	local message_type_flags = string.byte(buffer, 1)
	local qos = (bit.rshift(bit.band(message_type_flags,0x0006) , 1))
	local index = 1
	local message_type = bit.rshift(message_type_flags, 4)
	local remaining_len = 0
	local multiplier = 1
	repeat
	  index = index + 1
	  local digit = string.byte(buffer, index)
	  if(digit == nil)then return end
	  remaining_len = remaining_len + ((digit % 128) * multiplier)
	  multiplier = multiplier * 128
	until digit < 128
	--ngx.log(ngx.DEBUG, "message_type_flags:",message_type_flags,", remaining_len:",remaining_len,", buffer_len:",#buffer)
	if (#buffer < remaining_len) then
		return
	end
	local payload = string.sub(buffer, index + 1, index + remaining_len)
	index = 1
	if (message_type == MQTT.message.TYPE_PUBLISH) then
		local ver = libutility.readUint16(payload,index)
		index = index+2
		local timestamp = MQTT.Utility.readUint64(payload,index)
		index = index+8
		local tId,tId_len  = MQTT.protocol.decode_utf8(payload, index)
		index = index+tId_len+2
		local srcCid,srcCid_len  = MQTT.protocol.decode_utf8(payload, index)
		index = index+srcCid_len+2
		local topic,topic_len  = MQTT.protocol.decode_utf8(payload, index)
		local offset=0
		local recvTime=ngx.now()*1000
		if timestamp>0 and timestamp < recvTime then
			ngx.log(ngx.WARN, tId,", ",mqtt_protocol.clientId,", topic=",topic,", expire:",timestamp,"ms < current:",recvTime,"ms")
			return nil,1
		end

		if (qos > 0) then
			index = index+topic_len+2
			local pMsgId = string.byte(payload, index) * 256 + string.byte(payload, index + 1)
			index = index+2
			local cMsgId = pMsgId
			if s_qos > 0 then
				cMsgId = mqtt_protocol.clientMsgIdStore:incr("SEQ/"..mqtt_protocol.clientId,1)
				if cMsgId == nil then
					cMsgId=1
					mqtt_protocol.clientMsgIdStore:set("SEQ/"..mqtt_protocol.clientId,1)
				elseif cMsgId > 65535 then
					cMsgId=1
					mqtt_protocol.clientMsgIdStore:set("SEQ/"..mqtt_protocol.clientId,1)
				end
			end

			offset = MQTT.Utility.readUint32(payload, index)


			payload = string.sub(payload, index, remaining_len)
			--ngx.log(ngx.DEBUG, "index=",index, ", payload_len=",remaining_len, ", offset=",#strOffset, ", payload=",payload)

			local _payload = MQTT.protocol.encode_utf8(topic)
			_payload = _payload.. string.char(math.floor(cMsgId / 256)) .. string.char(cMsgId % 256)

			payload = _payload .. payload
			mqtt_protocol:message_write(MQTT.message.TYPE_PUBLISH, payload,qos)

			if s_qos > 0 then
				local timestamp =  math.ceil(ngx.now()) --tostring(ngx.now()*1000)
				local cacheVal = string.char(math.floor(pMsgId / 256)) .. string.char(pMsgId % 256) .. MQTT.Utility.uint32ToBytes(offset)
				cacheVal = cacheVal .. MQTT.Utility.uint32ToBytes(timestamp) .. MQTT.protocol.encode_utf8(srcCid) .. MQTT.protocol.encode_utf8(topic)
				mqtt_protocol:setOutstanding(topic,cMsgId)
				mqtt_protocol:setOutstanding(cMsgId,cacheVal)
			end
			ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", topic=",topic,", sMsgId=",cMsgId,", pMsgId=",pMsgId,", offset=",offset,", payload_len=",#payload,", recvTime:",recvTime)
			return cMsgId
		end
		offset = MQTT.Utility.readUint32(payload, index+topic_len+2)
		payload = string.sub(payload, index, index+remaining_len)
		mqtt_protocol:message_write(MQTT.message.TYPE_PUBLISH, payload,qos)
		ngx.log(ngx.WARN, tId,", ", mqtt_protocol.clientId,", topic=",topic,", qos =",qos ,", offset=",offset,", payload_len=",#payload,"[",remaining_len,"], recvTime:",recvTime)
	elseif (message_type == MQTT.message.TYPE_RESERVED) then

		payload = string.sub(payload, index, remaining_len)
		mqtt_protocol:parse_message_reserved(message_type,remaining_len,payload)
	else
		mqtt_protocol.websock:send_binary(buffer)
		ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", message_type=",message_type)
	end

end

local function process_subscribe(mqtt_protocol,topic,minoffset,pagesize,subTopicKey,s_qos)
	local vals
	local tmp_vals={}

	--if(indexOf == 0 ) then
		--vals = cache:get(cacheKey)
	--end
	local maxoffset=minoffset+pagesize
	for i=minoffset,maxoffset,1 do
		local cacheKey=topic.."/"..(i)
		vals = mqtt_protocol.msgNotifyStore:get(cacheKey)
		vals=nil
		if vals ~= nil then
			table.insert(tmp_vals,vals)
			table.insert(tmp_vals,i)
			ngx.log(ngx.DEBUG,mqtt_protocol.clientId,", cacheKey:",cacheKey, ", value_size:",#vals)
		else
			ngx.log(ngx.WARN,mqtt_protocol.clientId,", cache losed! mqtt_zrangebyscore(",topic,",",minoffset,",",pagesize,")")
			tmp_vals,maxoffset = _multi_hget4bl(mqtt_protocol.clientId,topic,minoffset,pagesize)
			break
		end
	end

	vals=tmp_vals
	local existed_expire=0

	if vals then

		--for key,buffer in pairs(vals) do
		local vlen=#vals
		local consumerOffset=-1
		ngx.log(ngx.DEBUG,mqtt_protocol.clientId,", topic:",topic, ", vlen:",vlen)
		for i=1, vlen, 2 do
			local buffer = vals[i]
			local offset=tonumber(vals[i+1])
			-- ngx.log(ngx.DEBUG,mqtt_protocol.clientId,", topic:",topic, ", key:",key)
			local cMsgId,_existed_expire=processQueue(buffer,mqtt_protocol,s_qos,offset)
			if offset>consumerOffset then
				consumerOffset=offset
			end
			if 1==_existed_expire then
				existed_expire=_existed_expire
			end
			if cMsgId ~= nil then
				return consumerOffset,cMsgId,existed_expire
			end
		end
	end
	return maxoffset,nil,existed_expire
end

function MQTT.protocol:mqtt_process_subscribe(					   -- Override Internal API
	_subsMap)  -- table
	local mqtt_protocol = self

	local newoffset
	local shdictName
	local offsetTable
	--local offsetTableSize;
	local subTopicKey
	local oldoffset
	local subsMap = _subsMap
	local ismore=0

	if not subsMap then
		subsMap = mqtt_protocol.subsMap
	end
	--ngx.log(ngx.DEBUG, mqtt_protocol.clientId, ", subsMap:",MQTT.Utility.tableprint(mqtt_protocol.subsMap))
	for sub_topic, subs in pairs(subsMap) do
		local minoffset = subs.min
		local maxoffset = subs.max
		if maxoffset then
			if maxoffset > MQTT.INT_MAX_RING then maxoffset=MQTT.INT_MAX_RING end
			process_subscribe(mqtt_protocol,sub_topic,minoffset,maxoffset,sub_topic,subs.qos)
			subs.max = nil
		else

			local indexOf = ffi.C.lua_utility_indexOf(sub_topic,"G/E/")

			if ffi.C.ngx_mqtt_topic_is_expression(sub_topic) then
				if(indexOf ~= 0 ) then
					shdictName="MQTT_TOPIC_DICT"
				else
					shdictName="MQTT_TOPIC_EXT_DICT"
				end
				offsetTable = mqtt_util.shdict_get2mqtt(shdictName,sub_topic)
			else
				offsetTable={}
				local topicStore
				if(indexOf ~= 0 ) then
					topicStore=ngx.shared.MQTT_TOPIC_DICT
				else
					topicStore=ngx.shared.MQTT_TOPIC_EXT_DICT
				end
				local _offset=topicStore:get(sub_topic)
				if _offset~=nil then
					offsetTable[sub_topic]=_offset
				end
			end

			-- ngx.log(ngx.DEBUG, mqtt_protocol.clientId, ", sub_topic:",sub_topic, ", offsetTable:",MQTT.Utility.tableprint(offsetTable))
			for topic, newoffset in pairs(offsetTable) do
				oldoffset=mqtt_protocol:getConsumerOffset(topic)
				if(oldoffset == nil ) then
					if minoffset <= -1 then
						oldoffset=newoffset+minoffset
					else
						oldoffset=minoffset
					end
					mqtt_protocol:storeConsumerOffset(topic,oldoffset)
				end
				if subs.qos > 0 then
					local sMsgId = mqtt_protocol:getOutstanding(topic)
					if sMsgId ~= nil then
						local outstanding = self:getOutstanding(sMsgId)
						local pubMsgId  = string.byte(outstanding, 1) * 256 + string.byte(outstanding, 2)
						local index = 3
						local _offset = MQTT.Utility.readUint32(outstanding, index)
						index = index+4
						local timestamp = MQTT.Utility.readUint32(outstanding, index)
						index = index+4
						local times_now = math.ceil(ngx.now())
						if times_now-timestamp < 60 then -- seconds
							oldoffset = newoffset -- continue
						else
							oldoffset = _offset-1
							local targetCid,targetCid_len  = MQTT.protocol.decode_utf8(outstanding, index)
							index = index+targetCid_len+2
							ngx.log(ngx.WARN, mqtt_protocol.clientId,", Timeout to resend: ",times_now-timestamp, "ms, pubMsgId:",pubMsgId,", topic:",topic, ", oldoffset:",_offset)
						end
					end
				end
				if oldoffset>=MQTT.INT_MAX_RING then
					oldoffset=0
					mqtt_protocol:storeConsumerOffset(topic,oldoffset)
				elseif((oldoffset>MQTT.INT_MAX_RING2) and newoffset<MQTT.INT_MAX_RING1) then
					newoffset=MQTT.INT_MAX_RING
				end
				--ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", topic:",topic, ", subsMap:",MQTT.Utility.tableprint(subsMap),", newoffset:",newoffset,", oldoffset:",oldoffset)
				if(newoffset>oldoffset) then

					if(newoffset > oldoffset+MQTT.configure.MQTT_TOPIC_SUB_MAX_RANGE) then
						ngx.log(ngx.ERR, mqtt_protocol.clientId,", topic:",topic,", invalid range specified! (oldoffset[",oldoffset,"],newoffset[",newoffset,"])>MQTT_TOPIC_SUB_MAX_RANGE and set oldoffset=newoffset-MQTT_TOPIC_SUB_MAX_RANGE[",newoffset-MQTT.configure.MQTT_TOPIC_SUB_MAX_RANGE,"]")
						oldoffset = newoffset-MQTT.configure.MQTT_TOPIC_SUB_MAX_RANGE
					end
					-- local req_args={}
 					local pagesize = newoffset-oldoffset
 					local existed_expire=0
 					::expire_process::
					if existed_expire==1 then
						if pagesize == newoffset-oldoffset then
							goto continue
						end
						pagesize = newoffset-oldoffset
					elseif pagesize>MQTT.configure.MQTT_TOPIC_SUB_PAGESIZE then
						pagesize=MQTT.configure.MQTT_TOPIC_SUB_PAGESIZE
						ismore=1
					end
					ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", topic:",topic, ", minoffset:",oldoffset+1,", newoffset:",oldoffset+pagesize,", maxoffset:",newoffset,", pagesize:",pagesize,", sub qos:",subs.qos)
					local consumerOffset,cMsgId,_existed_expire=process_subscribe(mqtt_protocol,topic,oldoffset+1,pagesize,topic,subs.qos)
					if cMsgId == nil then
						if consumerOffset==-1 then
							goto continue
						end
						if consumerOffset>= MQTT.INT_MAX_RING then
							consumerOffset=0
						end
						mqtt_protocol:storeConsumerOffset(topic,consumerOffset)
						if consumerOffset>=newoffset then
							goto continue
						end
						oldoffset=consumerOffset
					end
					if 1==_existed_expire then
						existed_expire=_existed_expire
						goto expire_process
					end
				end
				::continue::
			end
		end
	end

	local subscribeMapObj=MQTT.get(MQTT.GVAR.AREA_SUBSCRIBEMAP,self.clientId)
	if ismore==1 then
		subscribeMapObj.ismore=1
		subscribeMapObj.semaphore:post()
		ngx.log(ngx.DEBUG, mqtt_protocol.clientId,", semaphore:post()")
	else
		subscribeMapObj.ismore=0
	end
end

function MQTT.protocol:parse_message_puback(                      -- Override Internal API
  message_type_flags,  -- byte
  remaining_len,    -- integer
  message)             -- string

	if (remaining_len < 2) then
		error("MQTT.protocol:parse_message_puback(): Invalid remaining len: " .. remaining_len)
	end

	local pubMsgId  = string.byte(message, 1) * 256 + string.byte(message, 2)
	local outstanding = self:getOutstanding(pubMsgId)
	if outstanding == nil then
		ngx.log(ngx.WARN, self.clientId,", pMsgId=",pubMsgId,", Outstanding not found")
		return
	end

	local message_id = string.byte(outstanding, 1) * 256 + string.byte(outstanding, 2)
	local index = 3
	local offset = MQTT.Utility.readUint32(outstanding, index)
	index = index+4+4
	local targetCid,targetCid_len  = MQTT.protocol.decode_utf8(outstanding, index)
	index = index+targetCid_len+2
	local topic,topic_len  = MQTT.protocol.decode_utf8(outstanding, index)
	index = index+topic_len+2

	local connflgs = string.char(message_type_flags)
	local payload = connflgs .. MQTT.Utility.encode_remaining_length(remaining_len)
	payload = payload .. string.char(math.floor(message_id / 256)) .. string.char(message_id % 256)
	local timestamp = ngx.now()*1000
	payload = payload .. MQTT.Utility.uint64ToBytes(timestamp) .. MQTT.protocol.encode_utf8(self.clientId).. MQTT.protocol.encode_utf8(targetCid)
	local isEXT = ffi.C.lua_utility_indexOf(topic,"G/E/")
	if isEXT == 0 then
		payload = payload .. string.char(0x01)
	else
		payload = payload .. string.char(0x00)
	end
  	store_pub_msg(self,payload)
  	self:storeConsumerOffset(topic,offset)
	self:setOutstanding(message_id,nil)
	self:setOutstanding(topic,nil)
	ngx.log(ngx.DEBUG, self.clientId,", targetCid=",targetCid,", sMsgId=",message_id,", pMsgId=",pubMsgId,", topic=",topic,", offset=",offset)


	local topicStore
	local indexOf = ffi.C.lua_utility_indexOf(topic,"G/E/")
	if(indexOf ~= 0 ) then
		topicStore=ngx.shared.MQTT_TOPIC_DICT
	else
		topicStore=ngx.shared.MQTT_TOPIC_EXT_DICT
	end
	local newoffset=topicStore:get(topic)

	if newoffset~=nil and newoffset>offset then
		local subscribeMapObj=MQTT.get(MQTT.GVAR.AREA_SUBSCRIBEMAP,self.clientId)
		subscribeMapObj.ismore=1
		subscribeMapObj.semaphore:post()
		ngx.log(ngx.DEBUG, self.clientId,", semaphore:post()")
	end
end

function MQTT.protocol:parse_message_reserved(					   -- Override Internal API
  message_type_flags,  -- byte
  remaining_len,    -- integer
  message)			   -- string

	ngx.log(ngx.DEBUG)

	if (remaining_len < 3) then
	  error("MQTT.protocol:parse_message_reserved() : Invalid remaining len: " .. remaining_len)
	end

	local cmd = string.byte(message, 1)
	if cmd == MQTT.TYPE_RESERVED.KICK then
		local session_id,session_id_len = MQTT.protocol.decode_utf8(message, 2)
		if(self.session_id ~= session_id) then
			self.enableNotify = nil --
			local msg
			msg = string.char(0x01)
			msg = msg .. string.char(MQTT.TYPE_RESERVED.KICK)
			self:message_write(MQTT.message.TYPE_RESERVED, msg,0)
			self:disconnect()
			ngx.log(ngx.DEBUG,"recv MQTT.TYPE_RESERVED.KICK[kick]: clientId=",self.clientId,",session_id=",session_id,",self.session_id=",self.session_id)
		else
			ngx.log(ngx.DEBUG,"recv MQTT.TYPE_RESERVED.KICK[ignore]: clientId=",self.clientId,",session_id=",session_id,",self.session_id=",self.session_id)
		end
	end
end

function MQTT.protocol:auth(username,passwd)
	if MQTT.configure.MQTT_LOGIN_URL == nil then
		return true
	end
	ngx.log(ngx.DEBUG, self.clientId)
	local hc = http:new()
	local ok, code, headers, status, body  = hc:request {
		url = MQTT.configure.MQTT_LOGIN_URL.."?userName="..username.."&clientId="..self.clientId.."&password="..passwd,
		--- proxy = "http://127.0.0.1:8888",
		--- timeout = 3000,
		--- scheme = 'https',
		method = "GET", -- POST or GET
		-- add post content-type and cookie
		headers = { ["Content-Type"] = "application/x-www-form-urlencoded" }
	}
	if ok and code == 200 then
		local Subscribe = {}
		local res = cjson.decode(body)
		if res.code ~= "0" then
			return false,res.desc
		end
		local datas = res.body
		for key,val in pairs(datas) do
			Subscribe.topicFilter = val.TOPIC
			Subscribe.min=-1
			Subscribe.qos=val.QOS
			self:storeSubscribe(Subscribe.topicFilter,Subscribe)
		end
		return true
	else
		ngx.log(ngx.ERR, self.clientId,", ",username, ", ",code,", ",status)
	end
	return false,code
end


function _M.new(self, opts)

    if not opts then
       ngx.log(ngx.ERR, "failed to new")
	   return nil
    end
	local mqtt_protocol=opts.mqtt_protocol
	mqtt_protocol.eventNotify = eventNotify
	mqtt_protocol.subscribeFlg = 0
    return setmetatable({
        mqtt_protocol = opts.mqtt_protocol,
    }, mt)
end

return _M


