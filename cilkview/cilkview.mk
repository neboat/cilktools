TARGETS := cilkview.o cilkview_perf.o
SRC := cilkview.c cilkview_perf.c

-include ../include/mk.common

ifeq ($(PARALLEL),1)
CFLAGS += -DSERIAL_TOOL=0 -fcilkplus
endif

cilkview.o : # CFLAGS += -flto
cilkview.o : # LDFLAGS += -lrt

%_cv : LDFLAGS += -lrt

cilkview_perf.o : # CFLAGS += -flto
cilkview_perf.o : # LDFLAGS += -lrt -lpfm

%_cvp : LDFLAGS += -lrt -lpfm

clean :
	rm -f *~ *.o
