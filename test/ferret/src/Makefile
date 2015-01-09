TOOL_HOME ?= ../../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

VPATH = $(CURDIR)/src:$(CURDIR)/src/lsh:$(CURDIR)/tools:$(CURDIR)/image:$(CURDIR)/benchmark

ROOT = $(CURDIR)
SRCDIR = $(ROOT)/src
INCDIR = $(ROOT)/include

BINDIR = $(ROOT)/build/bin
LIBDIR = $(ROOT)/build/lib
OBJDIR = $(ROOT)/build/obj

TEST = $(BINDIR)/ferret-reducer
ALL_TESTS = $(TEST)

default : $(ALL_TESTS)

$(BINDIR) :
	if [ ! -e $(BINDIR) ]; then mkdir -p $(BINDIR); fi

$(LIBDIR) :
	if [ ! -e $(LIBDIR) ]; then mkdir -p $(LIBDIR); fi

$(OBJDIR) :
	if [ ! -e $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi

$(LIBDIR)/% : | $(LIBDIR)

$(OBJDIR)/% : | $(OBJDIR)

$(BINDIR)/% : | $(BINDIR)

# build the cass library
# $(LIBDIR)/libcass.a : | $(LIBDIR)

lib_src := $(wildcard src/*.c) $(wildcard src/lsh/*.c)
lib_src := $(notdir $(lib_src))
lib_obj := $(addprefix $(OBJDIR)/, $(lib_src:.c=.o))

# build the image support library
libimage_src := image.c extract.c edge.c srm.c
libimage_obj := $(addprefix $(OBJDIR)/, $(libimage_src:.c=.o))

# build the tool programs
tools_src := $(wildcard tools/*.c)
tools_tgt := $(notdir $(tools_src))
tools_tgt := $(addprefix $(BINDIR)/, $(tools_tgt:.c=))

$(tools_tgt) : $(LIBDIR)/libcass.a

$(image_tgt) : $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a

tools : $(tools_tgt)

image_tgt := cass_img_extract
image_tgt := $(addprefix $(BINDIR)/, $(image_tgt))

$(BINDIR)/ferret-reducer : $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a

include $(INCLUDE_DIR)/mk.common
include $(TOOL_HOME)/test/test.mk

LDLIBS += $(LIBS) -lrt -lm -lcass -lgsl -lgslcblas -ljpeg -lstdc++

$(BINDIR)/% : $(OBJDIR)/%.o
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIBDIR)/libcass.a : $(lib_obj)
	$(AR) rcs $@ $^

$(LIBDIR)/libcassimage.a : $(libimage_obj)
	$(AR) rcs $@ $^
