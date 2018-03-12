local _M = {}

local mt = { __index = _M }
-- virtual nodes number
local VIRTUAL_COUNT = 160;
-- sharding vaitual nodes number
local CONSISTENT_BUCKETS = 1024;

local crc32 = function(arg) return math.abs(ngx.crc32_long(arg)) end

local function chash_getUpsObj(self,upsName)
	local upsObj=self.G_CACHE[upsName]
	if upsObj== nil then
		upsObj={}
		upsObj["VIRTUAL_NODE"]={}
		upsObj["BUCKETS"]={}
		self.G_CACHE[upsName]=upsObj
	end
	return upsObj
end

--[[
-- add servers and to generate the 'BUCKETS'
--
-- @param {table} server all of the servers
-- ]]
function _M.add_server(self,upsName,server)
	local upsObj=chash_getUpsObj(self,upsName)
	local VIRTUAL_NODE=upsObj["VIRTUAL_NODE"]
	local BUCKETS=upsObj["BUCKETS"]
	for i,v in pairs(server) do
		for n=1,math.floor(VIRTUAL_COUNT) do
			local hash_key = v..'-'..(n-1);
			table.insert(VIRTUAL_NODE, {v, crc32(hash_key)});
		end
	end
	-- sorting by 'crc32(hash_key)', it means arg[2]
	table.sort(VIRTUAL_NODE, function (arg1, arg2)
		return (arg1[2] < arg2[2]);
	end);
	-- sharding
	local slice = math.floor(0xFFFFFFFF / CONSISTENT_BUCKETS);
	for i=1, CONSISTENT_BUCKETS do
		table.insert(BUCKETS, i, hash_find(self,upsName,math.floor(slice * (i -1)), 1, #VIRTUAL_NODE));
	end
end
--[[
-- Binary search
--
-- @param {float} key the value of we are looking for
-- @param {float} lo first index
-- @param {float} hi last index
-- @return {table} the node
--]]
function hash_find(self,upsName,key, lo, hi)
	local upsObj=self.G_CACHE[upsName]
	local VIRTUAL_NODE=upsObj["VIRTUAL_NODE"]
	if key <= VIRTUAL_NODE[lo][2] or key > VIRTUAL_NODE[hi][2] then
		return VIRTUAL_NODE[lo];
	end

	local middle = lo + math.floor((hi - lo) / 2);
	if middle == 1 then
		return VIRTUAL_NODE[middle];
	elseif key <=VIRTUAL_NODE[middle][2] and key > VIRTUAL_NODE[middle-1][2] then
		return VIRTUAL_NODE[middle];
	elseif key > VIRTUAL_NODE[middle][2] then
		return hash_find(self,upsName,key, middle+1, hi);
	end
	return hash_find(self,upsName,key, lo, middle-1);
end

--[[
-- get consistent hash value
--
-- @param {string} key
-- @return {string} node
--]]
function _M.get_upstream(self,upsName,key)
	local upsObj=chash_getUpsObj(self,upsName)
	local BUCKETS=upsObj["BUCKETS"]
	if #BUCKETS == 0 then
		return nil
	end
	return BUCKETS[(crc32(key) % CONSISTENT_BUCKETS) + 1][1];
end




local function chash_clean_upstream(self,upsName)
	local upsObj=chash_getUpsObj(self,upsName)
	upsObj["VIRTUAL_NODE"][{}]=nil
	upsObj["BUCKETS"][{}]=nil
	upsObj["VIRTUAL_NODE"]={}
	upsObj["BUCKETS"]={}
end
_M.clean_upstream = chash_clean_upstream

function _M.new(self, opts)
    return setmetatable({
		G_CACHE = {}
    }, mt)
end

return _M
