LIBCILKPROF = $(LIB_DIR)/libcilkprof.a
CILKPROF_SRC = cilkprof.c span_hashtable.c comp_hashtable.c
CILKPROF_OBJ = $(CILKPROF_SRC:.c=.o)

-include $(CILKPROF_OBJ:.o=.d)

ifeq ($(PARALLEL),1)
CFLAGS += -DSERIAL_TOOL=0 -fcilkplus
endif

ifeq ($(TOOL),cilkprof)
LDFLAGS += -lrt
endif

.PHONY : cleancilkprof

default : $(LIBCILKPROF)
clean : cleancilkprof

$(LIBCILKPROF) : $(CILKPROF_OBJ)

cilkprof.o : # CFLAGS += -flto
cilkprof.o : # LDFLAGS += -lrt

cleancilkprof :
	rm -f $(LIBCILKPROF) $(CILKPROF_OBJ) $(CILKPROF_OBJ:.o=.d*) *~
