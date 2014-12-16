// expect a race on heap-allocated data
// Angelina: I don't see any race ...
#include "cilksan.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

int y[16];

void bar(char *x) {
    x[0]++;
}    

void foo(void) {
    char *x = malloc(16);
    bar(x); // if you do a malloc, then a store, then a free, the compiler might optimize it all away.  So I do the store inside bar.
    free(x);
}

int main() {
    _Cilk_spawn foo();
    _Cilk_spawn foo();
    _Cilk_sync;
    printf("error count = %d\n", __cilksan_error_count());
    assert(__cilksan_error_count() == 0);

    return 0;
}
