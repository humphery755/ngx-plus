
local ffi = require("ffi")

ffi.cdef[[
typedef struct {
	size_t						len;
	unsigned char					*data;
} ngx_str_t;

char* strstr(const char *str1,const char *str2);
int lua_utility_indexOf(const char *str1,const char *str2);

const uint16_t utility_mqtt_read_uint16(const unsigned char *p,const size_t len);
const uint32_t utility_mqtt_read_uint32(const unsigned char *b,const size_t len);
const int64_t utility_mqtt_read_int64(const unsigned char *p,const size_t len);
const uint64_t utility_mqtt_read_uint64(const unsigned char *p,const size_t len);
const double utility_mqtt_read_double(const unsigned char *p,const size_t len);
int utility_mqtt_read_string(const unsigned char *p,const size_t plen,ngx_str_t *out);

int utility_mqtt_write_uint16(unsigned char *p,size_t len,uint16_t n);
int utility_mqtt_write_uint32(unsigned char *p,size_t len,uint32_t n);
int utility_mqtt_write_int64(unsigned char *b,size_t len,int64_t n);
int utility_mqtt_write_uint64(unsigned char *b,size_t len,uint64_t n);
int utility_mqtt_write_string(unsigned char *p,size_t len,ngx_str_t *str);

int ngx_lua_utility_uuid(char *buffer);

]]

local _M = {
    _VERSION = '0.10'
}


local mt = { __index = _M }

local _new_uuid = ffi.new("unsigned char[?]", 21)
function _M.uuid(self)
	ffi.C.ngx_lua_utility_uuid(_new_uuid)
  return ffi.string(_new_uuid,20)
end

_M.startswith = function(str, substr)
    if str == nil or substr == nil then
        return nil, "the string or the sub-stirng parameter is nil"
    end
    if string.find(str, substr) ~= 1 then
        return false
    else
        return true
    end
end

_M.endswith = function(str, substr)
    if str == nil or substr == nil then
        return nil, "the string or the sub-string parameter is nil"
    end
    str_tmp = string.reverse(str)
    substr_tmp = string.reverse(substr)
    if string.find(str_tmp, substr_tmp) ~= 1 then
        return false
    else
        return true
    end
end

_M.split = function(str, delimiter)
	if str==nil or str=='' or delimiter==nil then
		return nil
	end
	
  local result = {}
  for match in (str..delimiter):gmatch("(.-)"..delimiter) do
      table.insert(result, match)
  end
  return result
end

function _M.encode_utf8(input)  -- string
  local output
  output = string.char(math.floor(#input / 256))
  output = output .. string.char(#input % 256)
  output = output .. input
  return(output)
end

function _M.decode_utf8(input,index)  -- string
	local _length = string.byte(input, index) * 256 + string.byte(input, index+1)
	local output  = string.sub(input, index+2, index + _length + 1)
  return output,_length
end

local _new_u_char64 = ffi.new("unsigned char[?]", 8)
function _M.writeUint64(n)
	ffi.C.utility_mqtt_write_int64(_new_u_char64,8,n)
	return ffi.string(_new_u_char64,8)
end

function _M.readUint64(b,offset)
	local tmp = string.sub(b, offset)
	return tonumber(ffi.C.utility_mqtt_read_int64(tmp,8))
end

function _M.readUint32(b,offset)
	local tmp = string.sub(b, offset)
	return tonumber(ffi.C.utility_mqtt_read_uint32(tmp,4))
end

local _new_u_char32 = ffi.new("unsigned char[?]", 4)
function _M.writeUint32(n)
	ffi.C.utility_mqtt_write_uint32(_new_u_char32,4,n)
	return ffi.string(_new_u_char32,4)
end

function _M.readUint16(b,offset)
	local tmp = string.sub(b, offset)
	return tonumber(ffi.C.utility_mqtt_read_uint16(tmp,2))
end

local _new_u_char16 = ffi.new("unsigned char[?]", 2)
function _M.writeUint16(n)
	ffi.C.utility_mqtt_write_uint16(_new_u_char16,2,n)
	return ffi.string(_new_u_char16,2)
end

local do_tableprint

do_tableprint = function(data)
    local cstring = "{"
    if(type(data)=="table") then
		local cs = ""
        for k, v in pairs(data) do
			if (#cs > 2) then
				cs  = cs..", "
			end
            if(type(v)=="table") then
                cs = cs..tostring(k).."=" .. do_tableprint(v)
			else
				cs  = cs..tostring(k).."="..tostring(v)
            end
        end
		cstring = cstring .. cs
    elseif(type(data)=="nil") then
        cstring = cstring .. "nil"
	else
		cstring = cstring .. tostring(data)
    end
    cstring = cstring .."}"
	return cstring
end

_M.tableprint = do_tableprint

return _M
