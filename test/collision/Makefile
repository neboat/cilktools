LIB_DIR = ../../lib
INCLUDE_DIR = ../../include

TEST = collision

COLLISION_SRC = MultiCoreCollisionDetection.cpp aabbtree.cpp meshprocess.cpp tritri.cpp vector.cpp vrmlParser.cpp
COLLISION_OBJ = $(COLLISION_SRC:.cpp=.o)

.PHONY : cleanhypervector_test

default : $(TEST)

-include $(INCLUDE_DIR)/mk.common
-include ../test.mk

collision : CXXFLAGS += $(APPFLAGS)
collision : $(COLLISION_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

hypervector_test : CXXFLAGS += $(APPFLAGS)
hypervector_test : hypervector_test.cpp

clean : cleanhypervector_test

cleanhypervector_test :
	rm -f hypervector_test *.o *~
