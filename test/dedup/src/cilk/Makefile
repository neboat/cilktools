TOOL_HOME ?= ../../../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

DEDUP_OBJ = decoder.o
OTHER_OBJ = hashtable.o util.o rabin.o sha.o ktiming.o
UTIL_OBJ = $(addprefix util/, $(OTHER_OBJ))
REDUCER_OBJ = encoder-reducer.o dedup-reducer.o

TEST = dedup-reducer
ALL_TESTS = dedup-reducer

dedup-reducer : $(DEDUP_OBJ) $(OTHER_OBJ) $(UTIL_OBJ) $(REDUCER_OBJ)

default : $(ALL_TESTS)

include $(INCLUDE_DIR)/mk.common
include $(TOOL_HOME)/test/test.mk

OPENSSL_INCLUDE_DIR = ../openssl

DEDUP_CFLAGS += -Wall -D_XOPEN_SOURCE=600 -I $(OPENSSL_INCLUDE_DIR) -O3 -DNDEBUG -g 
DEDUP_LDLIBS += -lm -lrt

# Uncomment the following to enable gzip compression
DEDUP_CFLAGS += -DENABLE_GZIP_COMPRESSION
DEDUP_LDLIBS += -lz

DEDUP_CFLAGS += -DCILK -DHAVE_CONFIG_H -O3

CFLGAS += $(DEDUP_CFLAGS)
CXXFLAGS += $(DEDUP_CFLAGS)
LDLIBS += $(DEDUP_LDLIBS)

dedup-reducer.o : dedup.c
	$(CXX) $(CXXFLAGS) -c $< -o $@
