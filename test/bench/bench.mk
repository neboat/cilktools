# -*- mode: makefile-gmake; -*-
#TARGETS := fib fib-serial fibx cholesky cilksort fft heat knapsack lu \
#matmul nqueens rectmul strassen qsort foo throw

TARGETS := fib fib-serial fibx cholesky cilksort fft heat knapsack lu \
matmul nqueens rectmul strassen qsort foo throw

COMPILER ?= LLVM
TOOL ?= CILKPROF

TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

include $(INCLUDE_DIR)/mk.common
include $(INCLUDE_DIR)/tool.mk

CFLAGS += $(APP_CFLAGS)
CXXFLAGS += $(APP_CFLAGS)

INCLUDE_DIR = .

fib: fib.o
fib-serial: fib-serial.o 
fibx: fibx.o
cholesky: getoptions.o cholesky.o
cilksort: getoptions.o cilksort.o
fft: getoptions.o fft.o
heat: getoptions.o heat.o
knapsack: getoptions.o knapsack.o
lu: getoptions.o lu.o
matmul: getoptions.o matmul.o
nqueens: getoptions.o nqueens.o
rectmul: getoptions.o rectmul.o
strassen: getoptions.o strassen.o
loop-matmul: loop-matmul.o
qsort: qsort.o

%: %.o $(TOOL_TARGET)
	echo "Compiling for tool $(TOOL_TARGET)"
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

clean::
	-rm -f $(TARGETS) *.o *.s
