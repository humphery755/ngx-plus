-- Copyright (C) Humphery yu<humphery.yu@gmail.com>
-- redis cluster

local resty_redis = require "resty.redis"
local byte = string.byte
local sub = string.sub
local tcp = ngx.socket.tcp

local setmetatable = setmetatable



local ffi = require("ffi")
ffi.cdef[[
	typedef struct { uint16_t crc;} t_redis_crc;
]]

local _M = resty_redis
_M._VERSION = '0.21'


local crc16tab= {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
    0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
    0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
    0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
    0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
    0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
    0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
    0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
}

-- Redis3 CRC-16-CCITT
local crc = ffi.new("t_redis_crc[?]", 1)

function _M.redisCRC16(buf, len)
    crc[0].crc=0
    for i=1,len,1 do
    	local byte = string.byte(buf, i)
    	crc[0].crc = bit.bxor(bit.lshift(crc[0].crc, 8), crc16tab[bit.bxor(bit.rshift(crc[0].crc, 8), byte)+1])
        -- crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *buf++)&0x00FF];
    end
    return crc[0].crc
end


local mt = { __index = _M }


function _M.new(self,cache)
    local sock, err = tcp()
    if not sock then
        return nil, err
    end
    if not cache then
        return nil, "cluster or cache not initialized"
    end
    return setmetatable({ sock = sock,cache=cache}, mt)
end

function _M.newCluster(self)
	local cache = {}
	setmetatable(cache, {_mode = "v"})
    return setmetatable({ cache = cache}, mt)
end

function _M.connect(self, key, optFlg)
    local sock = self.sock
    if not sock then
        return nil, "not initialized"
    end
    local cache = self.cache


    self.subscribed = nil
	if not optFlg then
        optFlg=0
    end
    local crc = self.redisCRC16(key,#key)
    local slot = crc%16384
    local slots = cache

    local server = nil
    for i,s in pairs(slots) do
    	if s.min<=slot and s.max>=slot then
    		server=s
    	end
    end
    if server == nil then
        return nil, "unknow error: slot "..slot.." no found"
    end

	local ip = server.addrs[1]
	local port = server.addrs[2]
	if not ip then
        return nil, port
    end
	self.key=key
    return sock:connect(ip,port)
end

local function build_slots(sock,status,out_data)
	local line, err = sock:receive()
	if not line then
		if err == "timeout" then
			sock:close()
		end
		return nil, err
	end
	--ngx.log(ngx.DEBUG,line)

	local prefix = byte(line)

	if prefix == 42 then -- char '*'
		local n = tonumber(sub(line, 2))

		if n < 0 then
			return nil,"error reply: "..line
		end

		local last_slot

		local nvals = 0
		if(status==1) then
			status=2
			for i = 1, n do
				nvals = nvals + 1
				local res, err,status = build_slots(sock,status,nil)
				if err then
					return nil, err
				end
				out_data[i]=res
			end
		elseif status==2 then
			last_slot={}
			-- : min
			local line, err = sock:receive()
			if not line then
				if err == "timeout" then
					sock:close()
				end
				return nil, err
			end
			--ngx.log(ngx.DEBUG,line)
			last_slot.min=tonumber(sub(line, 2))
			-- : max
			local line, err = sock:receive()
			if not line then
				if err == "timeout" then
					sock:close()
				end
				return nil, err
			end
			--ngx.log(ngx.DEBUG,line)
			last_slot.max=tonumber(sub(line, 2))

			for i = 1, n-2 do
				-- *
				local line, err = sock:receive()
				if not line then
					if err == "timeout" then
						sock:close()
					end
					return nil, err
				end
				--ngx.log(ngx.DEBUG,line)
				-- $ip
				local line, err = sock:receive()
				if not line then
					if err == "timeout" then
						sock:close()
					end
					return nil, err
				end
				--ngx.log(ngx.DEBUG,line)
				local size = tonumber(sub(line, 2))
				if size < 0 then
					return null
				end

				local line, err = sock:receive(size)
				if not line then
					if err == "timeout" then
						sock:close()
					end
					return nil, err
				end
				--ngx.log(ngx.DEBUG,line)
				if last_slot.addrs == nil then
					last_slot.addrs={}
				end
				table.insert(last_slot.addrs,line)

				local dummy, err = sock:receive(2) -- ignore CRLF
				if not dummy then
					return nil, err
				end
				-- : port
				local line, err = sock:receive()
				if not line then
					if err == "timeout" then
						sock:close()
					end
					return nil, err
				end
				--ngx.log(ngx.DEBUG,line)
				table.insert(last_slot.addrs,tonumber(sub(line, 2)))
			end
			return last_slot
		end

		return out_data,nil --vals

	else
		return nil, "unkown prefix: \"" .. prefix .. "\""
	end
end

local function tableprint(data)
	local cstring = "{"
	if(type(data)=="table") then
		local cs = ""
		for k, v in pairs(data) do
			if (#cs > 2) then
				cs	= cs..", "
			end
			if(type(v)=="table") then
				cs = cs..tostring(k).."=" .. tableprint(v)
			else
				cs	= cs..tostring(k).."="..tostring(v)
			end
		end
		cstring = cstring .. cs
	else
		cstring = cstring .. tostring(data)
	end
	cstring = cstring .."}"
	return cstring
end

function _M.sync_slots(self,args)
	local nargs = #args
	local sock = tcp()
	sock:settimeout(5000)

	for i = 1, nargs,2 do
		local ip = args[i]
		local port = args[i+1]
		ngx.log(ngx.DEBUG,"connecting to ",ip,":",port)
		local ok, err = sock:connect(ip, tonumber(port))
		if ok then
			local ok, err = sock:send("cluster slots\r\n")
			if not err then
				local datas, err = build_slots(sock,1,self.cache)
				if err then
					ngx.log(ngx.ERR,"failed to receive: ",err)
				else
					ngx.log(ngx.DEBUG,tableprint(datas))
					sock:close()
					return
				end
			end
		else
			ngx.log(ngx.ERR,"failed to connect to ",ip,":",port,": ", err)
		end
	end
	sock:close()
end

return _M
