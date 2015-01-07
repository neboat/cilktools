TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

BFS_SRC = bfs.cpp graph.cpp bag.cpp
BFS_OBJ = $(BFS_SRC:.cpp=.o)

TEST = bfs

default : $(TEST)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

-include $(TEST).d

# CFLAGS += -O3
