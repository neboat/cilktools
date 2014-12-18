TEST := quicksort
SRC := quicksort.cpp

-include ../../include/mk.common

quicksort.o : CXXFLAGS += -O3 -fcilkplus -I ../../include/

quicksort_% : quicksort.o
quicksort_% : LDFLAGS += -fcilkplus -ldl
