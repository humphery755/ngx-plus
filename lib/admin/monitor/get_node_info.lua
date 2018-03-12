-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local eid = args["eid"] 

ngx.header.content_type = 'application/json';

local nodes={}
for _,k in pairs(dictSysconfig:get_keys()) do
    if lutility.startswith(k,"monitor.status.") then
        local o=dictSysconfig:get(k);
        if o ~= nil then
            local node = cjson.decode(o)
            table.insert(nodes,node);
        end
        
    end    
end
ngx.print(cjson.encode(nodes))
