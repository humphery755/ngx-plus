#!/usr/bin/lua
--
-- example_00.lua
-- ~~~~~~~~~~~~~~
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
-- Subscribe to a topic and publish all received messages on another topic.
--
-- ToDo
-- ~~~~
-- - On failure, automatically reconnect to MQTT server.
-- - Error handling: MQTT.client.connect()
-- - Error handling: MQTT.client.destroy()
-- - Error handling: MQTT.client.disconnect()
-- - Error handling: MQTT.client.handler()
-- - Error handling: MQTT.client.publish()
-- - Error handling: MQTT.client.subscribe()
-- - Error handling: MQTT.client.unsubscribe()
-- ------------------------------------------------------------------------- --

function callback(
  topic,    -- string
  message)  -- string

  print("Topic: " .. topic .. ", message: '" .. message .. "'")

  mqtt_client:publish(args.topic_p, message)
end

-- ------------------------------------------------------------------------- --

function is_openwrt()
  return(os.getenv("USER") == "root")  -- Assume logged in as "root" on OpenWRT
end

-- ------------------------------------------------------------------------- --



args={
host="192.168.100.213",port=9000,id="P/sssss",topic_s="G/E/test"
}

local MQTT = require "paho.mqtt"

local mqtt_client = MQTT.client.create(args.host, args.port, callback)

mqtt_client:connect(args.id)

mqtt_client:subscribe({ args.topic_s })

local error_message = nil

while (error_message == nil) do
  error_message = mqtt_client:handler()
  socket.sleep(1.0)  -- seconds
end

if (error_message == nil) then
  mqtt_client:unsubscribe({ args.topic_s })
  mqtt_client:destroy()
else
  print(error_message)
end

-- ------------------------------------------------------------------------- --
