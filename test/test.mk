ifneq (,$(TOOL))
TOOL_LC = $(shell echo $(TOOL) | tr A-Z a-z)
include $(TOOL_HOME)/$(TOOL_LC)/appflags.mk
endif

.PHONY : clean$(TEST)

$(TEST) : CFLAGS += $(APPFLAGS)
$(TEST) : CXXFLAGS += $(APPFLAGS)

clean : clean$(TEST)

clean$(TEST) :
	rm -f $(TEST) *.o *.d* *~
