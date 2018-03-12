-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local dictSysconfig = ngx.shared.DICT_DIV_sysconf_tmp

local headers = ngx.req.get_headers()
libmqttkit.log(ngx.DEBUG,headers["Accept"])
local args = ngx.req.get_uri_args()
local ver = args["ver"] 

if ver==nil then
	libmqttkit.log(ngx.ERR,"ver is null")
	ngx.exit(403)
end
if headers["Accept"] ~= 'application/octet-stream' then
	ngx.exit(403)
end

ngx.header.content_type = 'application/octet-stream';

local runtimeInfoBuf = dictSysconfig:get("DIV_runtimeInfo")
if runtimeInfoBuf == nil then
	libmqttkit.log(ngx.ERR,"runtimeinfo is null")
	ngx.exit(500)
end
local runtimeInfo,envhosts = lcommon.decode_runtimeInfo(runtimeInfoBuf)
if runtimeInfo == nil then
	libmqttkit.log(ngx.ERR,"runtimeinfo is null")
	ngx.exit(500)
end
if tonumber(ver) ~= runtimeInfo.ver then
	libmqttkit.log(ngx.ERR,"notmatch the version[",ver,"!=",runtimeInfo.ver,"]")
	ngx.exit(403)
end

local fid = runtimeInfo.fid
local db = lcommon:mysql_open()

local dbres1, err, errcode, sqlstate = db:query("SELECT a.id,a.type,bitsetid FROM t_policy a INNER JOIN t_policy_group_rel b ON a.id=b.pid WHERE b.pgid IN (SELECT id FROM t_policy_group WHERE flow_id="..fid..")", 100)
if not dbres1 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	ngx.exit(500)
end
if #dbres1 == 0 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"policy can't null, please check config for gid: ",gid)
	ngx.exit(500)
end
ngx.print(lutility.writeUint16(#dbres1))
libmqttkit.log(ngx.INFO,"policy size: ",#dbres1)
for j,res1 in pairs(dbres1) do
	local type=res1.type
	local pid=tonumber(res1.id)
	local bitsetid=tonumber(res1.bitsetid)

	if type == "aidset" or type == "cidset" or type == "oidset" then
		ngx.print(lutility.writeUint32(pid)..lutility.encode_utf8(type)..lutility.writeUint16(bitsetid)..lutility.writeUint16(0))
		goto continue;
	end
	
	if "iprange" == type or "aidrange" == type or "cidrange" == type or "oidrange" == type then
		local dbres2, err, errcode, sqlstate = db:query("select start,end from t_policy_range where pid="..pid.." and start<=end order by start,end limit 50000", 50000)
		if not dbres2 then
			lcommon:mysql_close(db)
			libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
			ngx.eof()
			return
		end
		ngx.print(lutility.writeUint32(pid)..lutility.encode_utf8(type)..lutility.writeUint16(bitsetid)..lutility.writeUint16(#dbres2))
		for k,res2 in pairs(dbres2) do
			ngx.print(lutility.writeUint64(tonumber(res2["start"]))..lutility.writeUint64(tonumber(res2["end"])))
		end
		goto continue;
	end

	local dbres2, err, errcode, sqlstate = db:query("select val from t_policy_numset where pid="..pid.." order by val limit 50000", 500000)
	if not dbres2 then
		lcommon:mysql_close(db)
		libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		ngx.eof()
		return
	end
	ngx.print(lutility.writeUint32(pid)..lutility.encode_utf8(type)..lutility.writeUint16(bitsetid)..lutility.writeUint16(#dbres2))
	for k,res2 in pairs(dbres2) do
		--table.insert(policy["data"],tonumber(res2["val"]))				
		ngx.print(lutility.writeUint32(tonumber(res2["val"])))
	end
	
::continue::
end

local sql = "SELECT uri,action,backend,a.id gid,text FROM t_policy_group a WHERE a.flow_id="..fid.." ORDER BY LENGTH(uri) DESC,a.level"
local dbres, err, errcode, sqlstate = db:query(sql, 10)
if not dbres then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
	return
end
if #dbres == 0 then
	lcommon:mysql_close(db)
	libmqttkit.log(ngx.ERR,"no data for sql: "..sql)
	ngx.eof()
	return
end
-- ~!@#$%^&*?
ngx.print(lutility.writeUint16(#dbres))
for i,res in pairs(dbres) do		
	local uri=res.uri
	local action=res.action
	local backend=res.backend
	local gid=res.gid
	local text=res.text
	if text == nil then
	text=""
	end

	local dbres1, err, errcode, sqlstate = db:query("SELECT a.id FROM t_policy a INNER JOIN t_policy_group_rel b ON a.id=b.pid WHERE b.pgid="..gid, 100)
	if not dbres1 then
		lcommon:mysql_close(db)
		libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		ngx.eof()
		return
	end
	if #dbres1 == 0 then
		lcommon:mysql_close(db)
		libmqttkit.log(ngx.ERR,"policy can't null, please check config for gid: ",gid)
		ngx.eof()
		return
	end
	libmqttkit.log(ngx.INFO,uri,", ",backend,", action:",action,", text:",text,", gid:",gid,",policy size: ",#dbres1)	
	ngx.print(lutility.encode_utf8(uri)..lutility.writeUint16(action)..lutility.encode_utf8(backend)..lutility.encode_utf8(text)..lutility.writeUint16(#dbres1))
	for j,res1 in pairs(dbres1) do
		local pid=tonumber(res1.id)
		ngx.print(lutility.writeUint32(pid))
	end
end

lcommon:mysql_close(db)
ngx.exit(200)
