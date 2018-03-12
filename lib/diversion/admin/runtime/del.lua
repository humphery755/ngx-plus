-- curl -H 'Content-Type: application/json'  http://localhost:8000/admin/runtime/del

local dictSysconfig = ngx.shared.DICT_DIVERSION_SYSCONFIG

dictSysconfig:delete("DIV_runtimeInfo")
