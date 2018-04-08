nohup sh ./bin/start.sh > nohup.log &
ps -ef |grep org.dna.mqtt.batch.Bootstrap |grep java |awk '{print $2}'
