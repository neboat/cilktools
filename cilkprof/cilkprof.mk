TARGETS := cilkprof.o
SRC := cilkprof.c

-include ../include/mk.common

cilkprof.o : CFLAGS += -flto
cilkprof.o : LDFLAGS += -lrt

clean :
	rm -f *~ *.o
