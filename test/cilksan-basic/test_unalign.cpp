#include <assert.h>
#include <stdio.h>
#include <cilk/cilk.h>

#include "cilksan.h"

int global;

int test1(char *arr) {

  int *x = (int *) &arr[2];
  *x = 3;

  return 0;
}

int test2(char *arr) {

  int *x = (int *) &arr[0];
  global = *x;

  return 0;
}

int main(void) {

  char arr[8] = {'0', '1', '0', '0', '0', '1', '6', '7'};

  fprintf(stderr, "arr is %p.\n", arr);
  cilk_spawn test1(arr);
  test2(arr);
  cilk_sync;
  assert(__cilksan_error_count() == 1);

  return 0;
}
