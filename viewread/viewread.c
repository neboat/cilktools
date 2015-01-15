#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <sys/types.h>
#include <execinfo.h>

#include <cilktool.h>

#include "viewread_stack.h"
#include "viewread_shadowmem.h"
#include "util.h"

#ifndef TRACE_CALLS
#define TRACE_CALLS 0
#endif

/*************************************************************************/
/**
 * Data structures.
 */
viewread_stack_t ctx_stack;

shadowmem_t *memory;

bool TOOL_INITIALIZED = false;

/*************************************************************************/
/**
 * Helper methods.
 */
void initialize_tool(viewread_stack_t *stack) {
  // This is a serial tool
  ensure_serial_tool();
  viewread_stack_init(stack, MAIN);
  memory = shadowmem_create();
  TOOL_INITIALIZED = true;
}

/*************************************************************************/
/**
 * Main tool methods.
 */

void cilk_tool_init(void) {
#if TRACE_CALLS
  fprintf(stderr, "cilk_tool_init()\n");
#endif

  initialize_tool(&ctx_stack);

  ctx_stack.in_user_code = true;
}

void cilk_tool_destroy(void) {
#if TRACE_CALLS
  fprintf(stderr, "cilk_tool_destroy()\n");
#endif

  TOOL_INITIALIZED = false;
}

void cilk_tool_print(void) {
  /* viewread_stack_t *stack; */
  /* stack = &ctx_stack; */
  return;
}

void cilk_enter_begin(__cilkrts_stack_frame *sf, void* rip)
{
#if TRACE_CALLS
  fprintf(stderr, "cilk_enter_begin(%p, %p)\n", sf, rip);
#endif
  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */
  viewread_stack_t *stack;

  if (!TOOL_INITIALIZED) {

    initialize_tool(&ctx_stack);

    stack = &ctx_stack;

    stack->in_user_code = false;

  } else {
    stack = &ctx_stack;

    assert(NULL != stack->bot);

    if (stack->bot->func_type != HELPER) {

      /* assert(stack->in_user_code); */

      stack->in_user_code = false;
    } else {
      /* assert(!(stack->in_user_code)); */
      stack->in_user_code = false;
    }
  }

  // Push a new frame onto the stack.  This autmoatically sets the
  // view_id of the new frame to the current view_id of the parent.
  viewread_stack_push(stack, CILK);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip); */
}

void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *rip)
{
  viewread_stack_t *stack = &ctx_stack;

#if TRACE_CALLS
  fprintf(stderr, "cilk_enter_helper_begin(%p, %p) from CILK\n", sf, rip);
#endif

  assert(NULL != stack->bot);

  // Push new frame onto the stack
  viewread_stack_push(stack, HELPER);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip); */
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  viewread_stack_t *stack = &ctx_stack;

  if (CILK == stack->bot->func_type) {
#if TRACE_CALLS
    fprintf(stderr, "cilk_enter_end(%p, %p) from CILK\n", sf, rsp);
#endif
    assert(!(stack->in_user_code));
  } else {
#if TRACE_CALLS
  fprintf(stderr, "cilk_enter_end(%p, %p) from HELPER\n", sf, rsp);
#endif
  }

  stack->in_user_code = true;
}

void cilk_tool_c_function_enter(void *rip) {
/* #if TRACE_CALLS */
/*   fprintf(stderr, "c_function_enter(%p)\n", rip); */
/* #endif */
}

void cilk_tool_c_function_leave(void *rip) {
/* #if TRACE_CALLS */
/*   fprintf(stderr, "c_function_leave(%p)\n", rip); */
/* #endif */
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  viewread_stack_t *stack;

#if TRACE_CALLS
  fprintf(stderr, "cilk_spawn_prepare(%p)\n", sf);
#endif
  if (!TOOL_INITIALIZED) {
    initialize_tool(&ctx_stack);
  } else {
    assert(ctx_stack.in_user_code);
  }
  stack = &ctx_stack;
  stack->in_user_code = false;
}

void cilk_spawn_or_continue(int in_continuation)
{
  viewread_stack_t *stack;
  stack = &ctx_stack;

  if (in_continuation) {
    // In the continuation
#if TRACE_CALLS
    fprintf(stderr, "cilk_spawn_or_continue(%d) from continuation\n", in_continuation);
#endif
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
  } else {
    // In the spawned child
#if TRACE_CALLS
    fprintf(stderr, "cilk_spawn_or_continue(%d) from spawn\n", in_continuation);
#endif
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  viewread_stack_t *stack;
  stack = &ctx_stack;

  stack->in_user_code = false;
#if TRACE_CALLS
  fprintf(stderr, "cilk_detach_end(%p)\n", parent);
#endif
  return;
}

void cilk_detach_end(void)
{
  viewread_stack_t *stack;
  stack = &ctx_stack;

  stack->in_user_code = true;
#if TRACE_CALLS
  fprintf(stderr, "cilk_detach_end()\n");
#endif
  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  viewread_stack_t *stack = &ctx_stack;

  if (CILK == stack->bot->func_type) {
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_begin(%p) from CILK\n", sf);
#endif
    assert(stack->in_user_code);
    stack->in_user_code = false;
  } else {
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf);
#endif
  }
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  viewread_stack_t *stack = &ctx_stack;

  if (CILK == stack->bot->func_type) {
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_end(%p) from CILK\n", sf);
#endif

    // Reset the number of local spawns
    stack->bot->local_spawns = 0;

    /* fprintf(stderr, "P bag %p, SP bag %p\n", */
    /*         stack->bot->p_bag, stack->bot->sp_bag); */

    if (NULL == stack->bot->p_bag && NULL != stack->bot->sp_bag) {
      stack->bot->p_bag = DisjointSet_find_set(stack->bot->sp_bag);
      stack->bot->p_bag->type = P;
      stack->bot->sp_bag = NULL;
    } else if (NULL != stack->bot->sp_bag) {
      DisjointSet_combine(stack->bot->p_bag, stack->bot->sp_bag);
      stack->bot->p_bag->type = P;
      stack->bot->sp_bag = NULL;
    }
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
  } else {
    assert(0 == stack->bot->local_spawns);
    assert(NULL == stack->bot->sp_bag);
    assert(NULL == stack->bot->p_bag);
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf);
#endif
  }
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  viewread_stack_t *stack = &ctx_stack;

  viewread_stack_frame_t *old_bottom;

  assert(stack->in_user_code);
  stack->in_user_code = false;

  // Pop the stack
  old_bottom = viewread_stack_pop(stack);

  // SP bag should be empty when old_bottom returns
  assert(NULL == old_bottom->sp_bag);

  /* fprintf(stderr, "returning P bag %p, dest P bag %p\n", */
  /*         old_bottom->p_bag, stack->bot->p_bag); */
  if (NULL == stack->bot->p_bag && NULL != old_bottom->p_bag) {
    stack->bot->p_bag = DisjointSet_find_set(old_bottom->p_bag);
    stack->bot->p_bag->type = P;
  } else if (NULL != old_bottom->p_bag) {
    DisjointSet_combine(stack->bot->p_bag, old_bottom->p_bag);
    stack->bot->p_bag->type = P;
  }
  assert(NULL == stack->bot->p_bag || P == stack->bot->p_bag->type);

  switch(old_bottom->func_type) {
    case CILK:  // Returning from called function
#if TRACE_CALLS
      fprintf(stderr, "cilk_leave_begin(%p) from CILK\n", sf);
#endif

      /* fprintf(stderr, "combining SS bags %p and %p\n", */
      /*         stack->bot->ss_bag, old_bottom->ss_bag); */
      if (stack->bot->local_spawns == 0) {
        // Combine SS bags
        DisjointSet_combine(stack->bot->ss_bag, old_bottom->ss_bag);
        assert(SS == stack->bot->ss_bag->type);
      } else {
        // Combine returning SS bag into its parent's SP bag
        if (NULL == stack->bot->sp_bag) {
          stack->bot->sp_bag = DisjointSet_find_set(old_bottom->ss_bag);
          stack->bot->sp_bag->type = SP;
        } else {
          DisjointSet_combine(stack->bot->sp_bag, old_bottom->ss_bag);
          stack->bot->sp_bag->type = SP;
        }          
      }
      break;
    case HELPER:  // Returning from spawned function
#if TRACE_CALLS
      fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf);
#endif

      /* fprintf(stderr, "returning SS bag %p, dest P bag %p\n", */
      /*         old_bottom->ss_bag, stack->bot->p_bag); */

      assert(0 != stack->bot->local_spawns);

      if (NULL == stack->bot->p_bag) {
        stack->bot->sp_bag = DisjointSet_find_set(old_bottom->ss_bag);
        stack->bot->sp_bag->type = P;
      } else {
        DisjointSet_combine(stack->bot->p_bag, old_bottom->ss_bag);
        stack->bot->sp_bag->type = P;
      }
      break;
    case MAIN:
      fprintf(stderr, "[ALERT] cilk_leave_begin(%p) from MAIN.\n", sf);
      break;
  }

  free(old_bottom);
}

void cilk_leave_end(void)
{
  viewread_stack_t *stack = &ctx_stack;

#if TRACE_CALLS
  switch(stack->bot->func_type) {
    case HELPER:
      fprintf(stderr, "cilk_leave_end() from HELPER\n");
      break;
    case CILK:
      fprintf(stderr, "cilk_leave_end() from CILK\n");
      break;
    case MAIN:
      fprintf(stderr, "cilk_leave_end() from MAIN\n");
      break;
  }
#endif
  assert(!(stack->in_user_code));
  stack->in_user_code = true;
}

void cilk_begin_reduce_strand(void) {
  return;
}

void cilk_end_reduce_strand(void) {
  return;
}

void cilk_begin_update_strand(void) {
  return;
}

void cilk_end_update_strand(void) {
  return;
}

void cilk_set_reducer(void *reducer, void *rip, const char *function, int line) {
  if (!TOOL_INITIALIZED) {
    initialize_tool(&ctx_stack);
    ctx_stack.in_user_code = true;
  }

  viewread_stack_t *stack = &ctx_stack;
  update_shadowmem(&memory, (reducer_t)reducer, (uintptr_t)rip,
                   stack->bot->ancestor_spawns + stack->bot->local_spawns,
                   stack->bot->ss_bag);
}

void cilk_read_reducer(void *reducer, void *rip, const char *function, int line) {
  viewread_stack_t *stack = &ctx_stack;
  reader_t *last_reader = get_reader((reducer_t)reducer, &memory);
  DisjointSet_t *rep = DisjointSet_find_set(last_reader->node);
  if (P == rep->type ||
      last_reader->spawns != stack->bot->ancestor_spawns + stack->bot->local_spawns) {
    fprintf(stderr, "View-read race detected between %p and %p (%s:%d)\n",
            (void*)(last_reader->reader), rip, function, line);
  } else {
    if (P != rep->type) {
      /* fprintf(stderr, "Safe read; rep %p\n", rep); */
      assert(stack->bot->ancestor_spawns + stack->bot->local_spawns == last_reader->spawns);
    } else {
      /* fprintf(stderr, "spawn counts are both %lx\n", */
      /*         stack->bot->ancestor_spawns + stack->bot->local_spawns); */
    }
  }
  update_shadowmem(&memory, (reducer_t)reducer, (uintptr_t)rip,
                   stack->bot->ancestor_spawns + stack->bot->local_spawns,
                   stack->bot->ss_bag);
}
