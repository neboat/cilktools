BASENAME ?= $(shell basename $(CURDIR))

LIB_DIR = ../lib
INCLUDE_DIR = ../include

TARGETS = $(LIBVIEWREAD)

.PHONY : default clean

default : $(TARGETS)

clean : clean$(BASENAME)

-include ../include/mk.common
-include $(BASENAME).mk
