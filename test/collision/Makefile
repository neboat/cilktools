TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = collision

COLLISION_SRC = MultiCoreCollisionDetection.cpp aabbtree.cpp meshprocess.cpp tritri.cpp vector.cpp vrmlParser.cpp
COLLISION_OBJ = $(COLLISION_SRC:.cpp=.o)

.PHONY : cleanhypervector_test

default : $(TEST)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

-include $(COLLISION_OBJ:.o=.d)

collision : $(COLLISION_OBJ)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

hypervector_test : CXXFLAGS += $(APP_CFLAGS)
hypervector_test : LDFLAGS += $(APP_LDLAGS)
hypervector_test : LDLIBS += $(APP_LDLIBS)
hypervector_test : hypervector_test.cpp

clean : cleanhypervector_test

cleanhypervector_test :
	rm -f hypervector_test *.o *~
