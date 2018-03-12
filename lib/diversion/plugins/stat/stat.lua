local tonumber = tonumber

local STAT_LOCK = "STAT_LOCK"
local KEY_START_TIME = "START_TIME"
local KEY_TOTAL_COUNT = "TOTAL_REQUEST_COUNT"
local KEY_TOTAL_SUCCESS_COUNT = "TOTAL_SUCCESS_REQUEST_COUNT"
local KEY_TRAFFIC_READ = "TRAFFIC_READ"
local KEY_TRAFFIC_WRITE = "TRAFFIC_WRITE"
local KEY_TOTAL_REQUEST_TIME = "TOTAL_REQUEST_TIME"

local KEY_REQUEST_2XX = "REQUEST_2XX"
local KEY_REQUEST_3XX = "REQUEST_3XX"
local KEY_REQUEST_4XX = "REQUEST_4XX"
local KEY_REQUEST_5XX = "REQUEST_5XX"
local status = ngx.shared.DICT_DIVERSION_STATUS

local _M = {}

_M.totalCount=0
_M.r2xx=0
_M.r3xx=0
_M.r4xx=0
_M.r5xx=0
_M.reqLen=0
_M.bsend=0
_M.start_time=0

function _M.init()
    local ok, err = status:add(STAT_LOCK, true)
    if ok then
        _M.start_time=ngx.time()

        status:set(KEY_TOTAL_COUNT, 0)

        status:set(KEY_TRAFFIC_READ, 0)
        status:set(KEY_TRAFFIC_WRITE, 0)

        status:set(KEY_TOTAL_REQUEST_TIME, 0)

        status:set(KEY_REQUEST_2XX, 0)
        status:set(KEY_REQUEST_3XX, 0)
        status:set(KEY_REQUEST_4XX, 0)
        status:set(KEY_REQUEST_5XX, 0)
    end

end

function _M.log()
    local ngx_var = ngx.var
    _M.totalCount=_M.totalCount+1
    local http_status = tonumber(ngx_var.status)
    if http_status >= 200 and http_status < 300 then
        _M.r2xx=_M.r2xx+1
    elseif http_status >= 300 and http_status < 400 then
        _M.r3xx=_M.r3xx+1
    elseif http_status >= 400 and http_status < 500 then
        _M.r4xx=_M.r4xx+1
    elseif http_status >= 500 and http_status < 600 then
        _M.r5xx=_M.r5xx+1
    end


    --status:incr(KEY_TRAFFIC_READ, ngx_var.request_length)
    --status:incr(KEY_TRAFFIC_WRITE, ngx_var.bytes_sent)

    --local request_time = ngx.now() - ngx.req.start_time()
    --status:incr(KEY_TOTAL_REQUEST_TIME, request_time)
end

function _M.flush()
    local ngx_var = ngx.var
    status:incr(KEY_TOTAL_COUNT, _M.totalCount)
    _M.totalCount=0
    status:incr(KEY_REQUEST_2XX, _M.r2xx)
    _M.r2xx=0
    status:incr(KEY_REQUEST_3XX, _M.r3xx)
    _M.r3xx=0
    status:incr(KEY_REQUEST_4XX, _M.r4xx)
    _M.r4xx=0
    status:incr(KEY_REQUEST_5XX, _M.r5xx)
    _M.r5xx=0

    --status:incr(KEY_TRAFFIC_READ, ngx_var.request_length)
    --status:incr(KEY_TRAFFIC_WRITE, ngx_var.bytes_sent)

    --local request_time = ngx.now() - ngx.req.start_time()
    --status:incr(KEY_TOTAL_REQUEST_TIME, request_time)
end

function _M.stat()
    local ngx_lua_version = ngx.config.ngx_lua_version
    local result = {
        --nginx_version = ngx.var.nginx_version,
        --ngx_lua_version = math.floor(ngx_lua_version / 1000000) .. '.' .. math.floor(ngx_lua_version / 1000) ..'.' .. math.floor(ngx_lua_version % 1000),
        worker_count = ngx.worker.count(),
        timestamp = ngx.time(),
        ngx_prefix = ngx.config.prefix(),

        start_time = _M.start_time,
        total_count = status:get(KEY_TOTAL_COUNT),
        --traffic_read = status:get(KEY_TRAFFIC_READ),
        --traffic_write = status:get(KEY_TRAFFIC_WRITE),
        --total_request_time = math.floor(status:get(KEY_TOTAL_REQUEST_TIME)),

        req_2xx = status:get(KEY_REQUEST_2XX),
        req_3xx = status:get(KEY_REQUEST_3XX),
        req_4xx = status:get(KEY_REQUEST_4XX),
        req_5xx = status:get(KEY_REQUEST_5XX),

        con_active = ngx.var.connections_active,
        con_reading = ngx.var.connections_reading,
        con_writing = ngx.var.connections_writing,
        con_idle = ngx.var.connections_waiting
    }

    return result
end


return _M
