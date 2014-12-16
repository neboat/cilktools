BUILDDIR=../../../../../build/Debug+Asserts
COMPILERRTDIR=../../../../../projects/compiler-rt
CC=$(BUILDDIR)/bin/clang
CXX=$(BUILDDIR)/bin/clang++
BASIC_CFLAGS=-g -O3 #-W -Wall -Werror
BASIC_CXXFLAGS=$(BASIC_CFLAGS)

#LIB_CFLAGS=-fPIC ../../libsanitizer/ -I../../libcilkrts/include 
LIB_CFLAGS=-fPIC -I$(COMPILERRTDIR)/lib/ -I$(COMPILERRTDIR)/lib/cilk/include 
LIB_CXXFLAGS=$(LIB_CFLAGS)

CFLAGS=-std=gnu11 $(BASIC_CFLAGS) $(LIB_CFLAGS)
CXXFLAGS=-std=gnu++11 $(BASIC_CXXFLAGS) $(LIB_CXXFLAGS)

# HELPER_CODE=debug_util.h disjointset.h metacall.h spbag.h stack.h
LIB_OBJ=debug_util.o mem_access.o cilksan.o driver.o print_addr.o
default: libcilksan.so

# Each object file should have a corresponding file of prerequisites.
# Include the prerequisites for each of our existing object files.
-include $(LIB_OBJ:.o=.d)

# This rule generates a file of prerequisites (i.e., a makefile)
# called name.d from a C source file called name.c.
%.d : %.c
	@set -e; rm -f $@; \
	$(CC) -MM -MP -MF $@.$$$$ $(CFLAGS) $< ; \
	sed -e 's|\($*\)\.o[ :]*|\1.o $@ : |g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


# This rule generates a file of prerequisites (i.e., a makefile)
# called name.d from a CPP source file called name.cpp.
%.d : %.cpp
	@set -e; rm -f $@; \
	$(CXX) -MM -MP -MF $@.$$$$ $(CXXFLAGS) $< ; \
	sed -e 's|\($*\)\.o[ :]*|\1.o $@ : |g' < $@.$$$$ > $@ ; \
	rm -f $@.$$$$

# mem_access.o: debug_util.h mem_access.h
# driver.o: cilksan_internal.h debug_util.h
# cilksan.o: debug_util.o mem_access.o $(HELPER_CODE) cilksan_internal.h debug_util.h
libcilksan.so: $(LIB_OBJ)
	$(CXX) $^ -shared -o $@

clean:
	rm -rf *.o *.d* *.so *~
