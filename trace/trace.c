#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "cilktool.h"

/*************************************************************************/
/**
 * Cilk instrumentation hooks.
 */

void cilk_enter_begin(uint32_t prop, __cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  fprintf(stderr, "cilk_enter_begin(prop = %d, sf = %p, this_fn = %p, rip = %p)\n", prop, sf, this_fn, rip);
}

void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  fprintf(stderr, "cilk_enter_helper_begin(sf = %p, this_fn = %p, rip = %p)\n", sf, this_fn, rip);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  fprintf(stderr, "cilk_enter_end(sf = %p, rsp = %p)\n", sf, rsp);
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  fprintf(stderr, "cilk_spawn_prepare(%p)\n", sf);
}

void cilk_spawn_or_continue(int in_continuation)
{
  if (in_continuation) {
    // In the continuation
    fprintf(stderr, "cilk_spawn_or_continue(CONTINUATION)\n");
  } else {
    // In the spawn
    fprintf(stderr, "cilk_spawn_or_continue(SPAWN)\n");
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  fprintf(stderr, "cilk_detach_begin(parent = %p)\n", parent);
}

void cilk_detach_end(void)
{
  fprintf(stderr, "cilk_detach_end()\n");
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  fprintf(stderr, "cilk_sync_begin(sf = %p)\n", sf);
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  fprintf(stderr, "cilk_sync_end(sf = %p)\n", sf);
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  fprintf(stderr, "cilk_leave_begin(sf = %p)\n", sf);
}

void cilk_leave_end(void)
{
  fprintf(stderr, "cilk_leave_end()\n");
}

