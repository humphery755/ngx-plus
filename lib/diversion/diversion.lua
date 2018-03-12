-- curl -H 'Cookie: aid=100' -v "http://localhost:4000/index.html"

-- local cjson = require "cjson"
local libck = require "resty.cookie"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local redirectInfo  = 'proxypass to upstream http://'

local function myerror_handler( err )
    libmqttkit.log(ngx.ERR,err)
end

local runtimeInfo = libdiv.get_runtimeinfo()
if runtimeInfo == nil then
    libmqttkit.log(ngx.WARN,"getRuntimeInfo is null\t",redirectInfo,ngx.var.backend)
    return
end

if runtimeInfo.status ~= 2 then
    libmqttkit.log(ngx.DEBUG,"runtimeInfo \"status\": ",runtimeInfo.status,", ",redirectInfo,ngx.var.backend)
    return
end

-- ngx.log(ngx.DEBUG,cjson.encode(runtimeInfo))
local cookie2form = function(text)
    if string.find(text,"${ngx.var.http_cookie}") then
        

        text=string.gsub(text, "${ngx.var.http_cookie}", "a")
    end
	local cookie = resty_cookie:new()
    local all_cookie = cookie:get_all()
	return backend,text,action
end

local pfunc = function()
	local backend,text,action = libdiv.exec_policy(runtimeInfo.ver)
	--local buf=lutility.writeUint16(action)..lutility.encode_utf8(backend)
	--dictUserUPS:set(aid,buf)
	-- libmqttkit.log(ngx.DEBUG,action,",",backend)
	return backend,text,action
end

local status, backend,text,action = xpcall(pfunc, myerror_handler)
if not status then
    libmqttkit.log(ngx.ERR,status,", ",redirectInfo,ngx.var.backend)
    return
end

if backend == nil or action == 0 then
    libmqttkit.log(ngx.INFO,redirectInfo,ngx.var.backend)
    return
end
ngx.var.action=action
if action == 1 then
    local ups = lconfig.DIV_BACKEND_MAPPING[backend]..ngx.var.suffix
    ngx.var.backend = ups
    libmqttkit.log(ngx.INFO,redirectInfo,ngx.var.backend)
elseif action == 2 then
    if "*" == text then
        ngx.var.backend = backend..ngx.var.uri
    else
        if string.find(text,"${ngx.var.http_cookie}") then
            text=string.gsub(text, "${ngx.var.http_cookie}", "a")
        end
        local tmp = string.sub(text,1,2)
        if tmp == "+" then
            ngx.var.backend = backend..ngx.var.uri..string.sub(text,2)
        else
            ngx.var.backend = backend..text
        end
    end
    libmqttkit.log(ngx.INFO,"redirect to ",ngx.var.backend)
    return ngx.redirect(ngx.var.backend,ngx.HTTP_MOVED_PERMANENTLY)
elseif action == 3 then
    ngx.var.backend = backend
    libmqttkit.log(ngx.WARN,"rewrite to ",ngx.var.backend)
else
--- action:0:default, 1:given, 2:redirect, 3:plugins-content_filte, 4:plugins-header_filter, 5:plugins-body_filter
    local ups = lconfig.DIV_BACKEND_MAPPING[backend]..ngx.var.suffix
    ngx.var.backend = ups
    ngx.var.plugin = text
    libmqttkit.log(ngx.INFO,"action: ",ngx.var.action,", ",redirectInfo,ngx.var.backend)
end
