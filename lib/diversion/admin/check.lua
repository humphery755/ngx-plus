-- 
local libdiv = require "ngxext.diversion"

local runtimeInfo = libdiv.get_runtimeinfo()
if runtimeInfo == nil then
    ngx.exit(502)
end
if runtimeInfo.status == 0 or runtimeInfo.status == 2 then
    ngx.exit(200)
end

ngx.exit(502)
