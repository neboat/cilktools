CILKSAN_TARGETS := libcilksan.so libcilksan.a
CILKSAN_SRC := cilksan.cpp debug_util.cpp mem_access.cpp driver.cpp print_addr.cpp
CILKSAN_OBJ := $(CILKSAN_SRC:.cpp=.o)

SUFFIXES += cs

LIB_CFLAGS += -fPIC
LIB_CXXFLAGS += $(LIB_CFLAGS)

ifeq ($(COMPILER),LLVM)
COMPILER_RT_DIR=$(COMPILER_ROOT)/src/projects/compiler-rt
LIB_CFLAGS += -I$(COMPILER_RT_DIR)/lib/ -I$(COMPILER_RT_DIR)/lib/cilk/include
LIB_CXXFLAGS += $(LIB_CFLAGS)
# else ifeq ($(COMPILER),GCC)
# COMPILER_ROOT = $(COMPILERS_HOME)/gcc-cilksan
# CC = $(COMPILER_ROOT)/bin/gcc
# CXX = $(COMPILER_ROOT)/bin/g++
# LIB_CFLAGS += -I $(COMPILER_ROOT)-src/libsanitizer -I$(COMPILER_ROOT)-src/libcilkrts/include 
# LIB_CXXFLAGS += $(LIB_CFLAGS)
endif

CFLAGS += $(LIB_CFLAGS)
CXXFLAGS += $(LIB_CXXFLAGS)

libcilksan.so: $(OBJ)
	$(CXX) $^ -shared -o $@

libcilksan.a: $(OBJ)
	ar rcs $@ $^ 

cleancilksan :
	rm -rf *.o *.d* $(CILKSAN_TARGETS) *~
