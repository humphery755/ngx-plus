local libredis = require "resty.redis"
local mysql = require "resty.mysql"
local libck = require "resty.cookie"
local libipparser = require "diversion.userinfo.ipParser"
local lutility = require "lib.Utility"
local cjson = require "cjson"

local ffi = require("ffi")
local _M = {}

local mt = { __index = _M }


function _M.mysql_open()
  local db, err = mysql:new()
  if not db then
		libmqttkit.log(ngx.ERR,"failed to instantiate mysql: ", err)
      return
  end

  db:set_timeout(30000) -- 1 sec 1000

  local ok, err, errcode, sqlstate = db:connect{
      host = lconfig.MYSQL_host,
      port = lconfig.MYSQL_port,
      database = lconfig.MYSQL_database,
      user = lconfig.MYSQL_user,
      password = lconfig.MYSQL_password,
      max_packet_size = lconfig.MYSQL_max_packet_size }

  if not ok then
		libmqttkit.log(ngx.ERR,"failed to connect: ",lconfig.MYSQL_host,":",lconfig.MYSQL_port,", reason: ", err, ": ", errcode, " ", sqlstate)
      ngx.exit(500)
  end
  libmqttkit.log(ngx.DEBUG,"connected to mysql.")
  return db
end
function _M.mysql_close(self,db)
  local ok, err = db:set_keepalive(lconfig.MYSQL_conn_max_idle_timeout, lconfig.MYSQL_conn_pool_size)
  if not ok then
		libmqttkit.log(ngx.ERR,"failed to set keepalive: ", err)
			db:close()
			return;
  end
  libmqttkit.log(ngx.DEBUG,"closed connected.")
end

function _M.redis_open(self,key)
	local redis = libredis:new()
  redis:set_timeout(1000)
  local ok, err = redis:connect(lconfig.REDIS_host, lconfig.REDIS_port)
  if not ok then
		libmqttkit.log(ngx.ERR,"failed to connect: ",lconfig.REDIS_host,":",lconfig.REDIS_port,", reason: ", err)
      return nil
  end
  return redis, err
end

function _M.redis_close(self,redis)
  return redis:set_keepalive(10000, 100)
end


function _M.redis_get(self,key)
	local redis = libredis:new()
  redis:set_timeout(1000)
  local ok, err = redis:connect(lconfig.REDIS_host, lconfig.REDIS_port)
  if not ok then
		libmqttkit.log(ngx.ERR,"failed to connect: ",lconfig.REDIS_host,":",lconfig.REDIS_port,", reason: ", err)
      return nil
  end
  local res, err = redis:get(key)
  if not res then
		libmqttkit.log(ngx.ERR,"failed to get key: "..key,", ", err)
  end
  local ok, err = redis:set_keepalive(10000, 100)
  if not ok then
      ngx.say("failed to set keepalive: ", err)
  end
  return res, err
end
function _M.redis_set(self,key,value,ttl)
	local redis = libredis:new()
  redis:set_timeout(1000)
  local ok, err = redis:connect(lconfig.REDIS_host, lconfig.REDIS_port)
  if not ok then
		libmqttkit.log(ngx.ERR,"failed to connect: ",lconfig.REDIS_host,":",lconfig.REDIS_port,", reason: ", err)
      return ok, err
  end
  ok, err = redis:set(key,value)
  if not ok then
		libmqttkit.log(ngx.ERR,"failed to set key: ",key,", ", err)
      redis:set_keepalive(10000, 100)
      return ok, err
  end
  local ok, err = redis:set_keepalive(10000, 100)
  if not ok then
      ngx.say("failed to set keepalive: ", err)
  end
  return ok, err
end



function _M.encode_runtimeInfo(runtimeInfo,diversionHost)
	if runtimeInfo == nil then
		libmqttkit.log(ngx.ERR,"runtimeInfo can't null")
		return nil
	end
	local src,to;
	src=runtimeInfo.src;
	to=runtimeInfo.to;
	if src==nil then
		src=""
	end
	if to==nil then
		to=""
	end
	local runtimeInfoBuf=lutility.writeUint16(runtimeInfo["status"])..lutility.writeUint32(runtimeInfo["ver"])..lutility.encode_utf8(src)..lutility.encode_utf8(to)..lutility.writeUint32(runtimeInfo["fid"])
	local count=0
	if diversionHost == nil then
		return runtimeInfoBuf..lutility.writeUint16(count)
	end

	for e,h in pairs(diversionHost) do
		count=count+1
	end
	runtimeInfoBuf=runtimeInfoBuf..lutility.writeUint16(count)
	for e,h in pairs(diversionHost) do
		runtimeInfoBuf=runtimeInfoBuf..lutility.encode_utf8(e)..lutility.encode_utf8(h)
	end
	return runtimeInfoBuf
end

function _M.decode_runtimeInfo(runtimeInfoBuf)
	local runtimeInfo = {}
	local diversionHost = {}
	local index = 1
	runtimeInfo["status"] = lutility.readUint16(runtimeInfoBuf,index);
	index = index + 2
	runtimeInfo["ver"] = lutility.readUint32(runtimeInfoBuf,index);
	index = index + 4
	local src,len = lutility.decode_utf8(runtimeInfoBuf,index);
	index = index + len +2
	local to,len = lutility.decode_utf8(runtimeInfoBuf,index);
	index = index + len +2
	runtimeInfo.src = src;
	runtimeInfo.to = to;
	runtimeInfo["fid"] = lutility.readUint32(runtimeInfoBuf,index);
	index = index + 4
	local vhlen =  lutility.readUint16(runtimeInfoBuf,index);
	index = index + 2
	if vhlen > 0 then
		for i=1, vhlen, 1 do
			local key,len = lutility.decode_utf8(runtimeInfoBuf,index);
			index = index + len +2
			local val,len = lutility.decode_utf8(runtimeInfoBuf,index);
			index = index + len +2
			diversionHost[key]=val;
		end
	end
	return runtimeInfo,diversionHost
end

function _M.get_userInfo()
	local userInfo={}
	local cookie, err = libck:new()
	if not cookie then
		libmqttkit.log(ngx.ERR, err)
	    return
	end

	-- get single cookie
	local field, err = cookie:get("aid")
	if not field then
	    userInfo.aid=tonumber(field)
	end
	field, err = cookie:get("cid")
	if not field then
	    userInfo.cid=tonumber(field)
	end
	field, err = cookie:get("oid")
	if not field then
	    userInfo.oid=tonumber(field)
	end
	userInfo.ip=libipparser.get();
	userInfo.uri=ngx.var.uri
	return userInfo
end

function _M.decode_policy(rulebuf)  -- string
	
	local policys={}
	local index=1
	local policyslen=lutility.readUint16(rulebuf,index)
	index=index+2
	for i=1, policyslen, 1 do
		local rule={}
		rule.id=lutility.readUint32(rulebuf,index)
		index=index+4
		local type,len=lutility.decode_utf8(rulebuf,index)
		index=index+len+2
		local bitset=lutility.readUint16(rulebuf,index)
		index=index+2
		local dlen=lutility.readUint16(rulebuf,index)
		index=index+2
		local data={}
		if "iprange" == type or "aidrange" == type or "cidrange" == type then
			if dlen<=0 then
				libmqttkit.log(ngx.ERR,"type: \"", type,"\" datalen is 0")
				return nil
			end
			for i=1, dlen, 1 do
			local range={}
			range["start"]=lutility.readUint64(rulebuf,index)
			index=index+8
			range["end"]=lutility.readUint64(rulebuf,index)
			index=index+8
			table.insert(data,range)
			end
		elseif "aidset" == type or "cidset" == type then
			for i=1, dlen, 1 do
			local set=lutility.readUint64(rulebuf,index)
			index=index+8
			table.insert(data,set)
			end
		elseif "ipset" == type or "oidset" == type or "aidsuffix" == type or "cidsuffix" == type then
			if dlen<=0 then
				libmqttkit.log(ngx.ERR,"type: \"", type,"\" datalen is 0")
				return nil
			end
			for i=1, dlen, 1 do
			local set=lutility.readUint32(rulebuf,index)
			index=index+4
			table.insert(data,set)
			end
		else
			libmqttkit.log(ngx.ERR,"no supper type: \"", type,"\"")
			return nil
		end
		
		rule.type=type
		rule.data=data
		table.insert(policys,rule)
	::continue::	
	end
	libmqttkit.log(ngx.DEBUG,"***********************",cjson.encode(policys))
	local divRules={}
	local rulelen=lutility.readUint16(rulebuf,index)
	
	index=index+2
	for i=1, rulelen, 1 do
		local g={}
		local uri,len=lutility.decode_utf8(rulebuf,index)
		index=index+len+2
		local action=lutility.readUint16(rulebuf,index)
		index=index+2
		local backend,len=lutility.decode_utf8(rulebuf,index)
		index=index+len+2
		local text,len=lutility.decode_utf8(rulebuf,index)
		index=index+len+2
		local plen=lutility.readUint16(rulebuf,index)
		index=index+2
		g.uri=uri
		g.action=action
		g.backend=backend
		g.text=text
		g.policys={}
		libmqttkit.log(ngx.DEBUG,uri,",",action,",",backend,",",text,",",plen)
		for i=1, plen, 1 do
			local pid=lutility.readUint32(rulebuf,index)
			index=index+4
			for _,policy in pairs(policys) do
				if policy.id==pid then					
					table.insert(g.policys,pid)
					--libmqttkit.log(ngx.DEBUG,"***********************",pid)
					break
				end
			end			
		end
		--libmqttkit.log(ngx.DEBUG,"***********************",cjson.encode(g))
		table.insert(divRules,g)
	end
	libmqttkit.log(ngx.DEBUG,cjson.encode(divRules))
	return divRules
end

function _M.encode_policy(divRules)  -- table
	if divRules == nil or #divRules == 0 then
		libmqttkit.log(ngx.ERR,"policy group can't null")
		return nil
	end
	local buff=lutility.writeUint16(#divRules)
	local policys
	-- action=0:upstream,1:redirect,2:rewrite
	for i,g in pairs(divRules) do  
	  policys=g.policys
	  local text=g.text
	  if text == nil then
		text=""
	  end
	  if #policys == 0 then
			libmqttkit.log(ngx.ERR,"policy can't null")
		return nil
	  end
	  buff=buff..lutility.encode_utf8(g.uri)..lutility.writeUint16(g.action)..lutility.encode_utf8(g.backend)..lutility.encode_utf8(text)..lutility.writeUint16(#policys)
	  for i,rule in pairs(policys) do
	    local type=rule.type
	    local data=rule.data
	    local dlen=#data
	    
	    buff=buff..lutility.writeUint32(rule.id)..lutility.encode_utf8(type)..lutility.writeUint16(dlen)
	    if "iprange" == type or "aidrange" == type or "cidrange" == type then
				if #data<=0 then
					libmqttkit.log(ngx.ERR,"type: \"", type,"\" datalen is 0")
					return nil
				end
	      for i,range in pairs(data) do
		buff=buff..lutility.writeUint64(range["start"])..lutility.writeUint64(range["end"])
	      end
	    elseif "aidset" == type or "cidset" == type then
	      for i,set in pairs(data) do
		--buff=buff..lutility.writeUint64(set)
	      end
	    elseif "ipset" == type or "oidset" == type or "aidsuffix" == type or "cidsuffix" == type then
				if #data<=0 then
					libmqttkit.log(ngx.ERR,"type: \"", type,"\" datalen is 0")
					return nil
				end
	      for i,str in pairs(data) do
		buff=buff..lutility.writeUint32(str)
	      end
	    else
				libmqttkit.log(ngx.ERR,"no supper type: \"", type,"\"")
		return nil
	    end
	  end
	end
	return buff
end


function _M.new(self, opts)
    return setmetatable({
		G_CACHE = {}
    }, mt)
end

return _M