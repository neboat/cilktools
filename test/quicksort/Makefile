TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = quicksort
ALL_TESTS = quicksort

default : $(ALL_TESTS)

$(TEST) : CC=$(CXX)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

# CFLAGS += -O0
# CXXFLAGS += -O0
