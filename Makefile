TOOLS = cilkprof cilksan cilkview viewread
TESTS_DIR = test
VPATH = $(TOOLS)

LIB_DIR = ./lib
INCLUDE_DIR = ./include

LIBRARIES = $(patsubst %,$(LIB_DIR)/lib%.a,$(TOOLS))

.PHONY : default clean

default :

include $(INCLUDE_DIR)/mk.common

ifneq (,$(TOOL))
default : $(LIB_DIR)/lib$(TOOL).a
TOOL_LC = $(shell echo $(TOOL) | tr A-Z a-z)
include $(TOOL_LC)/$(TOOL_LC).mk
else
$(foreach tool,$(TOOLS),$(eval -include $(tool)/$(tool).mk))
endif

clean : $(patsubst %,clean%,$(TOOLS))
