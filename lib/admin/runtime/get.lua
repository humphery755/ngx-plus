-- curl -H 'Accept: application/json' http://localhost:8000/action/runtime/get
local cjson = require "cjson"
local lutility = require "lib.Utility"
local resty_lock = require "resty.lock"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local ver = args["ver"]
local address = args["address"]
local status = args["status"]

local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp

local lock, err = resty_lock:new("DICT_DIV_sysconf_tmp")
if not lock then
	libmqttkit.log(ngx.ERR,"failed to create lock: ", err)
	ngx.exit(500)
end
local elapsed, err = lock:lock("__runtime.get.lock__")
dictSysconfig:set("alive.servers."..address,cjson.encode(args),10)
local ok, err = lock:unlock()
if not ok then
	libmqttkit.log(ngx.ERR,"failed to unlock: ", err)
end
					
local runtimeInfoBuf = dictSysconfig:get("DIV_runtimeInfo")
if runtimeInfoBuf == nil then
	ngx.sleep(1)
	ngx.exit(204)
end

local runtimeInfo,envhosts = lcommon.decode_runtimeInfo(runtimeInfoBuf)
if runtimeInfo == nil then
	ngx.sleep(1)
	ngx.exit(204)
end
if tostring(runtimeInfo.ver) == ver then
	ngx.sleep(2)
	ngx.exit(204)
end

if headers["Accept"] == 'application/octet-stream' then
	ngx.header.content_type = 'application/octet-stream';
	local nodes={}
	local elapsed, err = lock:lock("__runtime.get.lock__")
	for _,k in pairs(dictSysconfig:get_keys()) do
		if lutility.startswith(k,"alive.servers.") then
			local o=dictSysconfig:get(k);
			if o ~= nil then
				local node = cjson.decode(o)
				table.insert(nodes,node);
			end			
		end    
	end
	local syncNode;
	for _,n in pairs(nodes) do
		if n.status == 1 then
			syncNode = n
			break
		end
	end
	
	if syncNode == nil then
		args["status"]=1
		dictSysconfig:set("alive.servers."..address,cjson.encode(args))
		local ok, err = lock:unlock()
		if not ok then
			libmqttkit.log(ngx.ERR,"failed to unlock: ", err)
		end
		ngx.print(runtimeInfoBuf)
	else
		local ok, err = lock:unlock()
		if not ok then
			libmqttkit.log(ngx.ERR,"failed to unlock: ", err)
		end
		if status == "-1" then
			local runtimeInfo={}
			runtimeInfo.ver=0
			runtimeInfo["status"]=0
			runtimeInfo["fid"]=0
			ngx.print(lcommon.encode_runtimeInfo(runtimeInfo,{}))
		else
			ngx.sleep(1)
			ngx.exit(204)
		end		
	end
	
else
	ngx.header.content_type = 'application/json';
	ngx.print(cjson.encode(runtimeInfo));
end
libmqttkit.log(ngx.INFO,cjson.encode(runtimeInfo),cjson.encode(envhosts))
