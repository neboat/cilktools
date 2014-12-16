#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "cilksan.h"

int global = 0;
int *global_addr = 0;

int serial_fib(int n) {

  if( n <=0 ) { n = 1; }
  int array[n+1];
  memset(array, 0, sizeof(array));
  
  array[0] = 0;
  array[1] = 1;
  for(int i=2; i<=n; i++) {
    array[i] = array[i-1] + array[i-2];
  }

  return array[n];
}

void test_spawn_in_loop(int iter, int *res) {

  for(int i=1; i <= iter; i++) {
    *res = cilk_spawn serial_fib(i%30);
  }
  cilk_sync;
  printf("res is %d.\n", *res);
}

void test_spawn_in_loop_on_stack(int iter) {

  int res;
  for(int i=1; i <= iter; i++) {
    res = cilk_spawn serial_fib(i%30);
  }
  cilk_sync;
  printf("res is %d.\n", res);
}

void test_cilk_for(int iter) {

  cilk_for(int i=1; i <= iter; i++) {
    __attribute__((unused)) int res;
    res = serial_fib(i);
  }
}

void cilk_for_body(int index) {
  printf("Execute iter %d.\n", index);
}

void manual_cilk_for(int low, int high) {
tail_recur:
  int count = high - low;
  
  if( count > 1 ) {
    int mid = low + count / 2;
    cilk_spawn manual_cilk_for(low, mid);
    low = mid;
    goto tail_recur;
  }

  // execute the body otherwise
  cilk_for_body(low);
}

void test_manual_cilk_for(int iter) {

  manual_cilk_for(0, iter);
}

void test_cilk_for_race(int iter) {

  int x = 0;
  int res = 0;
  cilk_for(int i=1; i <= iter; i++) {
    res = serial_fib(i);
    if(i == 3 || i == 4) x++;
  }
  global = x;

  fprintf(stderr, "x (%p) is: %d.\n", &x, x);
  // without this print, the race on res is reported
  // to be on a line somewhere inside serial_fib
  fprintf(stderr, "res (%p) is: %d.\n", &res, res);
}

int main(int argc, char* argv[]) {

  int n = 10;
  int which = 0;
  int *heap_int = (int *) malloc(sizeof(int));
  fprintf(stderr, "XXX heapint: %p.\n", heap_int);
  *heap_int = 0;

  if (argc >= 2) {
    which = atoi(argv[1]);
    if(argc >= 3) {
      n = atoi(argv[2]);
    }
  }
  switch(which) {
    case 0:
      test_spawn_in_loop(n, heap_int);   // write-write race
      assert(__cilksan_error_count() == 1);
      break;
    case 1:
      test_cilk_for(n);        // no race
      assert(__cilksan_error_count() == 0);
      break;
    case 2:
      test_manual_cilk_for(n); // no race
      assert(__cilksan_error_count() == 0);
      break;
    case 3:
      test_cilk_for_race(n);   // write-read race
      assert(__cilksan_error_count() == 3);
      break;
    case 4:
      test_spawn_in_loop_on_stack(n);   // write-write race
      assert(__cilksan_error_count() == 1);
      break;
  }
  fprintf(stderr, "res: %d.\n", *heap_int);
  free(heap_int);

  return 0;
}
