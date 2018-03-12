-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"

local method_name = ngx.req.get_method()
--ngx.req.read_body()
--local buffer = ngx.req.get_body_data()
--ngx.log(ngx.INFO,buffer)
if method_name == "DELETE" then
    local args = ngx.req.get_uri_args()
    local gid = args["gid"] 
    local sql="DELETE FROM t_policy_range WHERE pid IN(SELECT a.id FROM t_policy a inner join t_policy_group_rel b on a.id=b.pid WHERE b.pgid="..gid..")";
    local db = lcommon:mysql_open()
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end

    sql="DELETE FROM t_policy_numset WHERE pid IN(SELECT id FROM t_policy a inner join t_policy_group_rel b on a.id=b.pid WHERE b.pgid="..gid..")";
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end

    sql="DELETE FROM t_policy a WHERE id in (select pid t_policy_group_rel b where b.pid=a.id where b.gpid="..gid..")";
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end

    sql="DELETE FROM t_policy_group_rel a WHERE pgid="..gid;
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end

    sql="DELETE FROM t_policy_group WHERE id="..gid;
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