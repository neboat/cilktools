LIBCILKVIEW = $(LIB_DIR)/libcilkview.a
LIBCILKVIEW_PERF = $(LIB_DIR)/libcilkview_perf.a

CILKVIEW_SRC = cilkview.c cilkview_perf.c
CILKVIEW_OBJ = $(CILKVIEW_SRC:.c=.o)

-include $(CILKVIEW_OBJ:.o=.d)

CFLAGS += $(TOOL_CFLAGS)
LDFLAGS += $(TOOL_LDFLAGS)
LDLIBS += $(TOOL_LDLIBS)

ifeq ($(PARALLEL),1)
CFLAGS += -DSERIAL_TOOL=0 -fcilkplus
endif

.PHONY : cleancilkview

cilkview.o : # CFLAGS += -flto
cilkview.o : # LDFLAGS += -lrt

cilkview_perf.o : # CFLAGS += -flto
cilkview_perf.o : # LDFLAGS += -lrt -lpfm

# %_cv : LDFLAGS += -lrt -I $(LIB_DIR)
# %_cv : %.o $(LIBCILKVIEW)

# %_cvp : LDFLAGS += -lrt -lpfm
# %_cvp : %.o $(LIBCILKVIEW_PERF)

cleancilkview :
	rm -f *~ $(LIBCILKVIEW) $(LIBCILKVIEW_PERF) $(CILKVIEW_OBJ) $(CILKVIEW_OBJ:.o=.d*)
