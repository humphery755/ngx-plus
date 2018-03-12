-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"

ngx.req.read_body()
local buffer = ngx.req.get_body_data()
libmqttkit.log(ngx.INFO,buffer)

local pg = cjson.decode(buffer)

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()
local sql;
if not pg.gid or pg.gid<1 then
    sql = "insert into t_policy_group(flow_id,uri,action,backend,text,level)values("..pg.fid..","..ngx.quote_sql_str(pg.uri)..","..pg.action..","..ngx.quote_sql_str(pg.backend)..","..ngx.quote_sql_str(pg.text)..","..pg.level..")";
else
    sql = "update t_policy_group set uri="..ngx.quote_sql_str(pg.uri)..",action="..pg.action..",backend="..ngx.quote_sql_str(pg.backend)..",text="..ngx.quote_sql_str(pg.text)..",level="..pg.level.." where id="..pg.gid;
end

local dbres, err, errcode, sqlstate = db:query(sql)
if not dbres then
    lcommon:mysql_close(db)
    libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
    ngx.exit(500)
end
lcommon:mysql_close(db)
local res = {code=0,desc="ok"}
ngx.print(cjson.encode(res))
