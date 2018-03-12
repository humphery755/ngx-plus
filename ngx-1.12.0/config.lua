
local _M = {
    _VERSION = '0.13'
}


local mt = { __index = _M }

local _data = {}
_M.GVAR={}
_M.GVAR.AREA_DEFAULT=1

_M.get=function(name,key)
	local tab=_data[name]
	if tab==nil then
		return nil
	end
	if key==nil then
		return tab
	end
    return tab[key]
end
_M.set=function(name,key,val)
	local tab=_data[name]
	if tab==nil then
		tab={}
		_data[name]=tab
	end
	if val==nil and tab[key]==nil then
		return
	end
	tab[key]=val
end

function _M.new(self, opts)
    return setmetatable({
		MQTT_TOPIC_SUB_PAGESIZE = 10,
		MQTT_SRV_REGIST_UPS_NAME = "http://127.0.0.1:1000",
		MQTT_SRV_REGIST_ADDR = "http://127.0.0.1:1000",
		REDIS_CLUSTER_ADDRESS = "127.0.0.1:6379",
        MYSQL_host = "127.0.0.1",
        MYSQL_port=3306,
        MYSQL_database="diversion",
        MYSQL_user="x",
        MYSQL_password="x",
        MYSQL_max_packet_size=1024 * 1024,
        MYSQL_conn_pool_size=50,
        MYSQL_conn_max_idle_timeout=10000,
        REDIS_host = "127.0.0.1",
        REDIS_port=6379,
        REDIS_keepalive_timeout=90000,
        REDIS_pool_size=1000,
        REDIS_connect_timeout=10000,
		DIV_config_admin_url = "http://127.0.0.1:6789",
		DIV_BACKEND_MAPPING={A="divs_a_ups",B="divs_b_ups"},
    }, mt)
end

return _M
