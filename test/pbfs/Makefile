CC=clang++
CFLAGS= -g -Wall -O3 -fcilkplus -ldl  # -Werror -falign-functions
LDFLAGS= -L$(CURDIR)
AR=ar

all: bfs

bfs : bfs.cpp graph.cpp graph.h bag.cpp bag.h util.h Makefile
	$(CC) $(CFLAGS) $@.cpp $(LDFLAGS) -o $@

clean :
	rm -f bfs *~
