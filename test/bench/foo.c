#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

static int bar(int x) {
  if(x > 3) {
    printf("In bar, taken if.\n");
    return x-1;
  } else {
    printf("In bar, taken else.\n");
    return 0;
  }
}

static int foo(int x) {
  if(x == 1) return 1;

  int ret = 0;
  ret = cilk_spawn foo(x-1);
  cilk_sync;

  return ret + x;
}

int main(int argc, char *argv[]) { 
  int n = 0;
  if(argc > 1) {
    n = atoi(argv[1]);
  }
  int ret = foo(3);
  bar(n);
  printf("%d.\n", ret);
 
  return 0; 
}
