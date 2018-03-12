-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local libck = require "resty.cookie"

local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp

if(ngx.var.uri == '/' or ngx.var.uri == '/admin/action/login/user') then return end;

local cookie, err = libck:new()
if not cookie then
    libmqttkit.log(ngx.ERR, err)
    return ngx.redirect("/",ngx.NGX_HTTP_MOVED_PERMANENTLY)
end

-- get single cookie
local token, err = cookie:get("access_token")
if not token then
    -- return ngx.redirect("/",ngx.NGX_HTTP_MOVED_PERMANENTLY)
    ngx.exit(403)
end

local authobj = dictSysconfig:get("_access_token_"..token);
if not token then
    return ngx.redirect("/",ngx.NGX_HTTP_MOVED_PERMANENTLY)
end