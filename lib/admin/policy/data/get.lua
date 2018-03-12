-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local pid = args["pid"]
local id = args["id"]
local pageIndex = args["pageIndex"]
local pageSize=16;
local rowIndex=0;
if pageIndex~= nil then
	pageIndex=tonumber(pageIndex);
	rowIndex=pageIndex*(pageSize-1);
end

ngx.header.content_type = 'application/json';


local db = lcommon:mysql_open()

local datas={}

local sql = "select id,start,end from t_policy_range ";
if pid~=nil then
	sql=sql.."where pid="..pid.." limit "..rowIndex..","..pageSize;
else
	sql=sql.."where id="..id;
end
libmqttkit.log(ngx.DEBUG,sql);
local dbres2, err, errcode, sqlstate = db:query(sql, pageSize)
if not dbres2 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	ngx.exit(500)
end
for k,res2 in pairs(dbres2) do
	local range={}
	range["id"]=tonumber(res2["id"])
	range["pid"]=pid
	range["start"]=tonumber(res2["start"])
	range["end"]=tonumber(res2["end"])

	table.insert(datas,range)
end

sql = "select id,val from t_policy_strset ";
if pid~=nil then
	sql=sql.."where pid="..pid.." limit "..rowIndex..","..pageSize;
else
	sql=sql.."where id="..id;
end
libmqttkit.log(ngx.DEBUG,sql);
local dbres2, err, errcode, sqlstate = db:query(sql, pageSize)
if not dbres2 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	ngx.exit(500)
end
for k,res2 in pairs(dbres2) do
	local set={}
	set["id"]=tonumber(res2["id"])
	set["pid"]=pid
	set["val"]=res2["val"]
	table.insert(datas,set)
end

sql = "select id,val from t_policy_numset ";
if pid~=nil then
	sql=sql.."where pid="..pid.." limit "..rowIndex..","..pageSize;
else
	sql=sql.."where id="..id;
end
libmqttkit.log(ngx.DEBUG,sql);
local dbres2, err, errcode, sqlstate = db:query(sql, pageSize)
if not dbres2 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	ngx.exit(500)
end
for k,res2 in pairs(dbres2) do
	local set={}
	set["id"]=tonumber(res2["id"])
	set["pid"]=pid
	set["val"]=tonumber(res2["val"])
	table.insert(datas,set)
end


lcommon:mysql_close(db)
libmqttkit.log(ngx.INFO,"pid:",pid,", body:",cjson.encode(datas))
ngx.print(cjson.encode(datas))
