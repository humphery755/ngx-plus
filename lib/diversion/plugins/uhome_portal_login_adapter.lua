-- curl -H 'Cookie: aid=100' -v "http://localhost:4000/index.html"
local cjson = require "cjson"

local _M = {
    _VERSION = '0.01'
}

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

_M.exec = function()
	--local u = ngx.var.arg_city
	if ngx.status == ngx.HTTP_MOVED_TEMPORARILY or ngx.status == ngx.HTTP_MOVED_PERMANENTLY then
		libmqttkit.log(ngx.INFO,"divs ****** ",ngx.status,", ",cjson.encode(ngx.resp.get_headers())); --ngx.resp.add_header
	elseif ngx.status ~= ngx.HTTP_OK then
		libmqttkit.log(ngx.ERR,"divs ****** ",ngx.status,", ",cjson.encode(ngx.resp.get_headers()));
	end
	-- ngx.log(ngx.ERR,"===================== ",ngx.header["content-length"],", ",ngx.header.location,",",ngx.header.content_type )
	--if ngx.header["Location"] ~= nil then
		--local newstr=string.gsub(ngx.header["Location"],"http://www.uhomecp.com/uhome-portal/shiro-cas", "http://n1.uhomecp.com/uhome-portal/shiro-cas")
		-- ngx.header["Location"]=newstr
		
	--end
	ngx.header.Foo = "blah"
end
return _M
