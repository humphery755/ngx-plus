---
-- @module mqtt_library
-- ~~~~~~~~~~~~~~~~
-- Version: 0.3 2014-10-06
-- -------------------------------------------------------------------------- --
-- Copyright (c) 2011-2012 Geekscape Pty. Ltd.
-- All rights reserved. This program and the accompanying materials
-- are made available under the terms of the Eclipse Public License v1.0
-- which accompanies this distribution, and is available at
-- http://www.eclipse.org/legal/epl-v10.html
--
-- Contributors:
--    Andy Gelme    - Initial API and implementation
--    Kevin KIN-FOO - Authentication and rockspec
-- -------------------------------------------------------------------------- --
--
-- Documentation
-- ~~~~~~~~~~~~~
-- Paho MQTT Lua website
--   http://eclipse.org/paho/
--
-- References
-- ~~~~~~~~~~
-- MQTT Community
--   http://mqtt.org

-- MQTT protocol specification 3.1
--   https://www.ibm.com/developerworks/webservices/library/ws-mqtt
--   http://mqtt.org/wiki/doku.php/mqtt_protocol   # Clarifications
--
-- Notes
-- ~~~~~
-- - Always assumes MQTT connection "clean session" enabled.
-- - Supports connection last will and testament message.
-- - Does not support connection username and password.
-- - Fixed message header byte 1, only implements the "message type".
-- - Only supports QOS level 0.
-- - Maximum payload length is 268,435,455 bytes (as per specification).
-- - Publish message doesn't support "message identifier".
-- - Subscribe acknowledgement messages don't check granted QOS level.
-- - Outstanding subscribe acknowledgement messages aren't escalated.
-- - Works on the Sony PlayStation Portable (aka Sony PSP) ...
--     See http://en.wikipedia.org/wiki/Lua_Player_HM
--
-- ToDo
-- ~~~~
-- * Consider when payload needs to be an array of bytes (not characters).
-- * Maintain both "last_activity_out" and "last_activity_in".
-- * - http://mqtt.org/wiki/doku.php/keepalive_for_the_protocol
-- * Update "last_activity_in" when messages are received.
-- * When a PINGREQ is sent, must check for a PINGRESP, within KEEP_ALIVE_TIME..
--   * Otherwise, fail the connection.
-- * When connecting, wait for CONACK, until KEEP_ALIVE_TIME, before failing.
-- * Should MQTT.protocol:connect() be asynchronous with a callback ?
-- * Review all public APIs for asynchronous callback behaviour.
-- * Implement parse PUBACK message.
-- * Handle failed subscriptions, i.e no subscription acknowledgement received.
-- * Fix problem when KEEP_ALIVE_TIME is short, e.g. mqtt_publish -k 1
--     MQTT.protocol:handler(): Message length mismatch
-- - On socket error, optionally try reconnection to MQTT server.
-- - Consider use of assert() and pcall() ?
-- - Only expose public API functions, don't expose internal API functions.
-- - Refactor "if self.connected()" to "self.checkConnected(error_message)".
-- - Maintain and publish messaging statistics.
-- - Memory heap/stack monitoring.
-- - When debugging, why isn't mosquitto sending back CONACK error code ?
-- - Subscription callbacks invoked by topic name (including wildcards).
-- - Implement asynchronous state machine, rather than single-thread waiting.
--   - After CONNECT, expect and wait for a CONACK.
-- - Implement complete MQTT broker (server).
-- - Consider using Copas http://keplerproject.github.com/copas/manual.html
-- ------------------------------------------------------------------------- --


local bit = require"bit"
local semaphore = require "ngx.semaphore"



local MQTT = {}

---
-- @field [parent = #mqtt_library] utility#utility Utility
--
MQTT.Utility = {}

---
-- @field [parent = #mqtt_library] #number VERSION
--
MQTT.VERSION = 0x03

---
-- @field [parent = #mqtt_library] #boolean ERROR_TERMINATE
--
MQTT.ERROR_TERMINATE = false      -- Message handler errors terminate process ?

---
-- @field [parent = #mqtt_library] #string DEFAULT_BROKER_HOSTNAME
--
MQTT.DEFAULT_BROKER_HOSTNAME = "m2m.eclipse.org"

---
-- An MQTT protocol
-- @type protocol

---
-- @field [parent = #mqtt_library] #protocol protocol
--
MQTT.protocol = {}
MQTT.protocol.__index = MQTT.protocol

---
-- @field [parent = #protocol] #number DEFAULT_PORT
--
MQTT.protocol.DEFAULT_PORT       = 1883

---
-- @field [parent = #protocol] #number KEEP_ALIVE_TIME
--
MQTT.protocol.KEEP_ALIVE_TIME    =   60  -- seconds (maximum is 65535)

---
-- @field [parent = #protocol] #number MAX_PAYLOAD_LENGTH
--
MQTT.protocol.MAX_PAYLOAD_LENGTH = 268435455 -- bytes

-- MQTT 3.1 Specification: Section 2.1: Fixed header, Message type

---
-- @field [parent = #mqtt_library] message
--
local _data = {}

MQTT.message = {}
MQTT.message.TYPE_RESERVED1   = 0x00
MQTT.message.TYPE_CONNECT     = 0x01
MQTT.message.TYPE_CONACK      = 0x02
MQTT.message.TYPE_PUBLISH     = 0x03
MQTT.message.TYPE_PUBACK      = 0x04
MQTT.message.TYPE_PUBREC      = 0x05
MQTT.message.TYPE_PUBREL      = 0x06
MQTT.message.TYPE_PUBCOMP     = 0x07
MQTT.message.TYPE_SUBSCRIBE   = 0x08
MQTT.message.TYPE_SUBACK      = 0x09
MQTT.message.TYPE_UNSUBSCRIBE = 0x0a
MQTT.message.TYPE_UNSUBACK    = 0x0b
MQTT.message.TYPE_PINGREQ     = 0x0c
MQTT.message.TYPE_PINGRESP    = 0x0d
MQTT.message.TYPE_DISCONNECT  = 0x0e
MQTT.message.TYPE_RESERVED	  = 0x0f


MQTT.message.VERSION_3_1 = 3
MQTT.message.VERSION_3_1_1 = 4
-- MQTT 3.1 Specification: Section 3.2: CONACK acknowledge connection errors
-- http://mqtt.org/wiki/doku.php/extended_connack_codes

MQTT.CONACK = {}
MQTT.CONACK.CONNECTION_ACCEPTED 			= 0x00
MQTT.CONACK.UNNACEPTABLE_PROTOCOL_VERSION 	= 0x01
MQTT.CONACK.IDENTIFIER_REJECTED 			= 0x02
MQTT.CONACK.SERVER_UNAVAILABLE 				= 0x03
MQTT.CONACK.BAD_USERNAME_OR_PASSWORD 		= 0x04
MQTT.CONACK.NOT_AUTHORIZED 					= 0x05
MQTT.CONACK.error_message = {          -- CONACK return code used as the index
  "Unacceptable protocol version",
  "Identifer rejected",
  "Server unavailable",
  "Bad user name or password",
  "Not authorized"
--"Invalid will topic"                 -- Proposed
}
MQTT.TYPE_RESERVED = {}
MQTT.TYPE_RESERVED.KICK 					= 0x01
MQTT.TYPE_RESERVED.SYNC 					= 0x02


MQTT.GVAR = {}

MQTT.GVAR.AREA_DEFAULT=1
MQTT.GVAR.AREA_SUBSCRIBEMAP=2


MQTT.get=function(name,key)
	local tab=_data[name]
	if tab==nil then
		return nil
	end
	if key==nil then
		return tab
	end
    return tab[key]
end
MQTT.set=function(name,key,val)
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


MQTT.Utility.tableprint = function(data)
    local cstring = "{"
    if(type(data)=="table") then
		local cs = ""
        for k, v in pairs(data) do
			if (#cs > 2) then
				cs  = cs..", "
			end
            if(type(v)=="table") then
                cs = cs..tostring(k).."=" .. MQTT.Utility.tableprint(v)
			else
				cs  = cs..tostring(k).."="..tostring(v)
            end
        end
		cstring = cstring .. cs
    else
        cstring = cstring .. tostring(data)
    end
    cstring = cstring .."}"
	return cstring
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Create an MQTT protocol instance
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

---
-- Create an MQTT protocol instance.
-- @param #string hostname Host name or address of the MQTT broker
-- @param #number port Port number of the MQTT broker (default: 1883)
-- @param #function callback Invoked when subscribed topic messages received
-- @function [parent = #protocol] create
-- @return #protocol created protocol
--
function MQTT.protocol.create(wb)  -- function: Invoked when subscribed topic messages received
             -- return:   mqtt_protocol table

  local mqtt_protocol = {}

  setmetatable(mqtt_protocol, MQTT.protocol)

  mqtt_protocol.destroyed     = false
  mqtt_protocol.websock = wb
  mqtt_protocol.clientStore = nil
  mqtt_protocol.clientTmpStore = nil
  mqtt_protocol.clientMsgIdStore = nil
  mqtt_protocol.subsMap = {}
  mqtt_protocol.connected = true
  mqtt_protocol.topicOffsetMap={}
  return(mqtt_protocol)
end

--------------------------------------------------------------------------------
-- Specify username and password before #protocol.connect
--
-- If called with empty _username_ or _password_, connection flags will be set
-- but no string will be appended to payload.
--
-- @function [parent = #protocol] auth
-- @param self
-- @param #string username Name of the user who is connecting. It is recommended
--                         that user names are kept to 12 characters.
-- @param #string password Password corresponding to the user who is connecting.
function MQTT.protocol.auth(self, username, password)
  -- When no string is provided, remember current call to set flags
  self.username = username or true
  self.password = password or true
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Destroy an MQTT protocol instance.
-- @param self
-- @function [parent = #protocol] destroy
--
function MQTT.protocol:destroy()                                    -- Public API
  ngx.log(ngx.DEBUG,self.clientId)

  if (self.destroyed == false) then
    self.destroyed = true         -- Avoid recursion when message_write() fails

    if (self.connected) then
		if(self.websock ~= nil) then
			self.websock:send_close()
			self.websock = nil
		end
		self.connected = false
	end

	local sessionBtyes = self.clientTmpStore:get(self.clientId)
	if sessionBtyes~= nil then
		local old_connect,oc_len = MQTT.protocol.decode_utf8(sessionBtyes,2)
		if old_connect~= nil and old_connect == self.last_connect then
			self.clientTmpStore:delete(self.clientId)
		end
	end

	self:backupConsumerOffset()
	self:backupSubscribes()
    --self.outstanding = nil
	for key,buffer in pairs(self.subsMap) do
		self:deleteSubscribe(key)
	end
	self.subsMap[{}]=nil
	self.subsMap=nil
	MQTT.set(MQTT.GVAR.AREA_SUBSCRIBEMAP,self.clientId,nil)
  end
end

function MQTT.protocol:setOutstanding(key,val)
	if val == nil then
		self.outstanding:delete(self.clientId .. "/" .. key)
		--ngx.log(ngx.DEBUG,self.clientId,", delete:",self.clientId .. "/" .. key)
	else
		self.outstanding:set(self.clientId .. "/" .. key,val,180) -- exptime: 180 seconds
		--ngx.log(ngx.DEBUG,self.clientId,", set:",self.clientId .. "/" .. key)
	end
end

function MQTT.protocol:getOutstanding(key)
	return self.outstanding:get(self.clientId .. "/" .. key)
end

function MQTT.protocol:storeConsumerOffset(topic,offset)
	local topicKey="/"..topic
	self.topicOffsetMap[topicKey] = offset
	ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", offset:",offset)
end

function MQTT.protocol:getConsumerOffset(topic)
  	local topicKey="/"..topic
	return self.topicOffsetMap[topicKey]
end

function MQTT.protocol:backupConsumerOffset()
	local buff=nil
	for topic,offset in pairs(self.topicOffsetMap) do
		local Subscribe = self.subsMap[topic]
		if Subscribe ~= nil and Subscribe.qos>0 then
			ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", offset:",offset)
			if buff == nil then
				buff=self.encode_utf8(tostring(topic))..MQTT.Utility.uint32ToBytes(offset)
			else
				buff=buff..self.encode_utf8(tostring(topic))..MQTT.Utility.uint32ToBytes(offset)
			end
		end
	end
	if buff ~= nil then
		self.clientStore:set("_offset/"..self.clientId,buff)
	end
end

function MQTT.protocol:recoverConsumerOffset()
	local buff=self.clientStore:get("_offset/"..self.clientId)
	if buff ~= nil then
		local index=1
		local len=#buff
		while (index<len) do
			local topic,tlen = self.decode_utf8(buff,index)
			index = index+tlen+2
			local offset = MQTT.Utility.readUint32(buff,index)
			index = index + 4
			self.topicOffsetMap[topic]=offset
			ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", offset:",offset)
		end
	end
end

function MQTT.protocol:clearConsumerOffset(topic)
	self.clientStore:delete("_offset/"..self.clientId)
end

function MQTT.protocol:storeSubscribe(topic,Subscribe)
	ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", min:",Subscribe.min,", qos:",Subscribe.qos)
	self.subsMap[topic] = Subscribe
end

function MQTT.protocol:deleteSubscribe(topic)
	local Subscribe=self.subsMap[topic]
	if(Subscribe==nil)then return end
	Subscribe.qos = nil
	Subscribe.topicFilter = nil
	Subscribe.min=nil
	Subscribe[{}]=nil
	self.subsMap[topic] = nil
end

function MQTT.protocol:backupSubscribes()
  local buff=nil
  for topic,Subscribe in pairs(self.subsMap) do
	  local it, err = ngx.re.gmatch(topic, "(\\+|\\#)")
	  local m, err = it()
	  if not m then
		  local min = self:getConsumerOffset(topic)
		  if(min ~= nil and min>0) then
			  Subscribe.min = min
		  end
	  end
	  if Subscribe.qos > 0 then
		  ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", min:",Subscribe.min,", qos:",Subscribe.qos)
		  local tmpbuff = self.encode_utf8(Subscribe.topicFilter)..MQTT.Utility.uint32ToBytes(Subscribe.min)
		  tmpbuff = tmpbuff..MQTT.Utility.uint32ToBytes(Subscribe.qos)
		  if buff == nil then
			  buff=tmpbuff
		  else
			  buff=buff..tmpbuff
		  end
	  end
  end
  if buff ~= nil then
	  self.clientStore:set("_subs/"..self.clientId,buff)
  end
end

function MQTT.protocol:recoverSubscribes()
  local buff=self.clientStore:get("_subs/"..self.clientId)
  if buff ~= nil then
	  local index=1
	  local len=#buff
	  while (index<len) do
	  	  local Subscribe={}
		  local topic,tlen = self.decode_utf8(buff,index)
		  Subscribe.topicFilter=topic
		  index = index+tlen+2
		  Subscribe.min = MQTT.Utility.readUint32(buff,index)
		  index = index + 4
		  Subscribe.qos = MQTT.Utility.readUint32(buff,index)
		  index = index + 4
		  if(Subscribe.min>0)then
			local min = self:getConsumerOffset(topic)
			if(min ~= nil and min>0) then
				Subscribe.min = min
			end
		  end
		  self.subsMap[topic] = Subscribe
		  ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,", min:",Subscribe.min,", qos:",Subscribe.qos)
	  end
  end
end

function MQTT.protocol:clearSubscribes(topic)
  self.clientStore:delete("_subs/"..self.clientId)
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit MQTT Disconnect message.
-- MQTT 3.1 Specification: Section 3.14: Disconnect notification
-- bytes 1,2: Fixed message header, see MQTT.protocol:message_write()
-- @param self
-- @function [parent = #protocol] disconnect
--
function MQTT.protocol:disconnect()                                 -- Public API
  ngx.log(ngx.DEBUG,ngx.var.remote_addr," ",self.clientId)

  if (self.connected) then
    self:message_write(MQTT.message.TYPE_DISCONNECT, nil,0)
	if(self.websock ~= nil) then
		self.websock:send_close()
		self.websock = nil
	end
	self.connected = false
  end
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Encode a message string using UTF-8 (for variable header)
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 2.5: MQTT and UTF-8
--
-- byte  1:   String length MSB
-- byte  2:   String length LSB
-- bytes 3-n: String encoded as UTF-8

function MQTT.protocol.encode_utf8(                               -- Internal API
  input)  -- string

  local output
  output = string.char(math.floor(#input / 256))
  output = output .. string.char(#input % 256)
  output = output .. input

  return(output)
end

function MQTT.protocol.decode_utf8(                               -- Internal API
	input,index)  -- string

	local _length = string.byte(input, index) * 256 + string.byte(input, index+1)
	local output  = string.sub(input, index+2, index + _length + 1)

  return output,_length
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Handle received messages and maintain keep-alive PING messages.
-- This function must be invoked periodically (more often than the
-- `MQTT.protocol.KEEP_ALIVE_TIME`) which maintains the connection and
-- services the incoming subscribed topic messages
-- @param self
-- @function [parent = #protocol] handler
--
function MQTT.protocol:handler(buffer)                                    -- Public API

    if (buffer ~= nil and #buffer > 0) then
      local index = 1
      -- Parse individual messages (each must be at least 2 bytes long)
      -- Decode "remaining length" (MQTT v3.1 specification pages 6 and 7)

      while (index < #buffer) do
        local message_type_flags = string.byte(buffer, index)
        local multiplier = 1
        local remaining_length = 0

        repeat
          index = index + 1
          local digit = string.byte(buffer, index)
          remaining_length = remaining_length + ((digit % 128) * multiplier)
          multiplier = multiplier * 128
        until digit < 128                              -- check continuation bit

        local message = string.sub(buffer, index + 1, index + remaining_length)

        if (#message == remaining_length) then
          self:parse_message(message_type_flags, remaining_length, message)
        else
          ngx.log(ngx.ERR,self.clientId,
            ", Incorrect remaining length: " ..
            remaining_length .. " ~= message length: " .. #message
          )
        end

        index = index + remaining_length + 1
      end

      -- Check for any left over bytes, i.e. partial message received

      if (index ~= (#buffer + 1)) then
        local error_message =
          "MQTT.protocol:handler(): Partial message received" ..
          index .. " ~= " .. (#buffer + 1)

        if (MQTT.ERROR_TERMINATE) then         -- TODO: Refactor duplicate code
          self:destroy()
          error(error_message)
        else
          ngx.log(ngx.ERR,self.clientId,",",error_message)
        end
      end
    end

  return(nil)
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit an MQTT message
-- ~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 2.1: Fixed header
--
-- byte  1:   Message type and flags (DUP, QOS level, and Retain) fields
-- bytes 2-5: Remaining length field (between one and four bytes long)
-- bytes m- : Optional variable header and payload

function MQTT.protocol:message_write(                             -- Internal API
  message_type,  -- enumeration
  payload,qosLevel,dupFlag,retainFlag)       -- string
                 -- return: nil or error message

-- TODO: Complete implementation of fixed header byte 1

  local message = self:encodeCommonHeader(message_type,qosLevel,dupFlag,retainFlag)

  if (payload == nil) then
    message = message .. string.char(0)  -- Zero length, no payload
  else
    if (#payload > MQTT.protocol.MAX_PAYLOAD_LENGTH) then
      return(
        "MQTT.protocol:message_write(): Payload length = " .. #payload ..
        " exceeds maximum of " .. MQTT.protocol.MAX_PAYLOAD_LENGTH
      )
    end

    -- Encode "remaining length" (MQTT v3.1 specification pages 6 and 7)

    local remaining_length = #payload

    repeat
      local digit = remaining_length % 128
      remaining_length = math.floor(remaining_length / 128)
      if (remaining_length > 0) then digit = digit + 128 end -- continuation bit
      message = message .. string.char(digit)
    until remaining_length == 0

    message = message .. payload
  end
  local payload_len = #message

  local status, error_message = self.websock:send_binary(message)

  if not status then
	ngx.log(ngx.ERR,self.clientId, ", failed to send a binary frame: ", error_message,", message_type:",message_type)
    self:destroy()
    return("MQTT.protocol:message_write(): " .. error_message)
  end

  self.last_activity = ngx.time()
  return(nil)
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT message
-- ~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 2.1: Fixed header
--
-- byte  1:   Message type and flags (DUP, QOS level, and Retain) fields
-- bytes 2-5: Remaining length field (between one and four bytes long)
-- bytes m- : Optional variable header and payload
--
-- The message type/flags and remaining length are already parsed and
-- removed from the message by the time this function is invoked.
-- Leaving just the optional variable header and payload.

function MQTT.protocol:parse_message(                             -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string: Optional variable header and payload

  local message_type = bit.rshift(message_type_flags, 4)

-- TODO: MQTT.message.TYPE table should include "parser handler" function.
--       This would nicely collapse the if .. then .. elseif .. end.

  if (message_type == MQTT.message.TYPE_CONNECT) then
    self:parse_message_connect(message_type_flags, remaining_length, message)
  elseif (message_type == MQTT.message.TYPE_CONACK) then
    self:parse_message_conack(message_type_flags, remaining_length, message)
  elseif (message_type == MQTT.message.TYPE_SUBSCRIBE) then
    self:parse_message_subscribe(message_type_flags, remaining_length, message)
  elseif (message_type == MQTT.message.TYPE_PUBLISH) then
    self:parse_message_publish(message_type_flags, remaining_length, message)
  elseif (message_type == MQTT.message.TYPE_PUBACK) then
	self:parse_message_puback(message_type_flags, remaining_length, message)
  elseif (message_type == MQTT.message.TYPE_SUBACK) then
    self:parse_message_suback(message_type_flags, remaining_length, message)

  elseif (message_type == MQTT.message.TYPE_UNSUBSCRIBE) then
    self:parse_message_unsubscribe(message_type_flags, remaining_length, message)

  elseif (message_type == MQTT.message.TYPE_UNSUBACK) then
    self:parse_message_unsuback(message_type_flags, remaining_length, message)

  elseif (message_type == MQTT.message.TYPE_PINGREQ) then
    self:ping_response()

  elseif (message_type == MQTT.message.TYPE_PINGRESP) then
    self:parse_message_pingresp(message_type_flags, remaining_length, message)

  elseif (message_type == MQTT.message.TYPE_DISCONNECT) then
	self:disconnect()
  elseif (message_type == MQTT.message.TYPE_RESERVED) then
	self:parse_message_reserved(message_type_flags, remaining_length, message)
  else
    local error_message =
      "MQTT.protocol:parse_message(): Unknown message type: " .. message_type

    if (MQTT.ERROR_TERMINATE) then             -- TODO: Refactor duplicate code
      self:destroy()
      error(error_message)
    else
      ngx.log(ngx.ERR,error_message)
    end
  end
end

function MQTT.protocol:genericDecodeCommonHeader(message_type_flags)
	local h1 = message_type_flags
	self.dupFlag = (bit.rshift(bit.band(h1,0x0008) , 3))
	self.dupFlag = (self.dupFlag == 1)

	self.qosLevel = (bit.rshift(bit.band(h1,0x0006) , 1))

	self.retainFlag = (bit.band(h1,0x0001))
	self.retainFlag = (self.retainFlag == 1)
	return 1
end

function MQTT.protocol:encodeCommonHeader(message_type,qosLevel,dupFlag,retainFlag)
	local flags = 0;
	if (dupFlag) then
		flags = bit.bor(flags,0x08)
	end
	if (retainFlag) then
		flags = bit.bor(flags,0x01)
	end
	flags = bit.bor(flags,bit.lshift(bit.band(qosLevel, 0x03),1));

	return string.char(bit.bor(bit.lshift(message_type, 4),flags))
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT CONNECT message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.2: CONNECT Acknowledge connection
--
-- byte 1: Reserved value
-- byte 2: Connect return code, see MQTT.CONNECT.error_message[]
function MQTT.protocol:parse_message_connect(                     -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  if (self.websock ~= nil) then
    if (remaining_length < 3) then
      error("MQTT.protocol:parse_message_connect(): Invalid remaining length: " .. remaining_length)
    end

    local protocolNameLen = string.byte(message, 1) * 256 + string.byte(message, 2)
    local protoName  = string.sub(message, 3, 2+protocolNameLen)
    local index  = protocolNameLen + 3

	if("MQIsdp" ~= tostring(protoName)) then
		error("MQTT.protocol:parse_message_connect(): Invalid protoName: " .. protoName)
	end

	local protocolVersion = string.byte(message, index)
	local connFlags = string.byte(message, index+1)
	index = index + 2

	self.cleanSession = (bit.rshift(bit.band(connFlags,0x02) , 1))
	if(self.cleanSession == 1) then
		self.cleanSession = true
	else
		self.cleanSession = false
	end

	self.willFlag = (bit.rshift(bit.band(connFlags , 0x04) , 2))
	if(self.willFlag == 1) then
		self.willFlag = true
	else
		self.willFlag = false
	end

	self.willQos = (bit.rshift(bit.band(connFlags , 0x18) , 3))
	if (self.willQos > 2) then
		error("Expected will QoS in range 0..2 but found: " .. willQos);
	end
	self.willRetain = (bit.rshift(bit.band(connFlags , 0x20) , 5))
	if(self.willRetain == 1) then
		self.willRetain = true
	else
		self.willRetain = false
	end

	local passwordFlag = (bit.rshift(bit.band(connFlags , 0x40) , 6))
	if(passwordFlag == 1) then
		passwordFlag = true
	else
		passwordFlag = false
	end
	local userFlag = (bit.rshift(bit.band(connFlags , 0x80) , 7))
	if(userFlag == 1) then
		userFlag = true
	else
		userFlag = false
	end

	if (not userFlag and passwordFlag) then
		error("Expected password flag to true if the user flag is true but was: " .. passwordFlag)
	end

	local keepAlive = string.byte(message, index) * 256 + string.byte(message, index+1)
	index = index + 2

	if ((remaining_length == 12 and protocolVersion == MQTT.message.VERSION_3_1) or
		(remaining_length == 10 and protocolVersion == MQTT.message.VERSION_3_1_1)) then
		-- out.add(message)
		return;
	end

	local clientIDLen = string.byte(message, index) * 256 + string.byte(message, index + 1)
	self.clientId  = string.sub(message, index + 2, index + 1 + clientIDLen)

	local Subscribe = {}
	Subscribe.min=-1
	Subscribe.qos=1
	local indexOf = ffi.C.lua_utility_indexOf(self.clientId,"&")
	if(indexOf > -1) then
		local args = string.sub(self.clientId, indexOf, clientIDLen)
		self.clientId  = string.sub(self.clientId, 1, indexOf)
		--indexOf = ffi.C.lua_utility_indexOf(args,"=")
		local it, err = ngx.re.gmatch(args, "(&)(\\w+)=(\\w+)")
		if not it then
			ngx.log(ngx.ERR,self.clientId, ", error: ", err)
		else
			while true do
				local m, err = it()
				if err then
					ngx.log(ngx.ERR,self.clientId, ", error: ", err)
					return
				end

				if not m then
					-- no match found (any more)
					break
				end

				-- found a match
				Subscribe[m[2]] = tonumber(m[3])
			end
		end
	end

	index = index + 2 + clientIDLen

	if (self.willFlag) then
		local willMessageLen = string.byte(message, index) * 256 + string.byte(message, index + 1)
		self.willMessage  = string.sub(message, index + 2, index + 1 + willMessageLen)
		index = index + 2 + willMessageLen
	end

	if (userFlag) then
		local userNameLen = string.byte(message, index) * 256 + string.byte(message, index + 1)
		self.username  = string.sub(message, index + 2, index + 1 + userNameLen)
		index = index + 2 + userNameLen
	end

	if (passwordFlag) then
		local passwordLen = string.byte(message, index) * 256 + string.byte(message, index + 1)
		self.password  = string.sub(message, index + 2, index + 1 + passwordLen)
		index = index + 2 + passwordLen
	end
	self.last_connect = self.password
	if(ffi.C.lua_utility_indexOf(self.clientId,"P/")==0) then
		if(not userFlag or not passwordFlag)then
			local msg
			msg = string.char(0x01)
			msg = msg .. string.char(MQTT.CONACK.NOT_AUTHORIZED)
			self:message_write(MQTT.message.TYPE_CONACK, msg,0)
			self:disconnect()
			ngx.log(ngx.DEBUG,self.clientId, ", MQTT.CONACK.NOT_AUTHORIZED")
			return
		end

		Subscribe.topicFilter = self.clientId
		if(Subscribe.min == -1) then
			local offset = self:getConsumerOffset(Subscribe.topicFilter)
			if(offset ~= nil)then
				Subscribe.min=offset
			end
		end
		self:storeSubscribe(Subscribe.topicFilter,Subscribe)
		local sessionBtyes = self.clientTmpStore:get(self.clientId)
		if sessionBtyes ~= nil then
			local old_connect,oc_len = MQTT.protocol.decode_utf8(sessionBtyes,2)
			if old_connect == self.last_connect then
				local msg
				msg = string.char(0x01)
				msg = msg .. string.char(MQTT.CONACK.IDENTIFIER_REJECTED)
				self:message_write(MQTT.message.TYPE_CONACK, msg,0)
				self:disconnect()
				ngx.log(ngx.ERR,self.clientId, ", MQTT.CONACK.IDENTIFIER_REJECTED: can't duplicate login")
				return
			end
			local msg = string.char(MQTT.TYPE_RESERVED.KICK)
			msg = msg .. MQTT.protocol.encode_utf8(self.last_connect)
			self.eventNotify(self,MQTT.message.TYPE_RESERVED,msg)
			ngx.log(ngx.ERR,self.clientId,", MQTT.TYPE_RESERVED.KICK: last_connect=",self.last_connect)
		end
		local serverId=MQTT.get(MQTT.GVAR.AREA_DEFAULT,"serverId")
		if serverId==nil then
			ngx.log(ngx.ERR,self.clientId, ", please set \"mqtt_server_id\" at nginx.conf.")
			self:destroy()
			return
		end
		self.clientTmpStore:set(self.clientId,string.char(ngx.worker.id())..MQTT.protocol.encode_utf8(self.last_connect)..MQTT.protocol.encode_utf8(serverId))
	end

	if self.cleanSession then
		self:clearConsumerOffset()
		self:clearSubscribes()
	else
		self:recoverConsumerOffset()
		self:recoverSubscribes()
	end

	local msg
	msg = string.char(0x01)
	msg = msg .. string.char(MQTT.CONACK.CONNECTION_ACCEPTED)
	self:message_write(MQTT.message.TYPE_CONACK, msg,0)

	local sema = semaphore.new()
	local function handler()
        while self.destroyed==false do
	        local ok, err = sema:wait(24000)  -- wait for a second at most
	        if not ok then
	            ngx.log(ngx.DEBUG,self.clientId,", sub thread: failed to wait on sema: ", err)
	        else
	            self:mqtt_process_subscribe()
	        end
        end
    end
	local co = ngx.thread.spawn(handler)
	MQTT.set(MQTT.GVAR.AREA_SUBSCRIBEMAP,self.clientId,{subsMap=self.subsMap,semaphore=sema})
	ngx.log(ngx.DEBUG,ngx.var.remote_addr," ",self.clientId," CONACK. minoffset=",Subscribe.min)
  end
end
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT CONACK message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.2: CONACK Acknowledge connection
--
-- byte 1: Reserved value
-- byte 2: Connect return code, see MQTT.CONACK.error_message[]

function MQTT.protocol:parse_message_conack(                      -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (remaining_length ~= 2) then
    error(self.clientId..", MQTT.protocol:parse_message_conack(): Invalid remaining length")
  end

  local return_code = string.byte(message, 2)

  if (return_code ~= 0) then
    local error_message = "Unknown return code"

    if (return_code <= table.getn(MQTT.CONACK.error_message)) then
      error_message = MQTT.CONACK.error_message[return_code]
    end

    error(self.clientId..": Connection refused: " .. error_message)
  end
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT PINGRESP message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.13: PING response

function MQTT.protocol:parse_message_pingresp(                    -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (remaining_length ~= 0) then
    error(self.clientId..", MQTT.protocol:parse_message_pingresp(): Invalid remaining length")
  end

-- ToDo: self.ping_response_outstanding = false
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT SUBSCRIBE message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.3: Subscribe message
--
-- Variable header ..
-- bytes 1- : Topic name and optional Message Identifier (if QOS > 0)
-- bytes m- : Payload

function MQTT.protocol:parse_message_subscribe(                     -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

	if (self.websock == nil) then
		return
	end

	if (remaining_length < 3) then
	  error(self.clientId..", MQTT.protocol:parse_message_subscribe(): Invalid remaining length: " .. remaining_length)
	end

	--MQTT.protocol:genericDecodeCommonHeader(message_type_flags)
	local index = 1
	-- Handle optional Message Identifier, for QOS levels 1 and 2
	-- TODO: Enable Subscribe with QOS and deal with PUBACK, etc.
	local message_id  = string.byte(message, index) * 256 + string.byte(message, index+1)
	--index = index + 2

	local subackMsg
	local localSubsMap={}
	--subackMsg = string.char(MQTT.Utility.shift_right(self.message_id,4))
	--subackMsg = subackMsg .. string.char(MQTT.Utility.shift_left(self.message_id,4))
	local obj_tmp = MQTT.get(MQTT.GVAR.AREA_SUBSCRIBEMAP,self.clientId)
	subackMsg = string.char(string.byte(message, index)) .. string.char(string.byte(message, index+1))
	index = index + 2
	while (index < remaining_length) do
		local topic_length = string.byte(message, index) * 256 + string.byte(message, index+1)
		local topic  = string.sub(message, index+2, index+1+topic_length)
		ngx.log(ngx.DEBUG,self.clientId,", old topic:",topic)
		local indexOf = ffi.C.lua_utility_indexOf(topic,"G/E/N/") -- G/E/ 表示扩展通道
		if(indexOf == 0 ) then
			self.enableNotify=true
		end

		local Subscribe = {}
		indexOf = ffi.C.lua_utility_indexOf(topic,"&")
		if(indexOf > -1) then
			topic  = string.sub(message, index+2, index+1+indexOf)
			local args = string.sub(message, index+2+indexOf, index+1+topic_length)
			--indexOf = ffi.C.lua_utility_indexOf(args,"=")
			local it, err = ngx.re.gmatch(args, "(&)(\\w+)=(\\w+)")
			if not it then
				ngx.log(ngx.ERR, self.clientId, ", error: ", err)
			else
				while true do
					local m, err = it()
					if err then
						ngx.log(ngx.ERR,self.clientId, ", error: ", err)
						return
					end

					if not m then
						-- no match found (any more)
						break
					end

					-- found a match
					-- ngx.log(ngx.DEBUG, "KEY: ", m[2],",VAL:",m[3])
					Subscribe[m[2]]=tonumber(m[3])
				end
			end
		end

		if(Subscribe.min == nil) then
			Subscribe.min = self:getConsumerOffset(topic)
			if(Subscribe.min == nil)then
				Subscribe.min = -1
			end
		else
			local it, err = ngx.re.gmatch(topic, "(\\+|\\#)")
			local m, err = it()
			if m then
				error(self.clientId..", MQTT.protocol:parse_message_subscribe(): Invalid topic: " .. topic)
			end
		end

		index = index + topic_length + 2
		local qosByte = string.byte(message, index)
		qosByte = (bit.band(qosByte, 0x03))
		index = index + 1


		Subscribe.qos = qosByte
		Subscribe.topicFilter = topic

		if Subscribe.max == nil then
			self:storeSubscribe(topic,Subscribe)
		else
			if (Subscribe.max - Subscribe.min)>1000 then
				error(self.clientId..", MQTT.protocol:parse_message_subscribe(): Invalid offset range: " .. (Subscribe.max - Subscribe.min))
			end
		end
		localSubsMap[topic] = Subscribe
		--self.subDICT.set(topic,self)
		--ngx.log(ngx.DEBUG,MQTT.Utility.tableprint(MQTT_SUB_MAP[topic]))
		subackMsg = subackMsg .. string.char(qosByte)
		if ffi.C.ngx_mqtt_topic_is_expression(topic) then
			obj_tmp.isexpress=1
			Subscribe.qos=0
		end
		ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,",qosByte:",qosByte,",messageSize:",#message,",minoffset:",Subscribe.min)
	end
	self.subscribeFlg = 1
	--ngx.log(ngx.DEBUG,MQTT.Utility.tableprint(self.subsMap))
	self:mqtt_process_subscribe(localSubsMap)
	for sub_topic, subs in pairs(localSubsMap) do
		subs.max = nil
	end
	localSubsMap[{}]=nil
	localSubsMap=nil
	self.subscribeFlg=2
	self.eventNotify(self,MQTT.message.TYPE_SUBSCRIBE,message)
	self:message_write(MQTT.message.TYPE_SUBACK, subackMsg,0)

end

function MQTT.protocol:mqtt_process_subscribe(                     -- Internal API
  subsMap)             -- table

end
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT PUBLISH message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.3: Publish message
--
-- Variable header ..
-- bytes 1- : Topic name and optional Message Identifier (if QOS > 0)
-- bytes m- : Payload

function MQTT.protocol:parse_message_publish(                     -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (self.websock ~= nil) then
    if (remaining_length < 3) then
      error(self.clientId..", MQTT.protocol:parse_message_publish(): Invalid remaining length: " .. remaining_length)
    end

    local topic_length = string.byte(message, 1) * 256 + string.byte(message, 2)
    local topic  = string.sub(message, 3, topic_length + 2)
    local index  = topic_length + 3

-- Handle optional Message Identifier, for QOS levels 1 and 2
-- TODO: Enable Subscribe with QOS and deal with PUBACK, etc.

    local qos = bit.rshift(message_type_flags, 1) % 3
	local message_id
    if (qos > 0) then
      message_id = string.byte(message, index) * 256 + string.byte(message, index + 1)
      index = index + 2
    end

    local payload_length = remaining_length - index + 1
    local payload = string.sub(message, index, index + payload_length - 1)

  end
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT SUBACK message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.9: SUBACK Subscription acknowledgement
--
-- bytes 1,2: Message Identifier
-- bytes 3- : List of granted QOS for each subscribed topic

function MQTT.protocol:parse_message_suback(                      -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (remaining_length < 3) then
    error("MQTT.protocol:parse_message_suback(): Invalid remaining length: " .. remaining_length)
  end

  local message_id  = string.byte(message, 1) * 256 + string.byte(message, 2)
  --[[
  local outstanding = self.outstanding[message_id]

  if (outstanding == nil) then
    error(me .. ": No outstanding message: " .. message_id)
  end

  self.outstanding[message_id] = nil

  if (outstanding[1] ~= "subscribe") then
    error(me .. ": Outstanding message wasn't SUBSCRIBE")
  end

  local topic_count = table.getn(outstanding[2])

  if (topic_count ~= remaining_length - 2) then
    error(me .. ": Didn't received expected number of topics: " .. topic_count)
  end
  ]]
end

function MQTT.protocol:parse_message_puback(                      -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)
  print("MQTT.protocol:parse_message_puback(): PUBACK -- UNIMPLEMENTED --"..self.clientId)    -- TODO
end
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT UNSUBSCRIBE message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.11: UNSUBSCRIBE Unsubscription
--
-- bytes 1,2: Message Identifier

function MQTT.protocol:parse_message_unsubscribe(                    -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

	if (remaining_length < 4) then
		error("MQTT.protocol:parse_message_unsubscribe(): Invalid remaining length")
	end

	local message_id = string.byte(message, 1) * 256 + string.byte(message, 2)

  	local index = 3
	while (index < remaining_length) do
		local topic_length = string.byte(message, index) * 256 + string.byte(message, index+1)
		local topic  = string.sub(message, index+2, index+1+topic_length)
		ngx.log(ngx.DEBUG,self.clientId,", topic:",topic,",topic_length:",topic_length,",messageSize:",#message,",index:",index)

		index = index + topic_length + 2

		self:deleteSubscribe(topic)

	end

	self.eventNotify(self,MQTT.message.TYPE_UNSUBSCRIBE,message)
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Parse MQTT UNSUBACK message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.11: UNSUBACK Unsubscription acknowledgement
--
-- bytes 1,2: Message Identifier

function MQTT.protocol:parse_message_unsuback(                    -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (remaining_length ~= 2) then
    error("MQTT.protocol:parse_message_unsuback(): Invalid remaining length")
  end

  local message_id = string.byte(message, 1) * 256 + string.byte(message, 2)
--[[
  local outstanding = self.outstanding[message_id]

  if (outstanding == nil) then
    error(me .. ": No outstanding message: " .. message_id)
  end

  self.outstanding[message_id] = nil

  if (outstanding[1] ~= "unsubscribe") then
    error(me .. ": Outstanding message wasn't UNSUBSCRIBE")
  end
  ]]
end


function MQTT.protocol:parse_message_reserved(                     -- Internal API
  message_type_flags,  -- byte
  remaining_length,    -- integer
  message)             -- string

  ngx.log(ngx.DEBUG,self.clientId)

  if (self.websock ~= nil) then
    if (remaining_length < 3) then
      error("MQTT.protocol:parse_message_reserved() : Invalid remaining length: " .. remaining_length)
    end

	local cmd = string.byte(message, 1)
	local last_connect,last_connect_length = MQTT.protocol.decode_utf8(message, 2)
	if(self.last_connect ~= last_connect) then
		self.enableNotify = nil --关闭通知
		local msg
		msg = string.char(0x01)
		msg = msg .. string.char(MQTT.TYPE_RESERVED.KICK)
		self:message_write(MQTT.message.TYPE_RESERVED, msg,0)
		self:disconnect()
		ngx.log(ngx.DEBUG,"recv MQTT.TYPE_RESERVED.KICK[kick]: clientId=",self.clientId,",last_connect=",last_connect,",self.last_connect=",self.last_connect)
	else
		ngx.log(ngx.DEBUG,"recv MQTT.TYPE_RESERVED.KICK[ignore]: clientId=",self.clientId,",last_connect=",last_connect,",self.last_connect=",self.last_connect)
    end
  end
end

-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit MQTT Ping response message
-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- MQTT 3.1 Specification: Section 3.13: PING response

function MQTT.protocol:ping_response()                            -- Internal API
  if (self.connected == false) then
    error("MQTT.protocol:ping_response(): Not connected")
  end

  self:message_write(MQTT.message.TYPE_PINGRESP, nil,0)
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit MQTT Publish message.
-- MQTT 3.1 Specification: Section 3.3: Publish message
--
-- * bytes 1,2: Fixed message header, see MQTT.protocol:message_write()
--            Variable header ..
-- * bytes 3- : Topic name and optional Message Identifier (if QOS > 0)
-- * bytes m- : Payload
-- @param self
-- @param #string topic
-- @param #string payload
-- @function [parent = #protocol] publish
--
function MQTT.protocol:publish(                                     -- Public API
  topic,    -- string
  payload)  -- string

  if (self.connected == false) then
    error("MQTT.protocol:publish(): Not connected")
  end

  ngx.log(ngx.DEBUG,self.clientId,", topic:",topic)

  local message = MQTT.protocol.encode_utf8(topic) .. payload

  self:message_write(MQTT.message.TYPE_PUBLISH, message,0)
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit MQTT Subscribe message.
-- MQTT 3.1 Specification: Section 3.8: Subscribe to named topics
--
-- * bytes 1,2: Fixed message header, see MQTT.protocol:message_write()
--            Variable header ..
-- * bytes 3,4: Message Identifier
-- * bytes 5- : List of topic names and their QOS level
-- @param self
-- @param #string topics table of strings
-- @function [parent = #protocol] subscribe
--
function MQTT.protocol:subscribe(                                   -- Public API
  topics)  -- table of strings

  if (self.connected == false) then
    error("MQTT.protocol:subscribe(): Not connected")
  end

  message_id = message_id + 1

  local message
  message = string.char(math.floor(message_id / 256))
  message = message .. string.char(message_id % 256)

  for index, topic in ipairs(topics) do
    ngx.log(ngx.DEBUG,self.clientId,",topic:",topic)
    message = message .. MQTT.protocol.encode_utf8(topic)
    message = message .. string.char(0)  -- QOS level 0
  end

  self:message_write(MQTT.message.TYPE_SUBSCRIBE, message,1)

  --self.outstanding[message_id] = { "subscribe", topics }
end

--- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --
-- Transmit MQTT Pingreq message.
-- MQTT 3.1 Specification: Section 3.8: Pingreq request

function MQTT.protocol:pingreq()  -- table of strings

  if (self.connected == false) then
    error("MQTT.protocol:pingreq(): Not connected")
  end
  ngx.log(ngx.DEBUG,self.clientId,",topic:",topic)
  self:message_write(MQTT.message.TYPE_PINGREQ,nil,0)
end

function MQTT.Utility.encode_remaining_length(_length)
	local remaining_length = _length
	local message=""
	repeat
      local digit = remaining_length % 128
      remaining_length = math.floor(remaining_length / 128)
      if (remaining_length > 0) then digit = digit + 128 end -- continuation bit
      message = message .. string.char(digit)
    until remaining_length == 0
	return message
end


--public static byte[]
function MQTT.Utility.longToByte(payload_len)

	--[[
	local b7= bit.band(number , 0xff) -- 将最低位保存在最低位
	local b6 = bit.band(bit.rshift(number, 8) , 0xff)
	local b5 = bit.band(bit.rshift(number, 16) , 0xff)
	local b4 = bit.band(bit.rshift(number, 24) , 0xff)
	local b3 = bit.band(bit.rshift(number, 32) , 0xff)
	local b2 = bit.band(bit.rshift(number, 40) , 0xff)
	local b1 = bit.band(bit.rshift(number, 48) , 0xff)
	local b0 = bit.band(bit.rshift(number, 56) , 0xff)
	return string.char(b0)..string.char(b1)..string.char(b2)..string.char(b3)..string.char(b4)..string.char(b5)..string.char(b6)..string.char(b7)
	]]

	local snd, extra_len_bytes
    if payload_len <= 125 then
        snd = payload_len
        extra_len_bytes = ""

    elseif payload_len <= 65535 then
        snd = 126
        extra_len_bytes = string.char(bit.band(bit.rshift(payload_len, 8), 0xff),
                               bit.band(payload_len, 0xff))

    elseif bit.band(payload_len, 0x7fffffff) < payload_len then
            return nil, "payload too big"
	else
        snd = 127
        -- XXX we only support 31-bit length here
        extra_len_bytes = string.char(0, 0, 0, 0, bit.band(bit.rshift(payload_len, 24), 0xff),
                               bit.band(bit.rshift(payload_len, 16), 0xff),
                               bit.band(bit.rshift(payload_len, 8), 0xff),
                               bit.band(payload_len, 0xff))
    end

	return string.char(snd) .. extra_len_bytes
end

--public static long
function MQTT.Utility.byteToLong(b)
	local snd = string.byte(b)
	local payload_len = bit.band(snd, 0x7f)
    -- print("payload len: ", payload_len)

    if payload_len == 126 then
        local data, err = string.sub(b,1,2)
        if not data then
            return nil, nil, "failed to receive the 2 byte payload length: "
                             .. (err or "unknown")
        end

        payload_len = bit.bor(bit.lshift(string.byte(data, 1), 8), string.byte(data, 2))

    elseif payload_len == 127 then
        local data, err = string.sub(b,1,8)
        if not data then
            return nil, nil, "failed to receive the 8 byte payload length: "
                             .. (err or "unknown")
        end

        if string.byte(data, 1) ~= 0
           or string.byte(data, 2) ~= 0
           or string.byte(data, 3) ~= 0
           or string.byte(data, 4) ~= 0
        then
            return nil, nil, "payload len too large"
        end

        local fifth = string.byte(data, 5)
        if bit.band(fifth, 0x80) ~= 0 then
            return nil, nil, "payload len too large"
        end

        payload_len = bit.bor(bit.lshift(fifth, 24),
                          bit.lshift(string.byte(data, 6), 16),
                          bit.lshift(string.byte(data, 7), 8),
                          string.byte(data, 8))
    end
--[[
	local s = 0
	local s0 = bit.band(string.byte(b, 1), 0xff)-- 最低位
	local s1 = bit.band(string.byte(b, 2), 0xff)
	local s2 = bit.band(string.byte(b, 3), 0xff)
	local s3 = bit.band(string.byte(b, 4), 0xff)
	local s4 = bit.band(string.byte(b, 5), 0xff)-- 最低位
	local s5 = bit.band(string.byte(b, 6), 0xff)
	local s6 = bit.band(string.byte(b, 7), 0xff)
	local s7 = bit.band(string.byte(b, 8), 0xff) -- s0不变

	s1=bit.lshift(s1, 8)
	s2=bit.lshift(s2, 16)
	s3=bit.lshift(s3, 24)
	s4=bit.lshift(s4, 32)
	s5=bit.lshift(s5, 40)
	s6=bit.lshift(s6, 48)
	s7=bit.lshift(s7, 56)
	s = bit.bor(s0,s1,s2,s3,s4,s5,s6,s7)
	]]
	return payload_len
end

function MQTT.Utility.uint64ToBytes(n)
	local _payload = ffi.new("unsigned char[?]", 8)
	ffi.C.utility_lua_write_int64(_payload,n)
	return ffi.string(_payload,8)

	--[[	return string.char(	bit.band(bit.rshift(n,56),0xff),
							bit.band(bit.rshift(n,48),0xff),
							bit.band(bit.rshift(n,40),0xff),
							bit.band(bit.rshift(n,32),0xff),
							bit.band(bit.rshift(n,24),0xff),
							bit.band(bit.rshift(n,16),0xff),
							bit.band(bit.rshift(n,8),0xff),
							bit.band(n,0xff))
							]]
end

function MQTT.Utility.readUint64(b,offset)
	local s0 = bit.band(string.byte(b, offset), 0xff)
	local s1 = bit.band(string.byte(b, offset+1), 0xff)
	local s2 = bit.band(string.byte(b, offset+2), 0xff)
	local s3 = bit.band(string.byte(b, offset+3), 0xff)
	local s4 = bit.band(string.byte(b, offset+4), 0xff)
	local s5 = bit.band(string.byte(b, offset+5), 0xff)
	local s6 = bit.band(string.byte(b, offset+6), 0xff)
	local s7 = bit.band(string.byte(b, offset+7), 0xff)

	return bit.bor(bit.lshift(s0, 56),bit.lshift(s1, 48),bit.lshift(s2, 40),bit.lshift(s3, 32),bit.lshift(s4, 24),bit.lshift(s5, 16),bit.lshift(s6, 8),s7)
end

function MQTT.Utility.uint32ToBytes(n)
		return string.char(bit.band(bit.rshift(n,24),0xff),
						bit.band(bit.rshift(n,16),0xff),
						bit.band(bit.rshift(n,8),0xff),
						bit.band(n,0xff))
end

function MQTT.Utility.readUint32(b,offset)
	local s0 = bit.band(string.byte(b, offset), 0xff)
	local s1 = bit.band(string.byte(b, offset+1), 0xff)
	local s2 = bit.band(string.byte(b, offset+2), 0xff)
	local s3 = bit.band(string.byte(b, offset+3), 0xff)

	s0=bit.lshift(s0, 24)
	s1=bit.lshift(s1, 16)
	s2=bit.lshift(s2, 8)

	return bit.bor(s0,s1,s2,s3)
end


local _str="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+=-0987654321~`\\|][{};':\",./<>?"
function MQTT.Utility.randStr(seed)
	math.randomseed(os.time()+seed);
	local n=math.random(0, 100)
	local str=""
	for j=1,n,1 do
		local k=math.random(0, #_str)
		str=str..string.sub(_str,k+1)
	end
	return str
end

-- For ... MQTT = require 'paho.mqtt'

return(MQTT)
