-- curl -H 'Content-Type: application/json' --data @test.json http://localhost:8000/admin/policy/set
-- curl -H 'Content-Type: application/octet-stream' --data @test.dat http://localhost:8000/admin/policy/set

local cjson = require "cjson"

local dictSysconfig = ngx.shared.DICT_DIVERSION_SYSCONFIG

-- local args = ngx.req.get_uri_args()
local headers = ngx.req.get_headers()
ngx.req.read_body()
local buffer = ngx.req.get_body_data()
if headers["Content-Type"] == 'application/octet-stream' then
	local divs = lcommon.decode_policy(buffer)
	if divs == nil then
		libmqttkit.log(ngx.ERR,"policy set failure, body is null ");
		ngx.exit(500)
	end
	dictSysconfig:set("DIV_RULES",buffer)
	libmqttkit.log(ngx.INFO,"policy set success, body: ",cjson.encode(divs));
elseif headers["Content-Type"] == 'application/json' then
	local divRules = cjson.decode(buffer)
	local buff = lcommon.encode_policy(divRules)
	dictSysconfig:set("DIV_RULES",buff)
	libmqttkit.log(ngx.INFO,"policy set success, body: ",buffer);
else
	ngx.exit(403)
end
