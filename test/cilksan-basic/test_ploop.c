#include <stdio.h>
#include <stdlib.h>

#include <cilk/cilk.h>
#include <cilk/cilktool.h>

int main(int argc, char *argv[]) {
  int n = 1024*1024;

  if (argc > 1) {
    n = atoi(argv[1]);
  }

  int x = 0;
  cilk_for (int i = 0; i < n; ++i) {
    x++;
  }

  printf("x = %d\n", x);
}

