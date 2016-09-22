TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TARGETS := fib fib-serial fibx cholesky cilksort fft heat knapsack lu \
matmul nqueens rectmul strassen qsort foo throw

default : $(TARGETS)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

CFLAGS += -O3
CXXFLAGS += -O3

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

# These are not actual bench; just here for testing; can remove later
foo: foo.o
throw: throw.o

ast: foo.c
	$(CC) $(CFLAGS) -fsanitize=thread -Xclang -ast-dump -fsyntax-only $^	
