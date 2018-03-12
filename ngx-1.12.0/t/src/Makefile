# **********************************************************************
#
# Copyright (c) 2003-2016 ZeroC, Inc. All rights reserved.
#g++ -shared -rdynamic -m64 -fvisibility=hidden -Wall -Werror -pthread -fPIC -g -L/home/humphery/tmp/ice-3.6.3/cpp/lib64 -o libtdd_sequence_SequenceServiceI.so -Wl,-h,libtdd_sequence_SequenceServiceI.so   generated/tdd_sequence.o Classes/tdd_sequence_SequenceServiceI.o -lIceBox  -lIce -lIceUtil
# **********************************************************************
NGX_HOME=/root/src/mqtt-nginx-1.12.0
THIRDPARTY_HOME=/usr/local/thirdparty
BENCHMARK_HOME=/usr/local/benchmark
PCRE_HOME=$(THIRDPARTY_HOME)/pcre-8.36
OPENSSL_HOME=/root/src/openssl-1.0.2k/.openssl
#ICE_HOME=/usr/local/ice-3.6.3
GLOG_HOME=$(THIRDPARTY_HOME)/glog
ZK_HOME=/usr/local/zkclient
LDFLAGS := -shared -rdynamic -m64 -fvisibility=hidden -Wall -Werror -pthread -fPIC -g3
CXXFLAGS :=
CCFLAGS += 
DEBUG=1
TARGET=test

NGX_SRC = $(shell find $(NGX_HOME)/src/core/ ! -iname 'nginx.c' ! -iname "ngx_thread_pool.c" ! -iname 'ngx_log.c' ! -iname "ngx_proxy_protocol.c" ! -iname "ngx_cycle.c" \
	! -iname "ngx_syslog.c" ! -iname "ngx_process_cycle.c" ! -iname "ngx_open_file_cache.c" ! -iname "ngx_resolver.c" ! -iname "ngx_module.c" -regex '.*\.cpp\|.*\.c' ) \
	$(shell find $(NGX_HOME)/src/event/ -maxdepth 1 ! -iname 'ngx_event_openssl_stapling.c' ! -iname "ngx_event_openssl.c" ! -iname 'modules*' \
	 -regex '.*\.c' ) \
	$(shell find $(NGX_HOME)/src/os/unix/ ! -iname 'ngx_connection.c' ! -iname "ngx_solaris*" ! -iname "ngx_file_aio_read.c" ! -iname "ngx_thread_*" \
	! -iname "ngx_freebsd*" ! -iname "ngx_darwin_*" ! -iname "ngx_linux_aio_read.c" ! -iname "ngx_user.c" -regex '.*\.cpp\|.*\.c' )
#$(wildcard ./generated/*.cpp) 

SOURCES = $(NGX_SRC) $(shell find ./ -regex '.*\.cpp\|.*\.c') 

ifndef V
$(warning "********************* $(SOURCES) " )
endif

STATICLIBS=$(PCRE_HOME)/lib/libpcre.a $(OPENSSL_HOME)/lib/libssl.a $(OPENSSL_HOME)/lib/libcrypto.a
SHAREDLIBS = -luuid -ldl

include ngx.mak

INCLUDES=-I$(NGX_HOME)/src/core/ -I$(NGX_HOME)/src/os/unix/ -I$(NGX_HOME)/src/event/ -I./ -I$(PCRE_HOME)/include
#INCLUDES=-I$(ICE_HOME)/include -I$(MYSQLCON)/include -I$(PUBLICFUNC)/include -I$(LIBREDIS) -I$(REDISCLIENT)/ -I$(DATACACHEPRO)/include -I$(CURL)/include/curl -I$(BOOST) -I$(GLOG_HOME)/include -I$(COMMROOT)

GEN_OBJS	= $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(GEN_SRC))
#STATICLIBS += -L$(LOG4C_HOME)/lib/liblog4c.a

####### Build rules
$(TARGET): $(OBJECTS) $(CORE_MAKEFILE_LIST)
	@mkdir -p $(@D)
	@echo ************************ $(@D)
	#g++ -m64 -O0 -fPIC  $(LDFLAGS) -o lib$(TARGET).so $(OBJECTS) $(STATICLIBS) $(SHAREDLIBS) $(LIBS)
	g++ -o $(TARGET) $(OBJECTS) $(STATICLIBS) $(SHAREDLIBS) $(LIBS)

t: 
	LD_LIBRARY_PATH=.:$(ICE_HOME)/lib64:$(BENCHMARK_HOME)/lib:$(ZDB_HOME)/lib:$(GLOG_HOME)/lib:$(LD_LIBRARY_PATH):$(ZK_HOME)/lib ./client 10000 --benchmark_counters_tabular=true

gdb: 
	LD_LIBRARY_PATH=.:$(ICE_HOME)/lib64:$(BENCHMARK_HOME)/lib:$(ZDB_HOME)/lib:$(GLOG_HOME)/lib:$(LD_LIBRARY_PATH) gdb ./client


db:$(GEN_OBJS)
	g++  $(CXXFLAGS) $(INCLUDES) $(DEFINES) $(SHAREDLIBS) $(GEN_OBJS) -L$(ICE_HOME)/lib64 -lGlacier2 -lIce -lIceUtil -lFreeze -ldb_cxx-5.3 tests/Dbtest.cpp Classes/IceExt_IceClientUtil.cpp Classes/dbutil.cpp Classes/DefaultSequenceDao.cpp -I $(BENCHMARK_HOME)/include -L $(BENCHMARK_HOME)/lib  -lpthread -lbenchmark -o dbtest
	LD_LIBRARY_PATH=.:$(ICE_HOME)/lib64:$(BENCHMARK_HOME)/lib:$(ZDB_HOME)/lib:$(GLOG_HOME)/lib:$(LD_LIBRARY_PATH) ./dbtest 10 --benchmark_counters_tabular=true
##@echo "$(LOG_LINK)$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(SHAREDLIBS) $(STATICLIBS) $(LIBS)"
##$(LOG_LINK)$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(SHAREDLIBS) $(STATICLIBS) $(LIBS)

####### Compile
$(OBJ_DIR)/%.o: ../%.cpp $(CORE_MAKEFILE_LIST)
	@mkdir -p $(@D)
	@echo ************************ $(@D)
	#@echo "1:$(OBJ_DIR)/%.o: ../%.cpp $(CORE_MAKEFILE_LIST)"
	$(LOG_CXX)$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp $(CORE_MAKEFILE_LIST)
	@mkdir -p $(@D)
	@echo "2:$(LOG_CXX)$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@"
	$(LOG_CXX)$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJ_DIR)/%.o: %.c $(CORE_MAKEFILE_LIST)
	@mkdir -p $(@D)
	@echo "$(LOG_CC)$(CC) $(CCFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@"
	$(LOG_CC)$(CC) $(CCFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

GEN::
	$(ICE_HOME)/bin/slice2cpp $(shell find slice -regex '.*\.ice') --output-dir generated/

#$(ICE_HOME)/bin/slice2freeze --output-dir generated/ --dict StringSeqRangeMap,string,tddl::sequences::SequenceRangePO StringSeqRangeMap slice/tdd_sequence.ice

