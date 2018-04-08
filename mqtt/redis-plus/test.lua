require "mylualib"
local t=os.clock() 
print("input: "..t)
local buf=mylualib.int64tostr(t)
print("output: "..#buf)
--
print(mylualib.readInt64(buf))