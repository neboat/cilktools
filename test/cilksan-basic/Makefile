# CILKSAN=redrace
CILKSAN=cilksan

BUILDDIR=../../../../../../llvm-cilkok
# BUILDDIR=../../../../../../llvm-cilk

CC=$(BUILDDIR)/bin/clang
CXX=$(BUILDDIR)/bin/clang++
BASIC_CFLAGS=-W -Wall -g -O0 -Werror
BASIC_CXXFLAGS=$(BASIC_CFLAGS)
# TOOLFLAGS = -I$(CURDIR)/../$(CILKSAN) -fsanitize=thread -fcilkplus -fcilktool
TOOLFLAGS = -I$(CURDIR)/../../cilktools/$(CILKSAN) -fsanitize=thread -fcilkplus # -fcilktool
CFLAGS=$(BASIC_CFLAGS) -std=c11 $(TOOLFLAGS)
CXXFLAGS=$(BASIC_CXXFLAGS) -std=c++11 $(TOOLFLAGS)
#LDFLAGS=-fcilkplus -g -Wl,-rpath -Wl,$(BUILDDIR)/lib -Wl,-rpath -Wl,$(BUILDDIR)/bin/lib/clang/3.4.1/lib -W1,-rpath -Wl,$(CURDIR)/../../cilktools/$(CILKSAN) -L$(CURDIR)/../../cilktools/$(CILKSAN)
LDFLAGS=-g -Wl,-rpath,$(CURDIR)/../../cilktools/$(CILKSAN) -L$(CURDIR)/../../cilktools/$(CILKSAN)
LDLIBS=-lcilkrts -ldl -lcilksan
TESTS =  test1 \
 test2 \
 test3 \
 test4 \
 test5 \
 test6 \
 test7 \
 test8 \
 test9 \
 test10 \
 test11 \
 test_bags \
 test_loop \
 test_mem \
 test_mem_list \
 test_stack \
 test_stack_mem \
 test_unalign \
 test_ploop 
# test_static \
 missing \
 missing_c \
 symbolize-test \
 # Use one item per line to make version control merging easier.
 # test_stack \
 # test_bags \
 # test_mem \
 # test_unalign \
 # test_mem_list \
# Use one item per line to make version control merging easier.

default: $(TESTS)

# $(TESTS): | library
# .PHONY: library
# library:
# 	cd ../$(CILKSAN); make

test1: test1.o
test2: test2.o
test3: test3.o
test4: test4.o
test5: test5.o
test6: test6.o
test7: CFLAGS+=-std=c99
test7: test7.o
test8: test8.o
test9: test9.o
test10: CFLAGS+=-std=c99
test10: test10.o
test11: test11.o

test_openmp: CFLAGS+=-std=c99 -fopenmp 
test_openmp: LDLIBS+=-std=c99 -fopenmp 
test_openmp: test_openmp.o

test_stack: test_stack.o
test_stack_mem: test_stack_mem.o
test_bags: test_bags.o
test_loop: test_loop.o
test_ploop: test_ploop.o
test_mem: test_mem.o
test_mem_list: test_mem_list.o
test_unalign: test_unalign.o

%.s: %.cpp
	$(CXX) -S $^ -fverbose-asm $(CXXFLAGS)  
%.s: %.c
	$(CC) -S $^ -fverbose-asm $(CFLAGS)  
test_openmp.s: test_openmp.c
	$(CC) -S $^ -std=c99 -fopenmp -fverbose-asm $(CFLAGS)  

%: %.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@ 

test_static: test_static.o


missing: missing.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@ 
missing_c: missing_c.o
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@ 

symbolize-test-lib.o: CFLAGS+=-fsanitize=thread -fPIC
libsymbolize-test.so: symbolize-test-lib.o
	$(CC) $< -shared -o $@
symbolize-test.o: symbolize-test.c
symbolize-test: LDFLAGS += -L. -Wl,-rpath,.
symbolize-test: LDLIBS += -lsymbolize-test
symbolize-test: symbolize-test.o libsymbolize-test.so
	$(CC) $< $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf *.s *.o $(TESTS) libsymbolize-test.so *~
