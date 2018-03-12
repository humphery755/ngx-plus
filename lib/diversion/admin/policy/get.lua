-- curl -H 'Accept: application/json' http://localhost:8000/admin/policy/get
-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/policy/get

local cjson = require "cjson"

local dictSysconfig = ngx.shared.DICT_DIVERSION_SYSCONFIG

local headers = ngx.req.get_headers()
libmqttkit.log(ngx.DEBUG,headers["Accept"])
if headers["Accept"] == 'application/json' then
	local buf = dictSysconfig:get("DIV_RULES")
	ngx.header.content_type = 'application/json';
	if buf == nil then
		ngx.exit(200)
	end
	local divRules = lcommon.decode_policy(buffer)
	libmqttkit.log(ngx.DEBUG,cjson.encode(divRules))
	ngx.print(cjson.encode(divRules))
elseif headers["Accept"] == 'application/octet-stream' then
	local buff = dictSysconfig:get("DIV_RULES")
	ngx.header.content_type = 'application/octet-stream';
	if buf == nil then
		ngx.exit(200)
	end
	ngx.print(buff)
else
	ngx.exit(403)
end
