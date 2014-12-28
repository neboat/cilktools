CILKSAN = redrace

CC = ../../../bin/gcc 
CXX = ../../../bin/g++ 
CFLAGS = -O3 -g -Werror -fcilkplus

TOOL_FLAGS = -I$(CURDIR)/../$(CILKSAN) -fsanitize=thread -fcilktool 
LDFLAGS = -fcilkplus -g -Wl,-rpath -Wl,$(CURDIR)/../../../lib64 -Wl,-rpath -Wl,$(CURDIR)/../$(CILKSAN) -L../$(CILKSAN)
LDLIBS = -ldl -lcilksan

ALL = fib 

all: $(ALL)

fib: fib.o
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS)

fib.o: fib.cpp
	$(CXX) -c -o $@ $(CFLAGS) $(TOOL_FLAGS) $<

clean:
	rm -f $(ALL) *.o *~
