ifneq (,$(TOOL))
LDLIBS += -l$(TOOL)
endif

.PHONY : clean$(TEST)

$(TEST) : CFLAGS += $(APPFLAGS)
$(TEST) : CXXFLAGS += $(APPFLAGS)

clean : clean$(TEST)

clean$(TEST) :
	rm -f $(TEST) *.o *.d* *~
