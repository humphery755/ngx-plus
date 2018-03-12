#!/usr/bin/lua
--
-- mqtt_subscribe.lua
-- ~~~~~~~~~~~~~~~~~~
-- Version: 0.2 2012-06-01
-- ------------------------------------------------------------------------- --
-- Copyright (c) 2011-2012 Geekscape Pty. Ltd.
-- All rights reserved. This program and the accompanying materials
-- are made available under the terms of the Eclipse Public License v1.0
-- which accompanies this distribution, and is available at
-- http://www.eclipse.org/legal/epl-v10.html
--
-- Contributors:
--    Andy Gelme - Initial implementation
-- -------------------------------------------------------------------------- --
--
-- Description
-- ~~~~~~~~~~~
-- Subscribe to an MQTT topic and display any received messages.
--
-- References
-- ~~~~~~~~~~
-- Lapp Framework: Lua command line parsing
--   http://lua-users.org/wiki/LappFramework
--
-- ToDo
-- ~~~~
-- None, yet.
-- ------------------------------------------------------------------------- --

local bit = require"bit"

function readUint32(b,offset)
	local s0 = bit.band(string.byte(b, offset), 0xff)
	local s1 = bit.band(string.byte(b, offset+1), 0xff)
	local s2 = bit.band(string.byte(b, offset+2), 0xff)
	local s3 = bit.band(string.byte(b, offset+3), 0xff)

	s0=bit.lshift(s0, 24)
	s1=bit.lshift(s1, 16)
	s2=bit.lshift(s2, 8)

	return bit.bor(s0,s1,s2,s3)
end

function callback(
  topic,    -- string
  message)  -- string
  local offset = readUint32(message, 1)
  local msg = string.sub(message,5,5+#message)
  local len = #msg
  if len > 65536 then
  	msg=string.sub(msg,1,1000).." ......* "
  end
  print("Topic: " .. topic .. ", offset: '" .. offset.. ", len: '" .. len..", message: '" .. msg .. "'")
end

-- ------------------------------------------------------------------------- --

function is_openwrt()
  return(os.getenv("USER") == "root")  -- Assume logged in as "root" on OpenWRT
end

-- ------------------------------------------------------------------------- --

print("[mqtt_subscribe v0.2 2012-06-01]")

local args={
host="192.168.100.213",port=9000,id="P/sssss",topic="G/2/test",debug=true,keepalive=650
}


local MQTT = require "paho.mqtt"

if (args.debug) then MQTT.Utility.set_debug(true) end

if (args.keepalive) then MQTT.client.KEEP_ALIVE_TIME = args.keepalive end

local mqtt_client = MQTT.client.create(args.host, args.port, callback)

if (args.will_message == "."  or  args.will_topic == ".") then
  mqtt_client:connect(args.id)
else
  mqtt_client:connect(
    args.id, args.will_topic, args.will_qos, args.will_retain, args.will_message
  )
end

mqtt_client:subscribe({args.topic})

local error_message = nil

while (error_message == nil) do
  error_message = mqtt_client:handler()
  socket.sleep(1.0)  -- seconds
end

if (error_message == nil) then
  mqtt_client:unsubscribe({args.topic})
  mqtt_client:destroy()
else
  print(error_message)
end

-- ------------------------------------------------------------------------- --
