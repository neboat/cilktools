LIBCILKSAN := $(LIB_DIR)/libcilksan.a
LTLIBCILKSAN := $(LIB_DIR)/libcilksan.so

CILKSAN_SRC := cilksan.cpp debug_util.cpp mem_access.cpp driver.cpp print_addr.cpp
CILKSAN_OBJ := $(CILKSAN_SRC:.cpp=.o)

CILKSAN_CFLAGS = $(TOOL_CFLAGS) -fPIC
CILKSAN_CXXFLAGS = $(CILKSAN_CFLAGS)

LDFLAGS += $(TOOL_LDFLAGS)
LDLIBS += $(TOOL_LDLIBS)

ifeq ($(COMPILER),LLVM)
COMPILER_RT_DIR=$(COMPILER_ROOT)/src/projects/compiler-rt
CILKSAN_CFLAGS += -I$(COMPILER_RT_DIR)/lib/ -I$(COMPILER_RT_DIR)/lib/cilk/include
CILKSAN_CXXFLAGS += $(LIB_CFLAGS)
# else ifeq ($(COMPILER),GCC)
# COMPILER_ROOT = $(COMPILERS_HOME)/gcc-cilksan
# CC = $(COMPILER_ROOT)/bin/gcc
# CXX = $(COMPILER_ROOT)/bin/g++
# LIB_CFLAGS += -I $(COMPILER_ROOT)-src/libsanitizer -I$(COMPILER_ROOT)-src/libcilkrts/include 
# LIB_CXXFLAGS += $(LIB_CFLAGS)
endif

.PHONY : cleancilksan

$(LTLIBCILKSAN) $(LIBCILKSAN) : $(CILKSAN_OBJ)
$(LTLIBCILKSAN) $(LIBCILKSAN) : CFLAGS += $(CILKSAN_CFLAGS)
$(LTLIBCILKSAN) $(LIBCILKSAN) : CXXFLAGS += $(CILKSAN_CXXFLAGS)

cleancilksan :
	rm -rf *.o *.d* $(LIBCILKSAN) $(LTLIBCILKSAN) *~
