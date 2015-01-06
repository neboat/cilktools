ifneq (,$(TOOL))
TOOL_LC = $(shell echo $(TOOL) | tr A-Z a-z)
include $(TOOL_HOME)/$(TOOL_LC)/appflags.mk
endif

.PHONY : default clean clean$(TEST)

$(TEST).d : CFLAGS += $(APP_CFLAGS)
$(TEST).d : CXXFLAGS += $(APP_CFLAGS)

$(TEST) : CFLAGS += $(APP_CFLAGS)
$(TEST) : CXXFLAGS += $(APP_CFLAGS)
$(TEST) : LDFLAGS += $(APP_LDFLAGS)
$(TEST) : LDLIBS += $(APP_LDLIBS)

# Need to use C++ to perform static linking with Cilk runtime
$(TEST) : CC=$(CXX)

clean : clean$(TEST)

clean$(TEST) :
	rm -f $(TEST) *.o *.d* *~
