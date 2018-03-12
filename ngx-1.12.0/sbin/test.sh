#!/usr/bin/env bash
export NGX_HOME=`pwd`
export PATH=objs/:$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$NGX_HOME/lib
. $NGX_HOME/sbin/env.sh

rm -rf objs/addon/http/ngx_lua_*

function test()
{
echo "test *************************************"
#prove  -r $NGX_HOME/modules/ngx_lua_diversion_module/t/div.t
prove -r $NGX_HOME/modules/nginx_upstream_check_module/test/t/http_check.t
cat t/servroot/logs/*.log

}

function g()
{
echo "gdb *************************************"
nginx -p $NGX_HOME/t/servroot/ -c $NGX_HOME/t/servroot/conf/nginx.conf
cat t/servroot/logs/*.log
sleep 3
pid=`cat t/servroot/logs/nginx.pid`
echo "gdb attach $pid"
gdb attach $pid -x sbin/gdbattach
cat t/servroot/logs/*.log
}

#/usr/sbin/nginx 	             #nginx的安装位置，一般是此
#-D MAXMAPENTRIES=256 	     #本机是个虚拟机，只给了其512M内存，为了保证内存不溢出，设为256
#-x 2082			     #指定其中一个nginx worker进程的pid
function flamengx(){
pid=`cat logs/nginx.pid`
stap --ldd -d sbin/nginx --all-modules -D MAXMAPENTRIES=2048 -D MAXACTION=20000 -D MAXTRACE=100 -D MAXSTRINGLEN=4096 -D MAXBACKTRACE=100 -x $pid ngx.stp --vp 0001 > temp/ngx.out
perl $THIRD_PARTY_HOME/performance/FlameGraph/stackcollapse-stap.pl temp/ngx.out > temp/ngx.out2
perl $THIRD_PARTY_HOME/performance/FlameGraph/flamegraph.pl temp/ngx.out2 > temp/ngx.svg
}
function flame(){
pid=`cat logs/nginx.pid`
$THIRD_PARTY_HOME/performance/openresty-systemtap-toolkit/sample-bt -p $pid -u -t 20 > temp/trace.txt
$THIRD_PARTY_HOME/performance/FlameGraph/stackcollapse-stap.pl temp/trace.txt > temp/trace.out
$THIRD_PARTY_HOME/performance/FlameGraph/flamegraph.pl temp/trace.out >  temp/trace.svg
}
function flamelua(){
pid=`cat logs/nginx.pid`
        #$THIRD_PARTY_HOME/performance/openresty-systemtap-toolkit/ngx-sample-lua-bt -p $pid --luajit20 -t 70 > temp/tmp.bt
$THIRD_PARTY_HOME/performance/openresty-systemtap-toolkit/ngx-pcrejit -p $pid  > temp/tmp.bt
$THIRD_PARTY_HOME/performance/openresty-systemtap-toolkit/fix-lua-bt temp/tmp.bt > temp/flame.bt
$THIRD_PARTY_HOME/performance/FlameGraph/stackcollapse-stap.pl temp/flame.bt > temp/flame.cbt
$THIRD_PARTY_HOME/performance/FlameGraph/flamegraph.pl temp/flame.cbt > flame.svg
}

function t()
{
        objs/nginx -p t/ -s stop
        objs/nginx -p t/
}
case $1 in
        "gdb")
                g;
                ;;
        "test")
                test;
                ;;
        "flame")
                flame;
                ;;
        "t")
                t;
                ;;
esac
exit 0

