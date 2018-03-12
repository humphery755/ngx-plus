local http = require "resty.http"
local Utility = require "lib.Utility"

local _M = {
    _VERSION = '0.13'
}


local mt = { __index = _M }
local hc = http:new()


local update_handler_warpper
local update_handler_err
local MERGE_REQ_CONF
local shmDict


local function update_handler(premature,host,url,strtime,contentType)
	local content_type="application/x-www-form-urlencoded"
	if contentType~=nil then
		content_type=contentType
	end
	local ok, code, headers, status, body  = hc:request {
        url = host..url,
        --- proxy = "http://127.0.0.1:8888",
        --- timeout = 3000,
        --- scheme = 'https',
        method = "GET", -- POST or GET
        -- add post content-type and cookie
        headers = { ["Content-Type"] = content_type }
    }
    if ok then
    	headers["connection"]=nil
    	headers["content-length"]=nil
    	headers["server"]=nil
		headers["content-encoding"]=nil
    	headers["transfer-encoding"]=nil


    	local headersBuff
        local headersLen=0
        for k,v in pairs(headers) do
            if headersLen==0 then
                headersBuff=Utility.encode_utf8(k)..Utility.encode_utf8(v)
            else
                headersBuff=headersBuff..Utility.encode_utf8(k)..Utility.encode_utf8(v)
            end
            headersLen=headersLen+1
        end
        if headersLen > 0 then
            headersBuff=Utility.uint32ToBytes(headersLen)..headersBuff
        else
            headersBuff=Utility.uint32ToBytes(1)..Utility.encode_utf8("Content-Type")..Utility.encode_utf8("application/x-www-form-urlencoded")
        end

    	--ngx.log(ngx.DEBUG, strHeaders)
    	local content = Utility.uint32ToBytes(code)..headersBuff
		if body ~=nil then
    		content = content..body
    	end
		shmDict:set(url,content)
		if code ~= 200 then
			libmqttkit.log(ngx.DEBUG, url..": "..strtime..": "..code)
		end
    end

    if headers then
    	--ngx.log(ngx.DEBUG, url..": "..tableprint(headers))
    end
    --strtime.a()
    local time=tonumber(strtime)
    ngx.timer.at(time, update_handler,host,url,strtime,contentType)
end

update_handler_err = function(conf)
    libmqttkit.log(ngx.ERR,debug.traceback())
   ngx.timer.at(1, update_handler_warpper,conf)
   --xpcall(function () update_handler(false,conf[1],conf[2],conf[3],conf[4]) end, function () update_handler_err(conf) end)
   --ngx.log(ngx.ERR,conf[1],conf[2],conf[3],conf[4])
end

update_handler_warpper = function(premature,conf)
	xpcall(function () update_handler(false,conf[1],conf[2],conf[3],conf[4]) end, function () update_handler_err(conf) end)
    --ngx.timer.at(1, update_handler,host,url,time,contentType)
end

local WROK_ID = ngx.worker.id()

-- conf:  ["host","uri","expire","Content-Type","method","body"]
function _M.new(self, workId,conf,shm)
	if WROK_ID~=workId then
		return
	end
	MERGE_REQ_CONF=conf
	shmDict=shm

    for key,conf in pairs(MERGE_REQ_CONF) do
        local ok, err = ngx.timer.at(3, update_handler_warpper,conf)
        if not ok then
            error("failed to create timer: ", err)
        end
    end

    return setmetatable({
        MERGE_REQ_CONF = conf
    }, mt)
end

return _M

