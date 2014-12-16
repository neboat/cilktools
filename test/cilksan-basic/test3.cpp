// expect a race
#include <assert.h>
#include <stdio.h>

#include "cilksan.h"

int a;

void add1(int n) {
    a++;
    printf("add1(%d)\n", n);
}

void foo(int x) {
    _Cilk_spawn add1(1);
    add1(x+2);
    _Cilk_sync;
    add1(3);
}

int main() {
    fprintf(stderr, "&a = %p.\n", &a);
    foo(0);
    // assert(__cilksan_error_count() == 2);

    return 0;
}
