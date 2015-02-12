BASENAME ?= $(shell basename $(CURDIR))

LIB_DIR = ../lib
INCLUDE_DIR = ../include

.PHONY : default clean

default:

include $(INCLUDE_DIR)/mk.common
include $(BASENAME).mk

TARGETS = $(LIBCILKPROF)

ifeq (1,$(OPT))
CFLAGS += -O3 -DNDEBUG
CXXFLAGS += -O3 -DNDEBUG
endif

default : $(TARGETS)

clean : clean$(BASENAME)
