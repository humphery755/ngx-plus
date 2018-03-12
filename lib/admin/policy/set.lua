-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"

ngx.req.read_body()
local buffer = ngx.req.get_body_data()
libmqttkit.log(ngx.INFO,buffer)

local p = cjson.decode(buffer)

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()
local sql;
if not p.id or p.id<1 then
    local pid
    sql = "insert into t_policy(type)values("..ngx.quote_sql_str(p.type)..")";
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end
    pid = dbres["insert_id"]
    sql = "insert into t_policy_group_rel(pid,pgid)values("..pid..","..p.gid..")";
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end
    libmqttkit.log(ngx.INFO,cjson.encode(dbres))
    lcommon:mysql_close(db)
    local res = {code=0,desc="ok",insert_id=pid}
    ngx.print(cjson.encode(res))
else
    sql = "update t_policy set type="..ngx.quote_sql_str(p.type).." where id="..p.id;
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end
    libmqttkit.log(ngx.INFO,cjson.encode(dbres))
    lcommon:mysql_close(db)
    local res = {code=0,desc="ok",insert_id=dbres["insert_id"]}
    ngx.print(cjson.encode(res))
end


