-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local fid = args["fid"] 

ngx.header.content_type = 'application/json';


local db = lcommon:mysql_open()

local sql="select uri,action,backend,a.id gid,text,level from t_policy_group a where a.flow_id="..fid.." order by LENGTH(uri) DESC,a.level"
local dbres, err, errcode, sqlstate = db:query(sql, 10)
if not dbres then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	return
end
if #dbres == 0 then
	libmqttkit.log(ngx.ERR,"no data for sql: "..sql)
	dbres={};
end
local divPolicys={}
for i,res in pairs(dbres) do		
	local g=nil
	for k,g1 in pairs(divPolicys) do
		if g1.gid == res.gid then
			g=g1
			break
		end
	end
	if g == nil then
		g={}
		table.insert(divPolicys,g)
	end
	g.uri=res.uri
	g.action=res.action
	g.backend=res.backend
	local gid=res.gid
	g.text=res.text
	g.gid=gid
	g.level=res.level
	g.policys={}

	local dbres1, err, errcode, sqlstate = db:query("select a.id,a.type from t_policy a inner join t_policy_group_rel b on a.id=b.pid where b.pgid="..gid, 100)
	if not dbres1 then
		lcommon:mysql_close(db)
		libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		ngx.exit(500)
	end
	if #dbres1 == 0 then
		g.policys=nil
	end
	for j,res1 in pairs(dbres1) do
		local type=res1.type
		local policy={}
		policy["type"]=type
		policy["id"]=tonumber(res1.id)
		policy["gid"]=tonumber(res1.gid)
		table.insert(g.policys,policy)
	end
end

lcommon:mysql_close(db)
libmqttkit.log(ngx.INFO,"fid:",fid,", body:",cjson.encode(divPolicys))
ngx.print(cjson.encode(divPolicys))
