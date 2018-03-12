-- curl -H 'Accept: application/json' http://localhost:8000/admin/runtime/get
local libdiv = require "ngxext.diversion"
local cjson = require "cjson"

local runtimeInfo = libdiv.get_runtimeinfo()

if runtimeInfo == nil then
	ngx.sleep(1)
	ngx.exit(204)
end

ngx.header.content_type = 'application/json';
ngx.print(cjson.encode(runtimeInfo));
