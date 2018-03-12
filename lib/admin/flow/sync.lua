-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"


local method_name = ngx.req.get_method()
--ngx.req.read_body()
--local buffer = ngx.req.get_body_data()
--ngx.log(ngx.INFO,buffer)
if method_name == "PUT" then
    local args = ngx.req.get_uri_args()
    local fid = args["fid"]

    local sql = "update t_diversion set ver=ver+1 where id in(select div_id from t_diversion_flow where id="..fid..")";
    local db = lcommon:mysql_open()
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end
    lcommon:mysql_close(db)
    ngx.header.content_type = 'application/json';
    local res = {code=0,desc="ok"}
    ngx.print(cjson.encode(res))
else
    ngx.exit(403)
end
