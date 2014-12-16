#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "cilksan.h" 

int global = 0;

static int serial_fib(int n) {

  if( n <= 0 ) { n = 1; }
  int array[n+1];
  memset(array, 0, sizeof(array));
  
  array[0] = 0;
  array[1] = 1;
  for(int i=2; i<=n; i++) {
    array[i] = array[i-1] + array[i-2];
  }

  return array[n];
}

static int serial_fib_with_ref(int *stk) {
  int n = *stk;
  return serial_fib(n);
}

static int three_way(int n, int depth, bool spawn) {
  int x = 0, y = 0, z = 0;

  if( depth == 0 ) {
    x = serial_fib(n-1); 
    y = serial_fib(n-2); 
    z = serial_fib(n-3); 

  } else {
    if( !spawn ) {
      x = three_way(n-1, depth-1, 0);
    } else {
      x = three_way(n-1, depth-1, 0);
      y = cilk_spawn three_way(n-2, depth-1, 0);
      z = three_way(n-3, depth-1, 1);
    }
    cilk_sync;
  }
  int res = x + y + z;

  return res;
}

static int 
three_way_with_read_write_race(int n, int depth, bool spawn, int *stk) {
  int x = 0, y = 0, z = 0;

  if( depth == 0 ) {
    x = serial_fib(n-1); 
    y = serial_fib(n-2); 
    z = serial_fib_with_ref(stk); 

  } else {
    if( !spawn ) {
      x = three_way_with_read_write_race(n-1, depth-1, 0, stk);
    } else {
      x = three_way_with_read_write_race(n-1, depth-1, 0, stk);
      y = cilk_spawn three_way_with_read_write_race(n-2, depth-1, 0, stk);
      if(depth == 1) {
          *stk = 1;
      }
      z = three_way_with_read_write_race(n-3, depth-1, 1, stk);
    }
    cilk_sync;
  }
  int res = x + y + z;

  return res;
}

static int 
three_way_with_write_write_race(int n, int depth, int *stk) {
  int x = 0, y = 0, z = 0;

  if( depth == 0 ) {
    x = serial_fib(n-1); 
    y = serial_fib(n-2); 
    z = serial_fib(n-3); 

  } else {
      x = three_way_with_write_write_race(n-1, depth-1, stk);
      y = cilk_spawn three_way_with_write_write_race(n-2, depth-1, stk);
      if(depth == 1) {
          *stk = 1;
      }
      z = three_way_with_write_write_race(n-3, depth-1, stk);
      cilk_sync;
  }
  int res = x + y + z;

  return res;
}

int main(int argc, char* argv[]) {

  int which = 0;
  int stk = 9;

  if (argc >= 2) {
    which = atoi(argv[1]);
  }

  switch(which) {
    case 0:
      // should not have race if the SP-bags maintained correctly
      global = three_way(12, 4, 1); 
      assert(__cilksan_error_count() == 0);
      break;

    case 1:
      // should report read / write race 
      global = three_way_with_read_write_race(12, 1, 1, &stk); 
      assert(__cilksan_error_count() == 1);
      break;

    case 2:
      // should report write / write race 
      global = cilk_spawn three_way_with_write_write_race(12, 3, &stk); 
      assert(__cilksan_error_count() == 1);
      break;
  }

  return 0;
}
