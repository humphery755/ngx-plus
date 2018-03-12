--curl --data @test.json http://localhost:8000/admin/action/test

local args = ngx.req.get_uri_args()
ngx.req.read_body()
local buffer = ngx.req.get_body_data()
local cjson = require "cjson"
local divRules = cjson.decode(buffer)

local dictSysconfig = ngx.shared.DICT_SYSCONFIG

local buff=lcommon.writeUint16(#divRules)
local conditionals
-- action=0:upstream,1:redirect,2:rewrite
for i,g in pairs(divRules) do  
  conditionals=g.conditionals
  buff=buff..lcommon.encode_utf8(g.uri)..lcommon.writeUint16(g.action)..lcommon.encode_utf8(g.backend)..lcommon.writeUint16(#conditionals)
  for i,rule in pairs(conditionals) do
    local type=rule.type
    local data=rule.data
    local dlen=#data
    
    buff=buff..lcommon.encode_utf8(type)..lcommon.writeUint16(dlen)
    if "iprange" == type or "aidrange" == type or "cidrange" == type then
      for i,range in pairs(data) do
        buff=buff..lcommon.writeUint64(range["start"])..lcommon.writeUint64(range["end"])
      end
    elseif "ipset" == type or "aidset" == type or "cidset" == type then
      for i,set in pairs(data) do
        buff=buff..lcommon.writeUint64(set)
      end
    elseif "aidsuffix" == type or "ocdprefix" == type then
      for i,str in pairs(data) do
        buff=buff..lcommon.encode_utf8(str)
      end
    end
  end
  dictSysconfig:set("DIV_RULES",buff)
  
  local runtimeInfo=lcommon.writeUint16(1)
  dictSysconfig:set("DIV_runtimeInfo",runtimeInfo)
end

local rulebuf = dictSysconfig:get("DIV_RULES")
local divRules={}
local index=1
local rulelen=lcommon.readUint16(rulebuf,index)
index=index+2
for i=1, rulelen, 1 do
	local g={}
	local uri,len=lcommon.decode_utf8(rulebuf,index)
	index=index+len+2
	local action=lcommon.readUint16(rulebuf,index)
	index=index+2
	local backend,len=lcommon.decode_utf8(rulebuf,index)
	index=index+len+2
	local conditionalslen=lcommon.readUint16(rulebuf,index)
	index=index+2
	g.uri=uri
	g.action=action
	g.backend=backend
	local conditionals={}
	g.conditionals=conditionals
	for i=1, conditionalslen, 1 do
		local type,len=lcommon.decode_utf8(rulebuf,index)
		index=index+len+2
		local dlen=lcommon.readUint16(rulebuf,index)
		index=index+2
		local data={}
		if "iprange" == type or "aidrange" == type or "cidrange" == type then
		  for i=1, dlen, 1 do
			local range={}
			range["start"]=lcommon.readUint64(rulebuf,index)
			index=index+8
			range["end"]=lcommon.readUint64(rulebuf,index)
			index=index+8
			table.insert(data,range)
		  end
		elseif "ipset" == type or "aidset" == type or "cidset" == type then
		  for i=1, dlen, 1 do
			local set=lcommon.readUint64(rulebuf,index)
			index=index+8
			table.insert(data,set)
		  end
		elseif "aidsuffix" == type or "ocdprefix" == type then
		  for i=1, dlen, 1 do
			local str,len=lcommon.decode_utf8(rulebuf,index)
			index=index+len+2
			table.insert(data,str)
		  end
		else
			ngx.log(ngx.ERR,"no supper type: \"", type,"\"")
			goto continue
		end
		local rule={}
		rule.type=type
		rule.data=data
		table.insert(conditionals,rule)
	::continue::	
	end
	table.insert(divRules,g)
end

ngx.log(ngx.DEBUG,cjson.encode(divRules))
