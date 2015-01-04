TEST = quicksort
QUICKSORT_SRC = quicksort.cpp

-include ../../include/mk.common

quicksort.o : CXXFLAGS += -O3 -g -fcilkplus -I ../../include/

quicksort_% : quicksort.o
quicksort_% : LDFLAGS += -fcilkplus -ldl
