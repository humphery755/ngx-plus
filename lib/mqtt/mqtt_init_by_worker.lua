local libredis3 = require "redis3"
local mqtt_util = require "nginx.mqtt"
local libnew_timer = require "new_timer"


MQTT.set(MQTT.GVAR.AREA_DEFAULT,"serverId",mqtt_util.get_serverId())


-----------------------------------------------------------------------------------
-- libredis3
-----------------------------------------------------------------------------------

redisCluster = libredis3:newCluster()
redisCluster:set_timeout(30000)

local function redis3_handler()
	redisCluster:sync_slots(MQTT.configure.REDIS_CLUSTER_ADDRESS)
end

local redis3_timer=libnew_timer:new{
	interval=60,
	callback_fn=redis3_handler,
}

-----------------------------------------------------------------------------------
-- process_subscribe
-----------------------------------------------------------------------------------
local libnotify_2worker_module = require "mqtt.mqtt_notify_2worker_module"
local notify_2worker_module = libnotify_2worker_module:new()

notify_2worker_module:startup(0)

-----------------------------------------------------------------------------------
-- process_srvchg_module
-----------------------------------------------------------------------------------
local libsrvchg_module = require "mqtt.mqtt_srvchg_module"
local srvchg_module = libsrvchg_module:new{
	SRV_STATUS_UPSTREAM_INFO=MQTT.configure.MQTT_SRV_REGIST_UPS_NAME,
	SRV_STATUS_SHM_NAME="MQTT_SRV_STATUS_DICT",
	SRV_STATUS_GET_URL=MQTT.configure.MQTT_SRV_REGIST_ADDR
}

srvchg_module:startup(0)


