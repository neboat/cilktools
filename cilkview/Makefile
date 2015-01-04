BASENAME ?= $(shell basename $(CURDIR))

LIB_DIR = ../lib
TARGETS = $(LIBCILKVIEW)

-include ../include/mk.common
-include $(BASENAME).mk

.PHONY : default clean

default : $(TARGETS)

clean : clean$(BASENAME)
