-- curl -H 'Accept: application/octet-stream' http://localhost:8000/admin/action/get_policy

local cjson = require "cjson"
local libdiv = require "ngxext.diversion"
local lutility = require "lib.Utility"

local headers = ngx.req.get_headers()
local args = ngx.req.get_uri_args()
local pid = args["pid"] 
local pIdx = args["pIdx"]
local pagesize = 40000
local pageoffset=pIdx*pagesize
libmqttkit.log(ngx.DEBUG,headers["Accept"])
if headers["Accept"] == 'application/octet-stream' then
	ngx.header.content_type = 'application/octet-stream';

	local db = lcommon:mysql_open()

	local dbres, err, errcode, sqlstate = db:query("select val from t_policy_numset where pid="..pid.." limit "..pageoffset..","..pagesize+1, pagesize+1)
	if not dbres then
		lcommon:mysql_close(db)
		libmqttkit.log(ngx.ERR,"bad result: ", err, ": ", errcode, ": ", sqlstate, ".")
		return
	end
	--local strbuf='';
	ngx.print(lutility.writeUint32(#dbres))
	for i,res in pairs(dbres) do
		ngx.print(lutility.writeUint64(tonumber(res.val)))
		--strbuf=strbuf..res.val..','
	end

	lcommon:mysql_close(db)
	libmqttkit.log(ngx.INFO,"pid:",pid,", pIdx:",pIdx,", numset total:",#dbres)
else
	ngx.exit(403)
end