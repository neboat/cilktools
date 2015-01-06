TARGETS := quicksort
QUICKSORT_SRC = quicksort.cpp

TOOL ?= CILKPROF

-include ../../include/mk.common
-include ../../include/tool.mk

CFLAGS += $(APPFLAGS)
CXXFLAGS += $(APPFLAGS)
INCLUDE_DIR = .

# quicksort.o : CXXFLAGS += -O3 -g -fcilkplus -I ../../include/

# quicksort_% : quicksort.o
# quicksort_% : LDFLAGS += -fcilkplus -ldl

quicksort : quicksort.o $(TOOL_TARGET)
	echo "Compiling for tool $(TOOL_TARGET)"
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

clean:
	-rm -f $(TARGET) *.o *.s
