local chash = require "chash"

chash.add_upstream("192.168.0.251")
chash.add_upstream("192.168.0.252")
chash.add_upstream("192.168.0.253")

local upstream = chash.get_upstream("my_hash_key")