http://www.easydarwin.org/index.html
ffmpeg -i sample_h264_1mbit.mp4 -vcodec libx264 -f rtsp rtsp://192.168.100.213:1554/stream.sdp

ffmpeg -i sample_h264_1mbit.mp4 -vcodec copy -acodec copy -f rtsp rtsp://192.168.100.213:1554/stream.sdp
 ffmpeg -re -i sample_h264_1mbit.mp4 -vcodec copy -acodec copy -f flv rtmp://192.168.100.213:1935/myapp/test

 HLS: rtmp://192.168.100.213:1935/hls/mystream
 ffmpeg -y -i sample_h264_1mbit.mp4 -pix_fmt yuv420p -vcodec libx264 -acodec libfaac -r 150 -profile:v baseline -b:v 2000k -maxrate 2500k  -force_key_frames 50 -s 640*360 -f flv rtmp://192.168.100.213:1935/hls/mystream
 
 ffmpeg -y -i sample_h264_1mbit.mp4 -pix_fmt yuv420p -vcodec libx264 -acodec libfaac -r 25 -profile:v baseline -b:v 1500k -maxrate 2000k -force_key_frames 50 -s 640×360 -map 0 -flags -global_header -f segment -segment_list /tmp/index_1500.m3u8 -segment_time 10 -segment_format mpeg_ts -segment_list_type m3u8 /tmp/segment%05d.ts
 
 rtmp://192.168.100.213:1935/myflv/mystream
 
 
offset:9,body:{"mId":"20151202000012000016528","is_master":0,"play":"EUROPE","msgId":"SP_oeurope-0000165285223869691448957896978","site_id":1,"uTime":1448957896000,"match_phase":0,"msgType":"2","odds_phase":2,"odds":"1.70400,3.62000,6.19000","is_init":0}

more logs/error.log |awk -F\| '{print $3 " \t\t=\t\t" $5}' |awk -F: ' {print $2}' |awk -F= '{if($2>3000 && $2<8000){print $0 | "sort -r -n -k3";}}' |sort

more logs/error.log |awk '{print $7 " \t\t=\t\t" $8}' |awk -F: ' {print $2}' |awk -F= '{if($2>3000 && $2<8000){print $0 | "sort -r -n -k3";}}' |sort

more logs/error.log |awk '{a[$7]++}END{for(i in a)print a[i],i}' |sort -k1 -n|more

lua atpanic: Lua VM crashed, reason: not enough memory

(gdb) lgcstat
62004 str        objects: max=2311, avg = 49, min=18, sum=3097816
53362 upval      objects: max=24, avg = 24, min=24, sum=1280688
89873 thread     objects: max=1072, avg = 618, min=424, sum=55570048
 166 proto      objects: max=3143, avg = 394, min=80, sum=65478
350690 func       objects: max=144, avg = 22, min=20, sum=7889012
  46 trace      objects: max=1708, avg = 506, min=160, sum=23320
4582 cdata      objects: max=4112, avg = 16, min=12, sum=77432
537503 tab        objects: max=2097192, avg = 169, min=32, sum=91081664
122389 udata      objects: max=440, avg = 430, min=40, sum=52737522

 sizeof strhash 1048576
 sizeof g->tmpbuf 4096
 sizeof ctype_state 8664
 sizeof jit_state 6040

total sz 212895580
g->strnum 62004, g->gc.total 231970900
elapsed: 161.930000 sec

(gdb) lgcstat
556851 str        objects: max=287, avg = 59, min=18, sum=32968181
41782 upval      objects: max=24, avg = 24, min=24, sum=1002768
483064 thread     objects: max=1048, avg = 639, min=424, sum=308713272
 164 proto      objects: max=3143, avg = 382, min=80, sum=62687
1794669 func       objects: max=144, avg = 21, min=20, sum=39395808
  36 trace      objects: max=1500, avg = 473, min=160, sum=17060
92087 cdata      objects: max=4112, avg = 16, min=12, sum=1477512
2719842 tab        objects: max=4194344, avg = 174, min=32, sum=475144600
239530 udata      objects: max=440, avg = 422, min=40, sum=101264904

 sizeof strhash 4194304
 sizeof g->tmpbuf 128
 sizeof ctype_state 8664
 sizeof jit_state 5912

total sz 964261024
g->strnum 556851, g->gc.total 964261024
elapsed: 795.590000 sec

(gdb) lgcstat
802677 str        objects: max=287, avg = 68, min=18, sum=54706362
 330 upval      objects: max=24, avg = 24, min=24, sum=7920
435011 thread     objects: max=976, avg = 610, min=64, sum=265753480
 172 proto      objects: max=3066, avg = 401, min=80, sum=69016
247330 func       objects: max=144, avg = 20, min=20, sum=4952788
  54 trace      objects: max=1500, avg = 503, min=160, sum=27176
213096 cdata      objects: max=4112, avg = 16, min=12, sum=3413656
2935653 tab        objects: max=4194344, avg = 157, min=32, sum=462322080
501909 udata      objects: max=440, avg = 374, min=40, sum=188183085

 sizeof strhash 4194304
 sizeof g->tmpbuf 16384
 sizeof ctype_state 8664
 sizeof jit_state 6040

total sz 983666179
g->strnum 802677, g->gc.total 983666179
elapsed: 694.270000 sec

(gdb) lgcstat
716071 str        objects: max=287, avg = 67, min=18, sum=48155570
 330 upval      objects: max=24, avg = 24, min=24, sum=7920
331586 thread     objects: max=976, avg = 708, min=424, sum=234838216
 170 proto      objects: max=3066, avg = 392, min=80, sum=66755
332096 func       objects: max=144, avg = 20, min=20, sum=6648108
  32 trace      objects: max=1708, avg = 559, min=160, sum=17900
414790 cdata      objects: max=4112, avg = 16, min=12, sum=6640760
3291920 tab        objects: max=2097192, avg = 171, min=32, sum=565827880
272541 udata      objects: max=440, avg = 394, min=40, sum=107556286

 sizeof strhash 4194304
 sizeof g->tmpbuf 128
 sizeof ctype_state 8664
 sizeof jit_state 5912

total sz 973973627
g->strnum 716071, g->gc.total 973973627
elapsed: 693.280000 sec

(gdb) lgcstat
692200 str        objects: max=2311, avg = 66, min=18, sum=46262303
 330 upval      objects: max=24, avg = 24, min=24, sum=7920
329185 thread     objects: max=976, avg = 706, min=424, sum=232636488
 173 proto      objects: max=3294, avg = 400, min=80, sum=69262
329697 func       objects: max=144, avg = 20, min=20, sum=6600140
  37 trace      objects: max=1708, avg = 545, min=160, sum=20196
502911 cdata      objects: max=4112, avg = 16, min=12, sum=8050696
3386865 tab        objects: max=2097192, avg = 166, min=32, sum=564602136
283096 udata      objects: max=440, avg = 384, min=40, sum=108927509

 sizeof strhash 4194304
 sizeof g->tmpbuf 4096
 sizeof ctype_state 8664
 sizeof jit_state 6040

total sz 971394978
g->strnum 692200, g->gc.total 971394978
elapsed: 745.400000 sec

  payload_len = bor(lshift(fifth, 24),
                          lshift(byte(data, 6), 16),
                          lshift(byte(data, 7), 8),
                          byte(data, 8))
						  
						  
	
bin/redis-trib.rb create --replicas 2 192.168.100.42:6379 192.168.100.42:7379 192.168.100.42:5379 192.168.100.69:6379 192.168.100.69:7379 192.168.100.69:5379 192.168.100.201:6379 192.168.100.201:7379 192.168.100.201:5379
./redis-trib.rb reshard_cluster --replicas 2 192.168.100.42:6379 192.168.100.42:7379 192.168.100.42:5379 192.168.100.69:6379 192.168.100.69:7379 192.168.100.69:5379 192.168.100.201:6379 192.168.100.201:7379 192.168.100.201:5379
配置集群

安装ruby

yum install ruby-devel.x86_64

yum install rubygems

安装redis gem 
# gem install redis
Fetching: redis-3.2.1.gem (100%)
Successfully installed redis-3.2.1
Parsing documentation for redis-3.2.1
Installing ri documentation for redis-3.2.1
1 gem installed
使用脚本建立集群机制  

在create的时候,加上参数--replicas 1 表示为每个master分配一个salve,如例子,则是3个master 3个salve

复制代码
# ./redis-trib.rb create 127.0.0.1:7000 127.0.0.1:7001 127.0.0.1:7002 127.0.0.1:7003 127.0.0.1:7004 127.0.0.1:7005
>>> Creating cluster
Connecting to node 127.0.0.1:7000: OK
Connecting to node 127.0.0.1:7001: OK
Connecting to node 127.0.0.1:7002: OK
Connecting to node 127.0.0.1:7003: OK
Connecting to node 127.0.0.1:7004: OK
Connecting to node 127.0.0.1:7005: OK
>>> Performing hash slots allocation on 6 nodes...
Using 6 masters:
127.0.0.1:7000
127.0.0.1:7001
127.0.0.1:7002
127.0.0.1:7003
127.0.0.1:7004
127.0.0.1:7005
M: f3dd250e4bc145c8b9f864e82f65e00d1ba627be 127.0.0.1:7000
   slots:0-2730 (2731 slots) master
M: 1ba602ade59e0770a15128b193f2ac29c251ab5e 127.0.0.1:7001
   slots:2731-5460 (2730 slots) master
M: 4f840a70520563c8ef0d7d1cc9d5eaff6a1547a2 127.0.0.1:7002
   slots:5461-8191 (2731 slots) master
M: 702adc7ae9caf1f6702987604548c6fc1d22e813 127.0.0.1:7003
   slots:8192-10922 (2731 slots) master
M: 4f87a11d2ea6ebe9caf02c9dbd827a3dba8a53cf 127.0.0.1:7004
   slots:10923-13652 (2730 slots) master
M: 216bbb7da50bd130da16a327c76dc6d285f731b3 127.0.0.1:7005
   slots:13653-16383 (2731 slots) master
Can I set the above configuration? (type 'yes' to accept): yes
>>> Nodes configuration updated
>>> Assign a different config epoch to each node
>>> Sending CLUSTER MEET messages to join the cluster
Waiting for the cluster to join...
>>> Performing Cluster Check (using node 127.0.0.1:7000)
M: f3dd250e4bc145c8b9f864e82f65e00d1ba627be 127.0.0.1:7000
   slots:0-2730 (2731 slots) master
M: 1ba602ade59e0770a15128b193f2ac29c251ab5e 127.0.0.1:7001
   slots:2731-5460 (2730 slots) master
M: 4f840a70520563c8ef0d7d1cc9d5eaff6a1547a2 127.0.0.1:7002
   slots:5461-8191 (2731 slots) master
M: 702adc7ae9caf1f6702987604548c6fc1d22e813 127.0.0.1:7003
   slots:8192-10922 (2731 slots) master
M: 4f87a11d2ea6ebe9caf02c9dbd827a3dba8a53cf 127.0.0.1:7004
   slots:10923-13652 (2730 slots) master
M: 216bbb7da50bd130da16a327c76dc6d285f731b3 127.0.0.1:7005
   slots:13653-16383 (2731 slots) master
[OK] All nodes agree about slots configuration.
>>> Check for open slots...
>>> Check slots coverage...
[OK] All 16384 slots covered.
复制代码
 如果需要全部重新自动配置,则删除所有的配置好的cluster-config-file,重新启动所有的redis-server,然后重新执行配置命令即可
 
 
 DEBUG o.d.m.b.PublishProcessor.process.188[MQTTUPConsumer.Disruptor.executor1]: ..1 new: 32 3d 00 18 47 2f 32 2f 32 30 31 35 30 38 31 39 31 30 30 30 30 30 31 37 33 36 36 34 00 01 00 00 00 8e 68 65 6c 6c 6f 20 e4 b8 ad e6 96 87 20 e6 82 a8 e5 a5 bd ef bc 81 77 6f 72 6c 64 21 34 
DEBUG o.d.m.b.PublishProcessor.process.197[MQTTUPConsumer.Disruptor.executor1]: ..1 new: 32 3d 00 18 47 2f 32 2f 32 30 31 35 30 38 31 39 31 30 30 30 30 30 31 37 33 36 36 34 00 01 00 00 00 ef bf bd 68 65 6c 6c 6f 20 e4 b8 ad e6 96 87 20 e6 82 a8 e5 a5 bd ef bc 81 77 6f 72 6c 64 21 34 



