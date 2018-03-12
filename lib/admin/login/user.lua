-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "Utility"

local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp
local method_name = ngx.req.get_method()

if method_name == "PUT" then
    ngx.req.read_body()
    local res = {code=0,desc="ok"}

    local args = ngx.req.get_post_args()
    local name = args["username"]
    local passwd = args["passwd"]
    if name==nil or passwd==nil then
        libmqttkit.log(ngx.ERR,"username or passwd is null");
        ngx.exit(403)
    end
    local sql="select username FROM t_user WHERE username="..ngx.quote_sql_str(name).." and password="..ngx.quote_sql_str(passwd).." and status=1";
    local db = lcommon:mysql_open()
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end

    if #dbres == 0 then
        res.code=-1
    else
        local token = lutility:uuid()
        dictSysconfig:set("_access_token_"..token,dbres[1]);
        ngx.header['Set-Cookie'] = {"access_token="..token.." ;path=/"};
    end

    lcommon:mysql_close(db)

    ngx.header.content_type = 'application/json';    
    ngx.print(cjson.encode(res))
else
    ngx.exit(403)
end