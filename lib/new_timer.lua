local _M = {}

local mt = { __index = _M }


local timer_fn
timer_fn = function (premature, ctx)
	if premature then
		return
	end

	local ok, err = pcall(ctx.callback_fn, ctx)
	if not ok then
		libmqttkit.log(ngx.ERR,"failed to run callback_fn cycle: ", err)
	end

	local ok, err = ngx.timer.at(ctx.interval, timer_fn, ctx)
	if not ok then
		if err ~= "process exiting" then
			libmqttkit.log(ngx.ERR,"failed to create timer: ", err)
		end
		return
	end
end

function _M.new(self, opts)
	if opts.interval==nil then
		error("interval is nil")
	end
	if opts.callback_fn==nil then
		error("callback_fn is nil")
	end
	local ok, err = ngx.timer.at(opts.interval, timer_fn, opts)
	if not ok then
		libmqttkit.log(ngx.ERR,"failed to create timer: " .. err)
	end
    return setmetatable({
		callback_fn=opts.callback_fn
    }, mt)
end

return _M