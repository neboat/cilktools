#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

class Foo {
public: 
  void loop(int n) {
    for(int i=0; i < n; i++) {
      if(i == 5) {
        return;
      } else {
        printf("Loop: %d.\n", i);
      }
    } 
  }
};

static int quack(int x) {
  while(x > 0) {
    x--;
    if(x == 2) { 
      printf("In quack, quitting in the middle of loop.\n");
      return x; }
  }

  printf("Leaving quack.\n");
  return 0;
}

static int bar(int x) {
  if(x > 3) {
    throw "Exception!.";
  } else {
    printf("In bar, taken else.\n");
    quack(x);
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
  try {
    bar(n);
  } catch(const char *msg) {
    fprintf(stderr, "%s.\n", msg);
  }

  Foo foo;
  foo.loop(n);

  printf("%d.\n", ret);
 
  return 0; 
}
