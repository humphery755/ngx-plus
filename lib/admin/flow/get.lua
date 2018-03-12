-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local eid = args["eid"] 

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()

local sql = "SELECT a.id fid,title,crt_date,status,b.div_env,b.src,b.to,b.ver,b.id eid FROM t_diversion b LEFT JOIN t_diversion_flow a ON (a.div_id=b.id AND a.status>=0 AND a.status<4) WHERE b.id="..eid;

local dbres, err, errcode, sqlstate = db:query(sql, 10)
if not dbres then
    lcommon:mysql_close(db)
    libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
    ngx.exit(500)
end
lcommon:mysql_close(db)
libmqttkit.log(ngx.INFO,"eid:",eid,", body:",cjson.encode(dbres))
ngx.print(cjson.encode(dbres))
