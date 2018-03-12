#os.execute("sh reload.sh") 

local t = io.popen('sh reload.sh')
local a = t:read("*all")

ngx.print(a)
