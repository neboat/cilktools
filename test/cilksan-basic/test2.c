// expect a race
#include <assert.h>
#include <stdio.h>

#include "cilksan.h"

int a;

void add1(int n) {
    a++;
    if (0) printf("add1(%d)\n", n);
}

void foo() {
    add1(0);
    _Cilk_spawn add1(1);
    add1(2);
    _Cilk_spawn add1(3);
    add1(4);
    _Cilk_sync;
    add1(5);
}

int main() {
    printf("  &a=%p\n", &a);
    foo();

    assert(__cilksan_error_count() == 2);

    return 0;
}
