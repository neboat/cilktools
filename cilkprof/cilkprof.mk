TARGETS := cilkprof.o
SRC := cilkprof.c

-include ../include/mk.common

ifeq ($(PARALLEL),1)
CFLAGS += -DSERIAL_TOOL=0 -fcilkplus
endif

INCLUDE_DIR=./../include
cilkprof.o : # CFLAGS += -flto
cilkprof.o : # LDFLAGS += -lrt

%_cp : LDFLAGS += -lrt
%_cp : %.o cilkprof.o

clean :
	rm -f *~ *.o
