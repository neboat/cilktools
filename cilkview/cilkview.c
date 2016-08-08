#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/* #define _POSIX_C_SOURCE 200112L */
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

/* #include <cilk/common.h> */
/* #include <internal/abi.h> */

#include <cilktool.h>

#include "context_stack.h"

#ifndef SERIAL_TOOL
#define SERIAL_TOOL 1
#endif

#if !SERIAL_TOOL
#include <cilk/reducer.h>
#include "context_stack_reducer.h"
#endif

#ifndef TRACE_CALLS
#define TRACE_CALLS 1
#endif

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */
#if SERIAL_TOOL
context_stack_t ctx_stack;
#else
CILK_C_DECLARE_REDUCER(context_stack_t) ctx_stack =
  CILK_C_INIT_REDUCER(context_stack_t,
		      reduce_context_stack,
		      identity_context_stack,
		      destroy_context_stack,
		      {NULL});
#endif

bool TOOL_INITIALIZED = false;

/*************************************************************************/
/**
 * Data structures and helper methods for time of user strands.
 */

static inline uint64_t elapsed_nsec(const struct timespec *stop,
			     const struct timespec *start) {
  return (uint64_t)(stop->tv_sec - start->tv_sec) * 1000000000ll
    + (stop->tv_nsec - start->tv_nsec);
}

static inline void gettime(struct timespec *timer) {
  // TB 2014-08-01: This is the "clock_gettime" variant I could get
  // working with -std=c11.  I want to use TIME_MONOTONIC instead, but
  // it does not appear to be supported on my system.
  /* timespec_get(timer, TIME_UTC); */
  clock_gettime(CLOCK_MONOTONIC, timer);
}

#if SERIAL_TOOL
// Ensure that this tool is run serially
static inline void ensure_serial_tool(void) {
  // assert(1 == __cilkrts_get_nworkers());
  fprintf(stderr, "Forcing CILK_NWORKERS=1.\n");
  char *e = getenv("CILK_NWORKERS");
  if (!e || 0!=strcmp(e, "1")) {
    // fprintf(err_io, "Setting CILK_NWORKERS to be 1\n");
    if( setenv("CILK_NWORKERS", "1", 1) ) {
      fprintf(stderr, "Error setting CILK_NWORKERS to be 1\n");
      exit(1);
    }
  }
}
#endif

void cilk_tool_init(void) {
#if TRACE_CALLS
  fprintf(stderr, "cilk_tool_init()\n");
#endif

#if SERIAL_TOOL
  ensure_serial_tool();

  context_stack_init(&ctx_stack, MAIN);

  ctx_stack.in_user_code = true;

  gettime(&(ctx_stack.start));
#else
  context_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);

  CILK_C_REGISTER_REDUCER(ctx_stack);

  REDUCER_VIEW(ctx_stack).in_user_code = true;

  gettime(&(REDUCER_VIEW(ctx_stack).start));
#endif

  TOOL_INITIALIZED = true;
}

void cilk_tool_destroy(void) {
#if TRACE_CALLS
  fprintf(stderr, "cilk_tool_destroy()\n");
#endif
  /* CILK_C_UNREGISTER_REDUCER(ctx_stack); */
  TOOL_INITIALIZED = false;
}

void cilk_tool_print(void) {

  assert(TOOL_INITIALIZED);
#if SERIAL_TOOL
  assert(NULL != ctx_stack.bot);

  uint64_t span = ctx_stack.bot->prefix_spn + ctx_stack.bot->contin_spn;
  uint64_t work = ctx_stack.running_wrk;
#else
  assert(MAIN == REDUCER_VIEW(ctx_stack).bot->func_type);
  assert(NULL != REDUCER_VIEW(ctx_stack).bot);

  uint64_t span = REDUCER_VIEW(ctx_stack).bot->prefix_spn + REDUCER_VIEW(ctx_stack).bot->contin_spn;
  uint64_t work = REDUCER_VIEW(ctx_stack).running_wrk;
#endif

  fprintf(stderr, "work = %fs, span = %fs, parallelism = %f\n",
	  work / (1000000000.0),
	  span / (1000000000.0),
	  work / (float)span);

}


/*************************************************************************/
/**
 * Hooks into runtime system.
 */

void cilk_enter_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  context_stack_t *stack;
#if TRACE_CALLS
  fprintf(stderr, "cilk_enter_begin(%p, %p, %p)\n", sf, this_fn, rip);
#endif
  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */

  if (!TOOL_INITIALIZED) {
    /* cilk_tool_init(); */
#if SERIAL_TOOL
    ensure_serial_tool();
    context_stack_init(&(ctx_stack), MAIN);

    stack = &(ctx_stack);
#else
    context_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);
  
    CILK_C_REGISTER_REDUCER(ctx_stack);

    stack = &(REDUCER_VIEW(ctx_stack));
#endif
    /* stack->in_user_code = true; */
    /* gettime(&(stack->start)); */

    TOOL_INITIALIZED = true;

  } else {
#if SERIAL_TOOL
    stack = &(ctx_stack);
#else
    stack = &(REDUCER_VIEW(ctx_stack));
#endif
    gettime(&(stack->stop));

    assert(NULL != stack->bot);

    if (stack->bot->func_type != HELPER) {
#if TRACE_CALLS
      if (MAIN == stack->bot->func_type) {
        printf("parent is MAIN\n");
      } else {
        printf("parent is SPAWN\n");
      }
#endif
      // TB 2014-12-18: This assert won't necessarily pass, if
      // shrink-wrapping has taken place.
      /* assert(stack->in_user_code); */
      
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

void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

#if TRACE_CALLS
  fprintf(stderr, "cilk_enter_helper_begin(%p, %p, %p)\n", sf, this_fn, rip);
#endif

  stack->in_user_code = false;

  assert(NULL != stack->bot);

  /* Push new frame onto the stack */
  context_stack_push(stack, HELPER);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

  /* fprintf(stderr, "cilk_enter_end(%p)\n", sf); */
  /* if (attributes & 0x4) { */
  /*   fprintf(stderr, "AFTER ENTER_SPAWN_HELPER\n"); */
  /* } else { */
  /*   fprintf(stderr, "AFTER ENTER_SPAWN\n"); */
  /* } */

  if (SPAWN == stack->bot->func_type) {
#if TRACE_CALLS
    fprintf(stderr, "cilk_enter_end(%p, %p) from SPAWN\n", sf, rsp);
#endif
    assert(!(stack->in_user_code));
  } else {
#if TRACE_CALLS
    fprintf(stderr, "cilk_enter_end(%p, %p) from HELPER\n", sf, rsp);
#endif
  }

  stack->in_user_code = true;
  gettime(&(stack->start));
}

void cilk_tool_c_function_enter(void *this_fn, void *rip) {
#if TRACE_CALLS
  fprintf(stderr, "c_function_enter(%p, %p)\n", this_fn, rip);
#endif
}

void cilk_tool_c_function_leave(void *rip) {
#if TRACE_CALLS
  fprintf(stderr, "c_function_leave(%p)\n", rip);
#endif
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack;
#if TRACE_CALLS
  fprintf(stderr, "cilk_spawn_prepare(%p)\n", sf);
#endif
  if (TOOL_INITIALIZED) {
#if SERIAL_TOOL
    stack = &(ctx_stack);
#else
    stack = &(REDUCER_VIEW(ctx_stack));
#endif

    gettime(&(stack->stop));

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    assert(stack->in_user_code);
    stack->in_user_code = false;
  }
}

void cilk_spawn_or_continue(int in_continuation)
{
  if (in_continuation) {
    // In the continuation
#if TRACE_CALLS
    fprintf(stderr, "cilk_spawn_or_continue() from continuation\n");
#endif

    context_stack_t *stack;
#if SERIAL_TOOL
    stack = &(ctx_stack);
#else
    stack = &(REDUCER_VIEW(ctx_stack));
#endif
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
    gettime(&(stack->start));
  } else {
    // In the spawn
#if TRACE_CALLS
    fprintf(stderr, "cilk_spawn_or_continue() from spawn\n");
#endif
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
#if TRACE_CALLS
  fprintf(stderr, "cilk_detach_begin(%p)\n", parent);
#endif  
  return;
}

void cilk_detach_end(void)
{
#if TRACE_CALLS
  fprintf(stderr, "cilk_detach_end()\n");
#endif
  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif
  gettime(&(stack->stop));

  if (SPAWN == stack->bot->func_type) {

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_begin(%p) from SPAWN\n", sf);
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
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

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
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_end(%p) from SPAWN\n", sf);
#endif
    gettime(&(stack->start));
  } else {
#if TRACE_CALLS
    fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf);
#endif
  }
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

  context_stack_frame_t *old_bottom;
  
  gettime(&(stack->stop));

  assert(stack->in_user_code);
  stack->in_user_code = false;

  if (SPAWN == stack->bot->func_type) {
#if TRACE_CALLS
    fprintf(stderr, "cilk_leave_begin(%p) from SPAWN\n", sf);
#endif
    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;
    assert(NULL != stack->bot->parent);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;

    /* Pop the stack */
    old_bottom = context_stack_pop(stack);
    stack->bot->contin_spn += old_bottom->prefix_spn;
    
  } else {
#if TRACE_CALLS
    fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf);
#endif

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
  context_stack_t *stack;
#if SERIAL_TOOL
  stack = &(ctx_stack);
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

  /* struct context_stack_el *old_bottom = ctx_stack; */

  /* assert(NULL != ctx_stack); */
#if TRACE_CALLS
  switch(stack->bot->func_type) {
  case HELPER:
    fprintf(stderr, "cilk_leave_end() into HELPER\n");
    break;
  case SPAWN:
    fprintf(stderr, "cilk_leave_end() into SPAWN\n");
    break;
  case MAIN:
    fprintf(stderr, "cilk_leave_end() into MAIN\n");
    break;
  }
#endif

  /* if (HELPER != stack->bot->func_type) { */
  /*   assert(!(stack->in_user_code)); */
  /*   stack->in_user_code = true; */
  /*   gettime(&(stack->start)); */
  /* } */
  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  gettime(&(stack->start));
}

