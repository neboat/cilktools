TOOLS = cilkprof cilksan cilkview viewread
TESTS_DIR = test
VPATH = $(TOOLS)

LIB_DIR = ./lib
INCLUDE_DIR = ./include

LIBRARIES = $(patsubst %,$(LIB_DIR)/lib%.a,$(TOOLS))

.PHONY : default clean

default :

-include $(INCLUDE_DIR)/mk.common

$(foreach tool,$(TOOLS),$(eval -include $(tool)/$(tool).mk))

clean : $(patsubst %,clean%,$(TOOLS))
