// expect a race on heap-allocated data
#include "cilksan.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

void foo(char *x) {
    x[0] = 1;
}

int main() {
    {
	char *x = malloc(16);
	printf("%p\n", x);
	_Cilk_spawn foo(x);
	x[0] = 0;
	_Cilk_sync;
	free(x);
    }
    assert(__cilksan_error_count() == 1);
    
    return 0;
}
