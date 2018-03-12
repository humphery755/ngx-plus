directory /app/mqtt1/nginx-gdb-utils

py import sys
py sys.path.append("/app/mqtt1/nginx-gdb-utils")

source luajit20.gdb
source ngx-lua.gdb
source luajit21.py
source ngx-raw-req.py
set python print-stack full

