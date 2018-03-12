local mqtt_util = require "nginx.mqtt"

local topicStore = ngx.shared.MQTT_TOPIC_DICT
local method_name = ngx.req.get_method()
local args = ngx.req.get_uri_args()
if args["ext"]~=nil then
	topicStore = ngx.shared.MQTT_TOPIC_EXT_DICT
end
local clientStore = ngx.shared.MQTT_CLIENT_DICT
--[[
local offsetTable = mqtt_util.shdict_get2mqtt("MQTT_TOPIC_DICT","G/2/#")
for topic, newoffset in pairs(offsetTable) do
	ngx.say(topic..":"..newoffset)
end
]]

function decode_utf8(                               -- Internal API
	input,index)  -- string

	local _length = string.byte(input, index) * 256 + string.byte(input, index+1)
	local output  = string.sub(input, index+2, index + _length + 1)

  return output,_length
end

if(method_name == "GET") then
	local funcName = args["method"]
	if funcName==nil then
		return
	end
	if funcName == "getTopicOffset" then
		if args["topic"]~=nil then
			local offsetTable = mqtt_util.shdict_get2mqtt("MQTT_TOPIC_DICT",args["topic"])
			ngx.say("success: "..table.maxn(offsetTable))
			for topic, newoffset in pairs(offsetTable) do
				ngx.say(topic.."="..newoffset)
				return
			end
			return
		end
		local keys = topicStore:get_keys()
		local msg="";
		for k, key in pairs(keys) do
			local val = topicStore:get(key)
			msg = msg..key.."="..val.."&"
		end
		ngx.print(msg)
	elseif funcName == "getConsumerOffset" then
		local cId = args["cId"]
		local buff=clientStore:get("_offset/"..cId)
		if buff ~= nil then
			local index=1
			local len=#buff
			while (index<len) do
				local topic,tlen = decode_utf8(buff,index)
				index = index+tlen+2
				local offset = MQTT.Utility.readUint32(buff,index)
				index = index + 4
				ngx.print("&",topic,"=",offset)
			end
		end
	end
	
elseif(method_name == "PUT") then
	ngx.req.read_body()
	local args = ngx.req.get_post_args()
	for key, val in pairs(args) do
		local oldVal = topicStore:get(key)
		if (oldVal == nil or oldVal < tonumber(val)) then
			--ngx.log(ngx.DEBUG, "ngx.shared.MQTT_TOPIC_DICT:set(key:",key,", val:",val,")")
			if string.find(key, "G/E/") ~= 1 then
				topicStore:set(key,tonumber(val))
			else
				topicStore:set(key,tonumber(val),604800)
			end
		else
			if string.find(key, "G/E/") ~= 1 then
				topicStore:set(key,tonumber(val))
			else
				topicStore:set(key,tonumber(val),604800)
			end
		end
	end
	ngx.print("success")
elseif(method_name == "POST") then
	if(topicStore:get("MQTT_RUNNING_FLG") ~= nil) then
		ngx.print("success")
		ngx.exit(ngx.HTTP_OK)
	end
	ngx.req.read_body()
	local args, err = ngx.req.get_post_args()
	if not args then
		libmqttkit.log(ngx.ERR,"failed to get post args: ", err)
		ngx.exit(ngx.HTTP_BAD_REQUEST)
	end
	for key, val in pairs(args) do
		if type(val) == "table" then
			libmqttkit.log(ngx.ERR,key, ": ", table.concat(val, ", "))
		else
			if string.find(key, "G/E/") ~= 1 then
				topicStore:set(key,tonumber(val))
			else
				topicStore:set(key,tonumber(val),604800)
			end
		end
	end
	ngx.print("success")
	topicStore:set("MQTT_RUNNING_FLG",1)
elseif(method_name == "DELETE") then

	for key, val in pairs(args) do
		topicStore:delete(key,val)
	end
	ngx.print("success")
else
	libmqttkit.log(ngx.ERR,"method_name:",method_name)
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end
