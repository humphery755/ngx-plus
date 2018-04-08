PRG_PATH=$(dirname $0)
PRG_HOME=$(readlink -f $PRG_PATH/..)
LIB_PATH=$PRG_HOME/lib
CONF_PATH=$PRG_HOME/config
CLASS_PATH=$CONF_PATH:$LIB_PATH:$LIB_PATH/*

BOOT_CLASS=org.dna.mqtt.batch.Bootstrap
#JAVA_OPTS="-agentlib:jdwp=transport=dt_socket,address=8000,server=y,suspend=n"
JAVA_OPTS="-Xms4g -Xmx4g -XX:MaxPermSize=128M -XX:PermSize=64M -Dfile.encoding=UTF-8 -Djava.library.path=$LIB_PATH"
JAVA_ENV="-DLOG_HOME=$PRG_HOME/logs -DAPP_HOST=$(hostname)-$(whoami)"
echo "java -cp $CLASS_PATH $JAVA_OPTS $JAVA_ENV $BOOT_CLASS"
java -cp $CLASS_PATH $JAVA_OPTS $JAVA_ENV $BOOT_CLASS 
