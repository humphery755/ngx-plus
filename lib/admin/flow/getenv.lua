-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()

local sql = "select id eid, div_env from t_diversion order by seq";

local dbres, err, errcode, sqlstate = db:query(sql, 10)
if not dbres then
    lcommon:mysql_close(db)
    libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
    ngx.exit(500)
end
lcommon:mysql_close(db)
libmqttkit.log(ngx.INFO,cjson.encode(dbres))
ngx.print(cjson.encode(dbres))
