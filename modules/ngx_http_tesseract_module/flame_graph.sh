sudo nginx-systemtap-toolkit/sample-bt -u -t 20 -p $1 > trace.txt
sudo nginx-systemtap-toolkit/FlameGraph/stackcollapse-stap.pl ./trace.txt > trace.out
rm trace.txt
nginx-systemtap-toolkit/FlameGraph/flamegraph.pl ./trace.out >  trace.svg
rm trace.out