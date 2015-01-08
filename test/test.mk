ifneq (,$(TOOL))
TOOL_LC = $(shell echo $(TOOL) | tr A-Z a-z)
include $(TOOL_HOME)/$(TOOL_LC)/appflags.mk
endif

.PHONY : default clean clean$(TEST)

# $(TEST).d : CFLAGS += $(APP_CFLAGS)
# $(TEST).d : CXXFLAGS += $(APP_CFLAGS)

CFLAGS += $(APP_CFLAGS)
CXXFLAGS += $(APP_CFLAGS)
LDFLAGS += $(APP_LDFLAGS)
LDLIBS += $(APP_LDLIBS)

% : %.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean : clean$(TEST)

clean$(TEST) :
	rm -f $(ALL_TESTS) *.o *.d* *~

-include $(patsubst %,%.d, $(ALL_TESTS))
