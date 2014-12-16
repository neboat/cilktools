// Expect no errors
#include <assert.h>
#include <stdio.h>

#include "cilksan.h"

void add1(int n) {
    int a = 0;
    a += n;
    printf("add1(%d)\n", a);
}

void foo() {
    _Cilk_spawn add1(1);
    add1(3);
}

int main() {
    foo();
    assert(__cilksan_error_count() == 0);

    return 0;
}
