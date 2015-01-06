TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = quicksort

default : $(TEST)

$(TEST) : CC=$(CXX)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

-include $(TEST).d
