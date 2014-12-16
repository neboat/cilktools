#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "cilksan.h"

int global = 0;

int serial_fib(int n) {

  if(n <= 0) { n = 1; } 
  int array[n+1];
  memset(array, 0, sizeof(array));
  
  array[0] = 0;
  array[1] = 1;
  for(int i=2; i<=n; i++) {
    array[i] = array[i-1] + array[i-2];
  }

  return array[n];
}

int serial_fib_with_ref(int *stk) {
  int n = *stk;
  return serial_fib(n);
}

void no_race() {
  int x = 20;
  int y = 20;
  int z = 0;
  x = cilk_spawn serial_fib(x);
  z++;
  y = cilk_spawn serial_fib(y);
  z++;
  z = cilk_spawn serial_fib(z);
  cilk_sync;
  z++;
}

void racy_update(int *stk_var) {
  *stk_var = cilk_spawn serial_fib(10); // racing on writing of *stk_var
  global = /* need to use a global; or this assignment gets optimized away */
        *stk_var + serial_fib(10); // racing on reading of *stk_var
  cilk_sync;
  
  return; 
} 

void race_with_param() {
  int x = 20;
  int y = 20;
  int z = 0;

  x = cilk_spawn serial_fib(x);
  y = cilk_spawn serial_fib(y); // race on writing of y
  z = cilk_spawn serial_fib(y); // race on reading of y
  cilk_sync;
  z++;
}

void race_with_param_ref() {
  int x = 0;
  int y = 20;
  int z = 20;

  x = cilk_spawn serial_fib_with_ref(&y);  // race on reading of y
                                           // in serial_fib_with_ref
  z = serial_fib(20);  
  int tmp = z;
  y = cilk_spawn serial_fib(tmp); // race on writing of y
  // no race on z
  cilk_sync;
  x++;
}

void race_with_return() {
  int x = 0;
  int y = 20;
  int z = 20;

  x = cilk_spawn serial_fib(y); // race on writing of x
  x = serial_fib(20); // race on writing of x 
  z = cilk_spawn serial_fib(z);
  cilk_sync;
  x++;
}

void race_with_cont() {
  int x = 0;
  x = cilk_spawn serial_fib(13); // write-read race on x
  global = // need to use a global, or this assignment gets optimized away
            x;
  cilk_sync;
}

void recur(int depth, int *stk) {
  if(depth == 0) {
    racy_update(stk);
  } 
  else recur(depth-1, stk);
}

void test_stack_depth(int depth) {

  int x = 0;
  recur(depth, &x);
  
  return; 
} 

void baz(int *x) {
  *x = 99;
}

void quack(int *x) {
  int l = *x;
  printf("quack: addr  is: %p, value: %d.\n", x, l);
}

int* foo(int input) {
  int *a = (int *) alloca(input*10);
  a[input*5] = input;
  int *addr = &(a[input*5]);
  printf("foo: addr is: %p, value: %d.\n", addr, *addr);
  a[input*5] = cilk_spawn serial_fib(5);
  cilk_sync;
  return addr;
}

void bar(int input, int *addr, int race) {
fprintf(stderr, "entering bar.\n");
  int *a = (int *) alloca(input*10);
  int ind = 0;
  printf("bar: addr is: %p.\n", addr);

  for(int i=0; i < input*10; i++) {
    if(addr == &(a[i])) {
      printf("bar: &a[%d] = %p.\n", i, &a[i]);
      ind = i;
      break;
    }
  }

  if(race) {
    cilk_spawn baz(&a[ind]);
    quack(&a[ind]);
    cilk_sync;
  }
}

// there should not be a race between foo and bar, but since we didn't 
// properly clean the shadow memory when we pop frames off the stack, 
// we got a bug where we can get false positive, and this is the test 
// case to trigger that.  
//
// Scenario: when a1 and a2 seem logically in parallel, but the access is not a 
// race due to cactus stack abstraction; in this case, we don't report a race,
// but we should replace a1 with a2.  Otherwise, another a3 could later come
// along that races with a2 but not a1.  
//
// Ex: A spawns B and C; C spawns D and F.  B access some address on its
// stack (call it a1).  D access the same address, which is allocated in C's
// stack (call it a2).  a1 and a2 don't race, because B and C has their own 
// cactus stack.  F can later access the same address (call it a3), and that 
// is a race, since D and F are in parallel, and they are both accessing C's
// stack (its parent's stack).  If we don't replace a1 with a2, we would not
// report a race on a3.  
//
// When param race == 0, we have A spawns B & C. No race between B & C.
// When param race == 1, we have A spawns B & C, and C spawns D and F.
// There is a race between D and F.
void tricky(int input, int race) {
fprintf(stderr, "entering tricky.\n");
  int *addr = cilk_spawn foo(input);
  __cilksan_disable_checking();
  // do this to avoid race being reported on writing and reading of addr
  int *addr_to_pass = addr;
  __cilksan_enable_checking();
  cilk_spawn bar(input, addr_to_pass, race); 
  cilk_sync;
}

int main(int argc, char* argv[]) {

  int x = 1;
  int which = 0;
  int input = 4;

  if (argc >= 2) {
    which = atoi(argv[1]);
    if (argc >= 3) {
      input = atoi(argv[2]);
    }
  }

  switch(which) {
    case 0:
      racy_update(&x); // write-read race
      assert(__cilksan_error_count() == 1);
      break;

    case 1:
    case 2:
    case 3:
      test_stack_depth(which); // write-read race
      assert(__cilksan_error_count() == 1);
      break;

    case 4:
      race_with_cont(); // write-read race
      assert(__cilksan_error_count() == 1);
      break;

    case 5:
      race_with_param(); // write-read race
      assert(__cilksan_error_count() == 1);
      break;

    case 6:
      race_with_param_ref(); // read-write race
      assert(__cilksan_error_count() == 1);
      break;

    case 7:
      race_with_return(); // write-write race
      assert(__cilksan_error_count() == 1);
      break;

    case 8:
      no_race();
      assert(__cilksan_error_count() == 0);
      break;

    case 9: // no race 
      // this test case is difficult to construct; extremely fragile.
      // need to run with input = 4 and CILK_NWORKERS = 1 to trigger it; 
      // somehow input = 8 causes Cilkscreen to core dump, and running 
      // with input = 40 core dumps even running normally with 1 worker.  
      // I think the stack gets corrupted somehow.  So, use input == 4 
      // for unit testing.
      tricky(input, 0);
      assert(__cilksan_error_count() == 0);
      break;

    case 10: // similar to 9, but now there is a race at the end
      tricky(input, 1);
      assert(__cilksan_error_count() == 1);
      break;
  }

  return 0;
}
