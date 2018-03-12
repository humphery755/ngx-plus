-- curl -H 'Cookie: aid=100' -v "http://localhost:4000/index.html"
if ngx.var.action ~= '4' or ngx.var.plugin == '' then
return
end
local plugin = require("diversion.plugins."..ngx.var.plugin)
plugin.exec()

