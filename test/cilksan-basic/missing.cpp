#include <assert.h>
#include "cilksan.h"

// expect a race
int a;
void bar() {
    a++;
}
void zot() {
    a++;
}
int main() {
    _Cilk_spawn bar();
    zot();
    assert(__cilksan_error_count() == 1);

    return 0;
}
