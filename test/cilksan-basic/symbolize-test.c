extern void foo();

#include <assert.h>
#include "cilksan.h"

int main(void) {
    _Cilk_spawn foo();
    foo();
    assert(__cilksan_error_count() == 2);

    return 0;
}
