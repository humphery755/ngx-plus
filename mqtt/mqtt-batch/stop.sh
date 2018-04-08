#!/bin/bash
ps -ef |grep org.dna.mqtt.batch.Bootstrap |grep java|awk '{print $2}' |xargs kill -9