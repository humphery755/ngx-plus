
local fileBuf
local filelen=0
local chunk_size = 4096
local form = upload:new(chunk_size)
while true do
	local typ, res, err = form:read()
	if not typ then         
		ngx.say(err)
		return
	end
	if typ == "header" then
		if res[1] ~= "Content-Type" then
			--local filen_ame = get_filename(res[2])
			--local extension = getExtension(filen_ame)
		end
	 elseif typ == "body" then
		filelen= filelen + tonumber(string.len(res))
		if fileBuf then
			fileBuf=fileBuf..res
		else
			fileBuf=res
		end
	elseif typ == "part_end" then
		-- do nothing
	elseif typ == "eof" then
		break
	else
		-- do nothing
	end
end
if filelen>0 then
	local outlen=2000
	local outbuf = ffi.new("unsigned char[?]", outlen)
	local impBuf = ffi.new("unsigned char[?]",filelen)
	local lang = ffi.new("unsigned char[?]",8)	
    ffi.copy(impBuf,fileBuf)
	ffi.copy(lang,"eng") --chi_sim eng
	if(ffi.C.ngx_tesseract_ffi_get_utf8_text(impBuf,filelen,outbuf,outlen,lang)==0) then
		ngx.say(ffi.string(outbuf))
	end
end
