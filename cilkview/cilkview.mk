TARGETS := cilkview.o cilkview_perf.o
SRC := cilkview.c cilkview_perf.c

-include ../include/mk.common

cilkview.o : CFLAGS += -flto
cilkview.o : LDFLAGS += -lrt

cilkview_perf.o : CFLAGS += -flto
cilkview_perf.o : LDFLAGS += -lrt -lpfm

clean :
	rm -f *~ *.o
