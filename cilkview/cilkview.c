#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define _POSIX_C_SOURCE 200112L
#include <stdbool.h>
#include <inttypes.h>

/* #include <cilk/common.h> */
/* #include <internal/abi.h> */

#include <cilktool.h>

#include <cilk/reducer.h>

#include "context_stack.h"
#include "context_stack_reducer.h"

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */

CILK_C_DECLARE_REDUCER(context_stack_t) ctx_stack =
  CILK_C_INIT_REDUCER(context_stack_t,
		      reduce_context_stack,
		      identity_context_stack,
		      destroy_context_stack,
		      NULL);

bool TOOL_INITIALIZED = false;

/*************************************************************************/
/**
 * Data structures and helper methods for time of user strands.
 */

inline uint64_t elapsed_nsec(const struct timespec *stop,
			     const struct timespec *start) {
  return (uint64_t)(stop->tv_sec - start->tv_sec) * 1000000000ll
    + (stop->tv_nsec - start->tv_nsec);
}

inline void gettime(struct timespec *timer) {
  // TB 2014-08-01: This is the "clock_gettime" variant I could get
  // working with -std=c11.  I want to use TIME_MONOTONIC instead, but
  // it does not appear to be supported on my system.
  timespec_get(timer, TIME_UTC);
}

void cilk_tool_init(void) {
  
  context_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);
  
  CILK_C_REGISTER_REDUCER(ctx_stack);

  REDUCER_VIEW(ctx_stack).in_user_code = true;
  gettime(&(REDUCER_VIEW(ctx_stack).start));

  TOOL_INITIALIZED = true;
}

void cilk_tool_destroy(void) {
  CILK_C_UNREGISTER_REDUCER(ctx_stack);
  TOOL_INITIALIZED = false;
}

void cilk_tool_print(void) {

  assert(TOOL_INITIALIZED);
  /* assert(MAIN == REDUCER_VIEW(ctx_stack).bot->func_type); */
  assert(NULL != REDUCER_VIEW(ctx_stack).bot);

  uint64_t span = REDUCER_VIEW(ctx_stack).bot->prefix_spn + REDUCER_VIEW(ctx_stack).bot->contin_spn;
  uint64_t work = REDUCER_VIEW(ctx_stack).running_wrk;

  fprintf(stderr, "work = %fs, span = %fs, parallelism = %f\n",
	  work / (1000000000.0),
	  span / (1000000000.0),
	  work / (float)span);

}


/*************************************************************************/
/**
 * Hooks into runtime system.
 */

void cilk_enter_begin(__cilkrts_stack_frame *sf, void *rip)
{
  context_stack_t *stack;
  /* fprintf(stderr, "cilk_enter_begin(%p)\n", sf); */
  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */

  if (!TOOL_INITIALIZED) {
    /* cilk_tool_init(); */
    context_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);
  
    CILK_C_REGISTER_REDUCER(ctx_stack);

    stack = &(REDUCER_VIEW(ctx_stack));

    /* stack->in_user_code = true; */
    /* gettime(&(stack->start)); */

    TOOL_INITIALIZED = true;

  } else {
    stack = &(REDUCER_VIEW(ctx_stack));
    gettime(&(stack->stop));

    assert(NULL != stack->bot);

    if (stack->bot->func_type != HELPER) {
      assert(stack->in_user_code);
      
      uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
      stack->running_wrk += strand_time;
      stack->bot->contin_spn += strand_time;
      
      stack->in_user_code = false;
    } else {
      assert(!(stack->in_user_code));
    }
  }

  /* Push new frame onto the stack */
  context_stack_push(stack, SPAWN);
}

void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *rip)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
  /* fprintf(stderr, "cilk_enter_helper_begin(%p)\n", sf); */

  assert(NULL != stack->bot);

  /* Push new frame onto the stack */
  context_stack_push(stack, HELPER);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
  /* fprintf(stderr, "cilk_enter_end(%p)\n", sf); */

  /* if (attributes & 0x4) { */
  /*   fprintf(stderr, "AFTER ENTER_SPAWN_HELPER\n"); */
  /* } else { */
  /*   fprintf(stderr, "AFTER ENTER_SPAWN\n"); */
  /* } */

  if (SPAWN == stack->bot->func_type) {
    // fprintf(stderr, "cilk_enter_end() from SPAWN\n");
    assert(!(stack->in_user_code));
    stack->in_user_code = true;

    gettime(&(stack->start));
  } else {
    // fprintf(stderr, "cilk_enter_end() from HELPER\n");
  }
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
  gettime(&(stack->stop));

  // fprintf(stderr, "cilk_spawn_prepare()\n");

  uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
  stack->running_wrk += strand_time;
  stack->bot->contin_spn += strand_time;

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

void cilk_spawn_or_continue(int in_continuation)
{
  if (in_continuation) {
    // In the continuation
    // fprintf(stderr, "cilk_spawn_or_continue() from continuation\n");

    context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
    gettime(&(stack->start));
  } else {
    // In the spawn
    // fprintf(stderr, "cilk_spawn_or_continue() from spawn\n");
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  return;
}

void cilk_detach_end(void)
{
  // fprintf(stderr, "cilk_detach_end()\n");
  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
  gettime(&(stack->stop));

  if (SPAWN == stack->bot->func_type) {

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    // fprintf(stderr, "cilk_sync_begin() from SPAWN\n");
    assert(stack->in_user_code);
    stack->in_user_code = false;
  } else {
    // fprintf(stderr, "cilk_sync_end() from HELPER\n");
  }
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));

  if (stack->bot->lchild_spn > stack->bot->contin_spn) {
    stack->bot->prefix_spn += stack->bot->lchild_spn;
  } else {
    stack->bot->prefix_spn += stack->bot->contin_spn;
  }
  stack->bot->lchild_spn = 0;
  stack->bot->contin_spn = 0;
  
  if (SPAWN == stack->bot->func_type) {
    assert(!(stack->in_user_code));
    stack->in_user_code = true;

    // fprintf(stderr, "cilk_sync_end() from SPAWN\n");
    gettime(&(stack->start));
  } else {
    // fprintf(stderr, "cilk_sync_end() from HELPER\n");
  }
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
  context_stack_frame_t *old_bottom;
  
  gettime(&(stack->stop));

  if (SPAWN == stack->bot->func_type) {
    /* fprintf(stderr, "cilk_leave_begin(%p) from SPAWN\n", sf); */

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    assert(stack->in_user_code);
    stack->in_user_code = false;

    assert(NULL != stack->bot->parent);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;

    /* Pop the stack */
    old_bottom = context_stack_pop(stack);
    stack->bot->contin_spn += old_bottom->prefix_spn;
    
  } else {
    /* fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf); */

    assert(HELPER != stack->bot->parent->func_type);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;

    /* Pop the stack */
    old_bottom = context_stack_pop(stack);
    if (stack->bot->contin_spn + old_bottom->prefix_spn > stack->bot->lchild_spn) {
      // fprintf(stderr, "updating longest child\n");
      stack->bot->prefix_spn += stack->bot->contin_spn;
      stack->bot->lchild_spn = old_bottom->prefix_spn;
      stack->bot->contin_spn = 0;
    }
    
  }

  free(old_bottom);
}

void cilk_leave_end(void)
{
  context_stack_t *stack = &(REDUCER_VIEW(ctx_stack));

  /* struct context_stack_el *old_bottom = REDUCER_VIEW(ctx_stack); */

  /* assert(NULL != REDUCER_VIEW(ctx_stack)); */
  switch(stack->bot->func_type) {
  case HELPER:
    /* fprintf(stderr, "cilk_leave_end() into HELPER\n"); */
    break;
  case SPAWN:
    /* fprintf(stderr, "cilk_leave_end() into SPAWN\n"); */
    break;
  case MAIN:
    /* fprintf(stderr, "cilk_leave_end() into MAIN\n"); */
    break;
  }

  if (HELPER != stack->bot->func_type) {
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
    gettime(&(stack->start));
  }
}

