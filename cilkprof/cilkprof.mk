LIBCILKPROF = $(LIB_DIR)/libcilkprof.a

CILKPROF_SRC = cilkprof.c # cc_hashtable.c util.c iaddrs.c # functions.c call_sites.c # SFMT-src-1.4.1/SFMT.c # strand_hashtable.c 
CILKPROF_OBJ = $(CILKPROF_SRC:.c=.o)

-include $(CILKPROF_OBJ:.o=.d)

CFLAGS += $(TOOL_CFLAGS)
LDFLAGS += $(TOOL_LDFLAGS)
LDLIBS += $(TOOL_LDLIBS)

ifneq ($(BURDENING),)
CFLAGS += -DBURDENING=$(BURDENING)
endif

ifeq ($(PARALLEL),1)
CFLAGS += -DSERIAL_TOOL=0 -fcilkplus # -I SFMT-src-1.4.1/
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

# SFMT-src-1.4.1/SFMT.o: CFLAGS += -DSFMT_MEXP=19937 -DHAVE_SSE2

# SFMT-src-1.4.1/%.o : SFMT-src-1.4.1/%.c
# 	$(CC) $(CFLAGS) -o $@ -c $<

# cc_hashtable.o: CFLAGS += -DSFMT_MEXP=19937 -DHAVE_SSE2
# call_sites.o: CFLAGS += -DSFMT_MEXP=19937 -DHAVE_SSE2

cleancilkprof :
	rm -f $(LIBCILKPROF) $(CILKPROF_OBJ) $(CILKPROF_OBJ:.o=.d*) *~
