#!/bin/bash
clear

PRG="$0"
while [ -h "$PRG" ]; do
  ls=`ls -ld "$PRG"`
  link=`expr "$ls" : '.*-> \(.*\)$'`
  if expr "$link" : '/.*' > /dev/null; then
    PRG="$link"
  else
    PRG=`dirname "$PRG"`/"$link"
  fi
done
# Get standard environment variables
PRGDIR=`dirname "$PRG"`
# Only set NGX_HOME if not already set
[ -z "$NGX_HOME" ] && NGX_HOME=`cd "$PRGDIR/.." >/dev/null; pwd`
export NGX_HOME
cd $NGX_HOME

. $NGX_HOME/sbin/env.sh

function stop(){
pid=`cat $NGX_HOME/logs/nginx.pid`
if [ -n "$pid" ];then
	$NGX_HOME/sbin/nginx -p $NGX_HOME/ -s stop
	for((i=1;i<30;i++))
	do
		p_count=`ps -ef|grep nginx |grep ${pid}|wc |awk '{print $1}'`
		if (($p_count > 0)); then
			echo "nginx process: ${p_count}"
			sleep 1
		fi
	done
fi
}


function gdb_()
{
mid=`cat $NGX_HOME/logs/nginx.pid`
pid=`ps -ef|grep $mid|grep "worker process"|awk '{print $2}'`
echo "gdb attach $pid"
gdb attach $pid -x $NGX_HOME/sbin/gdbattach
}

function start(){
#valgrind --tool=memcheck $NGX_HOME/sbin/nginx -p $NGX_HOME
$NGX_HOME/sbin/nginx -p $NGX_HOME
}

case $1 in
        "g")
                gdb_;
                ;;
        "start")
                start;
                ;;
        "stop")
                stop;
                ;;
        "reload")
                $NGX_HOME/sbin/nginx -s reload -p $NGX_HOME
                ;;
        "test")
                $NGX_HOME/sbin/nginx -t -p $NGX_HOME;
                ;;
        "restart")
                stop;
                start;
                ;;
         *) #default 
		echo "Useage: sbin/nginx.sh start|stop|reload|restart|test"
esac
exit 0

