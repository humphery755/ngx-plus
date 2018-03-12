-- {c_v:{a_v:{h_v:{s_m:{}}}}}
-- curl http://192.168.100.213:4000/upgrade?c_v=BYAPP&a_v=a1.0&h_v=h1.0&sm=IOS
local cjson = require "cjson"

local confJson = [[
{
	"BYAPP": {
		"a1.0": {
			"h1.0": {
				"IOS": {"file": "/xxx.apk",	"size": 1024},
				"AND": {}
			}
		}
	}
}
]]
ngx.log(ngx.ERR,confJson)

local Configure = cjson.decode(confJson)


local args = ngx.req.get_uri_args()

local av = args["a_v"]
if av==nil then
	ngx.log(ngx.ERR,"a_v is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local hv = args["h_v"]
if hv==nil then
	ngx.log(ngx.ERR,"h_v is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local sm = args["s_m"]
if sm==nil then
	ngx.log(ngx.ERR,"s_m is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local cv = args["c_v"]
if cv==nil then
	ngx.log(ngx.ERR,"c_v is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local cvConf=Configure[cv]
if cvConf==nil then
	ngx.log(ngx.ERR,cv.." is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local avConf=cvConf[av]
if avConf==nil then
	ngx.log(ngx.ERR,av.." is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local hvConf=avConf[hv]
if hvConf==nil then
	ngx.log(ngx.ERR,hv.."  is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local smConf=hvConf[sm]
if smConf==nil then
	ngx.log(ngx.ERR,sm.." is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end

local url=smConf["file"]
if url==nil then
	ngx.log(ngx.ERR,"file is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end
local size=smConf["size"]
if size==nil then
	ngx.log(ngx.ERR,"size is null")
	ngx.exit(ngx.HTTP_BAD_REQUEST)
end
url=url.."?file-length="..size

local hash=smConf["hash"]
if hash~=nil then
	url=url.."&file-hash="..hash
end

url=url.."&last-time="..(ngx.now()*1000)


ngx.redirect(url)


