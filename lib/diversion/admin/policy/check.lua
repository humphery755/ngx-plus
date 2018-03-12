-- curl -H 'Accept: application/json'  http://localhost:8000/admin/policy/check
local cjson = require "cjson"

local dictSysconfig = ngx.shared.DICT_DIVERSION_SYSCONFIG

local headers = ngx.req.get_headers()
libmqttkit.log(ngx.DEBUG,headers["Accept"])
if headers["Accept"] == 'application/json' then
	local buf = dictSysconfig:get("DIV_RULES")
	ngx.header.content_type = 'application/json';
	if buf == nil then
		ngx.exit(400)
	end
	local divRules = lcommon.decode_policy(buf)
	libmqttkit.log(ngx.DEBUG,cjson.encode(divRules))
	ngx.print(cjson.encode(divRules))
else
	ngx.exit(403)
end