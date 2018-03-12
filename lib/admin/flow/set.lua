-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"

ngx.req.read_body()
local buffer = ngx.req.get_body_data()
libmqttkit.log(ngx.INFO,buffer)

local flow = cjson.decode(buffer)

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()
local sql;
local syncSql=nil
if not flow.fid or flow.fid<1 then
    sql = "insert into t_diversion_flow(title,status,div_id,crt_date)values("..ngx.quote_sql_str(flow.title)..",0,"..flow.eid..",now()) ";
else
    sql = "update t_diversion_flow set title="..ngx.quote_sql_str(flow.title)..",status="..flow.status.." where id="..flow.fid;
    if flow.status == 4 then
        syncSql = "update t_diversion set ver=ver+1 where id in(select div_id from t_diversion_flow where id="..flow.fid..")";
    end
end

local dbres, err, errcode, sqlstate = db:query(sql)
if not dbres then
    lcommon:mysql_close(db)
    libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
    ngx.exit(500)
end

if syncSql~=nil then
    local dbres, err, errcode, sqlstate = db:query(syncSql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",syncSql)
        ngx.exit(500)
    end
end
lcommon:mysql_close(db)
local res = {code=0,desc="ok",insert_id=dbres["insert_id"]}
ngx.print(cjson.encode(res))
