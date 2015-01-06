ifeq ($(TOOL),cilkview)
LDLIBS += -lcilkview -lrt
else ifeq ($(TOOL),cilkview_perf)
LDLIBS += -lcilkview_perf -lrt -lpfm
endif
