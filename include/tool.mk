CILKRTS_HOME = $(HOME)/sandbox/cilk_ok/cilkplus_rts
TOOL_HOME = $(HOME)/sandbox/cilk_ok/cilktools

CILKRTS_STATIC_LIB=$(CILKRTS_HOME)/lib/libcilkrts.a
# Use this for dynamic linking of cilkrts in LDFLAGS
# CILKRTS_DYNAMIC_LIB=-Wl,-rpath -Wl,$(CILKRTS_HOME)/lib -lcilkrts
LDFLAGS = $(CILKRTS_STATIC_LIB)
LDLIBS= -lrt -ldl -lpthread

ifeq ($(TOOL),CILKPROF)
	BASIC_CFLAGS += -DCILKPROF=1 -DCILKSAN=0
	APPFLAGS += -fcilktool-instr-c
        TOOLFLAGS += -I$(TOOL_HOME)/include
        TOOL_TARGET = $(TOOL_HOME)/cilkprof/cilkprof.o
else ifeq ($(TOOL),CILKSAN)
	TOOLFLAGS += -I$(TOOL_HOME)/cilksan -fsanitize=thread
	BASIC_CFLAGS += -DCILKPROF=0 -DCILKSAN=1
        TOOL_TARGET = $(TOOL_HOME)/cilksan/libcilksan.a
        # Use the following for dynamic linking
	# LDFLAGS += -Wl,-rpath -Wl,$(TOOL_HOME)/cilksan -L$(TOOL_HOME)/cilksan
	# LDLIBS += -lcilksan
else ifeq ($(TOOL),NULL)
	BASIC_CFLAGS += -DCILKPROF=0 -DCILKSAN=0
else
	echo "Error: unrecognized tool $(TOOL)"
endif
