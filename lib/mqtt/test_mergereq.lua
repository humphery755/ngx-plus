local Utility = require "lib.Utility"

local dict = ngx.shared.MERGE_REQ_DICT
local uri=ngx.var.request_uri
local indexOf=ffi.C.ngx_lua_utility_indexOf(uri,"&time=")
if(indexOf>0) then
	uri=string.sub(uri, 1, indexOf)
end
local content=dict:get(uri)
if content == nil then
	--ngx.exit(404)
	return ngx.exec("@web-inner")
end
local index=1
local code=Utility.readUint32(content,index)
index=index+4
local headers={}
local headersLen=Utility.readUint32(content, index)
index=index+4

for i = 1,headersLen,1 do
	local k,len=Utility.decode_utf8(content, index)
	index=index+len+2
	local v,len=Utility.decode_utf8(content, index)
	index=index+len+2
	ngx.header[k]=v
end

local len=#content
ngx.status=code
if (len-index) > 1 then
	body = string.sub(content, index, len)
	ngx.print(body)
end
ngx.exit(code)
