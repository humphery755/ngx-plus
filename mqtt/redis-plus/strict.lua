-- eval "return mqtt_zadd(KEYS[1],ARGV[1],ARGV[2])" 1 topic14 ABCDEFGHIJKLMN 3
-- eval "return mqtt_zrangebyscore(KEYS[1],ARGV[1],ARGV[2],ARGV[3],ARGV[4])" 1 G/2/1 108725 108842 1475897703 99999
local dbg=debug
local mt = {}
setmetatable(_G, mt)
--				1  2147483647
--			    -|-
--		   -		 -
--		 - 			   -
--		-				-
--		 -			   -
--		   -		 -
--			   -|-
local INT_MAX_RING=2147480000 --2147483647
local INT_MAX_RING1=INT_MAX_RING/3
local INT_MAX_RING2=INT_MAX_RING1+INT_MAX_RING1

mt.__newindex = function (t, n, v)
 if dbg.getinfo(2) then
	local w = dbg.getinfo(2, "S").what
	if w ~= "main" and w ~= "C" then
		error("Script attempted to create global variable '"..tostring(n).."'", 2)
	end
 end
 rawset(t, n, v)
end
mt.__index = function (t, n)
 if dbg.getinfo(2) and dbg.getinfo(2, "S").what ~= "C" then
	error("Script attempted to access unexisting global variable '"..tostring(n).."'", 2)
 end
 return rawget(t, n)
end
local __ext_global_var_4mqtt={}
rawset(_G, '__ext_global_var_4mqtt', __ext_global_var_4mqtt)

local __f_mqtt_zadd = function (key,data,_index)
	
	local offset=__ext_global_var_4mqtt[key]
	if offset == nil then
		local t = redis.call('ZRANGE',key,-1,-1,'WITHSCORES')
		offset = t[2]
		if offset == nil then
			offset = 0
		else
			offset = tonumber(offset)
		end
	end
	
	if offset >= INT_MAX_RING then
		local t = redis.call('ZREMRANGEBYSCORE',key, 0,INT_MAX_RING1)
		offset=0
	elseif offset == INT_MAX_RING1 then
		local t = redis.call('ZREMRANGEBYSCORE',key, INT_MAX_RING1+1,INT_MAX_RING2)
	elseif offset == INT_MAX_RING2 then
		local t = redis.call('ZREMRANGEBYSCORE',key, INT_MAX_RING2+1,INT_MAX_RING)
	end
	
	offset = offset+1
	
	__ext_global_var_4mqtt[key] = offset
	local payload = data
	local index = tonumber(_index)
	if index < 0 then
		return offset
	elseif index > 0 then
		local b_offset = string.char(bit.band(bit.rshift(offset,24),0xff),
							bit.band(bit.rshift(offset,16),0xff),
							bit.band(bit.rshift(offset,8),0xff),
							bit.band(offset,0xff))
		--error("Script attempted to access unexisting global variable '"..tostring(data).."'", 2)
		payload = string.sub(data, 1, index)
		--error("Script attempted to access unexisting global variable '"..data.."'"..index)
		payload = payload..b_offset..string.sub(data, index+5,#data)
	end
	local rev4zadd = redis.call('zadd',key, offset,payload)
	return offset
end

rawset(_G, 'mqtt_zadd', __f_mqtt_zadd)

function decode_utf8(                               -- Internal API
	input,index)  -- string

	local _length = string.byte(input, index) * 256 + string.byte(input, index+1)
	local output  = string.sub(input, index+2, index + _length + 1)

  return output,_length
end

local __f_mqtt_zrangebyscore
__f_mqtt_zrangebyscore = function (key,_smin,_pagesize,_time,_maxlimit)
	local _min=tonumber(_smin)
	local pagesize=tonumber(_pagesize)
	local maxlimit=tonumber(_maxlimit)
	local _max=pagesize+_min-1
	
	if pagesize > maxlimit then
		return {{},_max,"pagesize larger than maxlimit"}
	end
	
	local offset=__ext_global_var_4mqtt[key]
	if offset == nil then
		local t = redis.call('ZRANGE',key,-1,-1,'WITHSCORES')
		offset = t[2]
		if offset == nil then
			offset = 0
		else
			offset = tonumber(offset)
		end
		__ext_global_var_4mqtt[key] = offset
	end
	
	if offset<INT_MAX_RING1 and _max>INT_MAX_RING2 then
		if _max>INT_MAX_RING then
			_max=INT_MAX_RING
		end
	elseif offset>INT_MAX_RING2 then
		if _max>offset then
			_max=offset
		end
	end
	
	if _min > _max then
		return {{},_max,"min larger than max"}
	end
		
	if maxlimit>1 and (offset-_min)>maxlimit then
			_min=offset-maxlimit
			_max=_min+pagesize
	end

	
	local t = redis.call('zrangebyscore',key,_min,_max,'WITHSCORES')
	local recvTime=tonumber(_time)
	redis.log(redis.LOG_VERBOSE,#t,key,_min,_max,offset)
	local tlen=#t
	if(tlen>0)then
		_min=t[tlen]+1
	end
	for i=tlen, 2, -2 do
		local buffer = t[i-1]
		--redis.log(redis.LOG_VERBOSE,t[i])
		local message_type_flags = string.byte(buffer, 1)		
		local message_type = bit.rshift(message_type_flags, 4)
		local remaining_len = 0
		local multiplier = 1
		local index = 1
		repeat
		  index = index + 1
		  local digit = string.byte(buffer, index)
		  if(digit == nil)then return {nil,_max,"read remaining_len error"} end
		  remaining_len = remaining_len + ((digit % 128) * multiplier)
		  multiplier = multiplier * 128
		until digit < 128

		if (#buffer > remaining_len) then
			if (message_type == 0x03) then -- TYPE_PUBLISH
				index = index+3
				local buf=string.sub(buffer, index,index+8)
				local timestamp = readInt64(buf)
				--redis.log(redis.LOG_VERBOSE,"timestamp:",timestamp,",recvTime:",recvTime)
				if timestamp>0 and timestamp < recvTime then
					table.remove(t,i)
					table.remove(t,i-1)
				end
			end
		end
	end
	
	local total=#t
	if total<tlen then	
		
		if _max>=offset or _max>=INT_MAX_RING then
			redis.log(redis.LOG_VERBOSE,#t,key,_min,_max)
			
			return {t,_max}
		end
		
		local t1= __f_mqtt_zrangebyscore(key,_min,(pagesize-total/2),_time,_maxlimit)
		_max=t1[2]
		t1=t1[1]
		for i=1, #t1 do
			table.insert(t,t1[i])
		end
	end
	redis.log(redis.LOG_VERBOSE,#t,key,_min,_max)
	
	return {t,_max}
end

rawset(_G, 'mqtt_zrangebyscore', __f_mqtt_zrangebyscore)

--debug = nil