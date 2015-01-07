TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = fib

default : $(TEST)

# fib_split : CFLAGS += $(APP_CFLAGS)
# fib_split : CXXFLAGS += $(APP_CFLAGS)
# fib_split : LDFLAGS += $(APP_LDFLAGS)
# fib_split : LDLIBS += $(APP_LDLIBS)

fib_split : fib_split.o

include $(INCLUDE_DIR)/mk.common
include ../test.mk

-include $(TEST).d
