// expect no race
#include "cilksan.h"
#include <assert.h>
#include <stdio.h>

int touchn(int n) {
    int x[n];
    printf("&x[0]=%p\n", &x[0]);
    for (int i=0; i<n; i++) x[i] = n;
    int r = 0;
    for (int i=0; i<n-1; i++) {
	r += x[i]*x[i+1];
    }
    return r;
}

int foo() {
    int a = _Cilk_spawn touchn(2);
    int b = touchn(2);
    _Cilk_sync;
    return a + b;
}

int main() {
    int x = foo();
    assert(x!=0);
    assert(__cilksan_error_count() == 0);
    
    return 0;
}
