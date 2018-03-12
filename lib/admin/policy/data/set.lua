-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local lutility = require "lib.Utility"

ngx.req.read_body()
local args = ngx.req.get_uri_args()
local ptype = args["ptype"]
local buffer = ngx.req.get_body_data()
libmqttkit.log(ngx.INFO,buffer)

local pds = cjson.decode(buffer)

ngx.header.content_type = 'application/json';

local db = lcommon:mysql_open()
local sql;
for k,pd in pairs(pds) do
    if ptype=='aidrange' or ptype=='cidrange' or ptype=='iprange' then
        if not pd.id or pd.id<1 then
            sql = "insert into t_policy_range(pid,start,end)values("..pd.pid..","..pd.start..","..pd["end"]..")";
        else
            sql = "update t_policy_range set start="..pd.start..",end="..pd["end"].." where id="..pd.id;
        end
    else
        if not pd.id or pd.id<1 then
            sql = "insert into t_policy_numset(pid,val)values("..pd.pid..","..pd.val..")";
        else
            sql = "update t_policy_numset set val="..pd.val.." where id="..pd.id;
        end
    end
    local dbres, err, errcode, sqlstate = db:query(sql)
    if not dbres then
        lcommon:mysql_close(db)
        libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, " ",sql)
        ngx.exit(500)
    end
end
lcommon:mysql_close(db)
local res = {code=0,desc="ok"}
ngx.print(cjson.encode(res))
