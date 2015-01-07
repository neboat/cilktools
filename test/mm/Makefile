TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = mm_dac

default : $(TEST)

mm_dac_coarsen : mm_dac_coarsen.o
mm_dac_coarsen_transpose : mm_dac_coarsen_transpose.o

include $(INCLUDE_DIR)/mk.common
include ../test.mk

CFLAGS += -O3 -Dn=512

-include $(TEST).d
