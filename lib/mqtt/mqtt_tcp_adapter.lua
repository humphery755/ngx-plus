local bit = require"bit"

local _M = {
    _VERSION = '0.13'
}

local mt = { __index = _M }

function _M.new(self, opts)

    if not opts then
        libmqttkit.log(ngx.ERR, "failed to new")
	   return nil
    end
	local sock
    sock, err = ngx.req.socket(true)
    if not sock then
        return nil, err
    end
    local timeout = opts.timeout

    if timeout then
        sock:settimeout(timeout)
    end

    return setmetatable({
        timeout = timeout,
        max_payload_len = opts.max_payload_len or 65535,
        sock = sock,
    }, mt)
end

function _M.encodeMqttHeader(self,message_type,qosLevel,dupFlag,retainFlag)
    local flags = 0;
    if (dupFlag) then
        flags = bit.bor(flags,0x08)
    end
    if (retainFlag) then
        flags = bit.bor(flags,0x01)
    end
    flags = bit.bor(flags,bit.lshift(bit.band(qosLevel, 0x03),1));

    return string.char(bit.bor(bit.lshift(message_type, 4),flags))
end

MQTT_TYPE_PINGRESP  = 0x0d

function _M.send_pong(self)
    local message = self:encodeMqttHeader(MQTT_TYPE_PINGRESP,0,0,false)
    message = message .. string.char(0)
    return self:send_binary(message)
end

function _M.recv_frame(self,byte_count)
	--byte_count = byte_count or 128
    --return sock:receive(byte_count)
    local buffer
    local message_type_flags,err = self.sock:receive(1)
    if not message_type_flags then
    	self.fatal=1
        return nil, nil, "failed to receive the first 1 bytes: " .. err
    end
    buffer=message_type_flags
    local multiplier = 1
    local remaining_length = 0

    repeat
      local digit,err = self.sock:receive(1)
      if not digit then
      	self.fatal=1
        return nil, nil, "failed to receive the 2 byte remaining length: " .. (err or "unknown")
      end
      buffer=buffer..digit
      digit = string.byte(digit, 1)
      remaining_length = remaining_length + ((digit % 128) * multiplier)
      multiplier = multiplier * 128
    until digit < 128
    if remaining_length > 0 then
    	if remaining_length > self.max_payload_len then
    		self.fatal=1
	        return nil, nil, "exceeding max payload len"
	    end
    	local data,err=self.sock:receive(remaining_length)
    	if not data then
    		self.fatal=1
            return nil, nil, "failed to read payload: " .. (err or "unknown")
        end
    	buffer=buffer..data
    end
    message_type_flags = string.byte(message_type_flags, 1)
    local message_type = bit.rshift(message_type_flags, 4)
    libmqttkit.log(ngx.DEBUG,remaining_length,", ",message_type)
    return buffer
end

function _M.send_close(self)
    --self.sock:close()
    --if not self.sock then return end
    --self.sock:setkeepalive(1)
    self.sock=nil
end

function _M.send_binary(self,message)
    return self.sock:send(message)
end


return _M