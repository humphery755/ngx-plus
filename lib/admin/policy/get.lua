-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local pid = args["pid"]

ngx.header.content_type = 'application/json';


local db = lcommon:mysql_open()
local sql="select a.id,b.pgid gid,a.type from t_policy a inner join t_policy_group_rel b on a.id=b.pid where a.id="..pid
local dbres1, err, errcode, sqlstate = db:query(sql, 100)
if not dbres1 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	ngx.exit(500)
end
local policys={}
for j,res1 in pairs(dbres1) do
	local type=res1.type
	local policy={}
	policy["type"]=type
	policy["id"]=tonumber(res1.id)
	policy["gid"]=tonumber(res1.gid)
	table.insert(policys,policy)
end

lcommon:mysql_close(db)
libmqttkit.log(ngx.INFO,"pid:",pid,", body:",cjson.encode(policys))
ngx.print(cjson.encode(policys))
