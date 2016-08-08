#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define _POSIX_C_SOURCE = 200112L
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#include <locale.h>
#include <sys/ioctl.h>
#include <err.h>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilktool.h>

/* #include "perf_util.h" */
#include <perfmon/pfmlib_perf_event.h>

#include <cilk/reducer.h>

#include "cilkview_perf_stack.h"

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */

cilkview_perf_stack_t ctx_stack;

bool TOOL_INITIALIZED = false;
const char *perf_event = "instructions";
struct perf_event_attr attr;

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
  timespec_get(timer, TIME_UTC);
}

static inline int perf_setup(void) {
  int ret;
  ret = pfm_initialize();
  if (PFM_SUCCESS != ret) {
    errx(1, "cannot initialize library: %s", pfm_strerror(ret));
  }

  memset(&attr, 0, sizeof(attr));

  ret = pfm_get_perf_event_encoding(perf_event, PFM_PLM0 | PFM_PLM3, &attr, NULL, NULL);
  if (PFM_SUCCESS != ret) {
    errx(1, "cannot find encoding: %s", pfm_strerror(ret));
  }

  attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
  attr.disabled = 1;

  int fd;
  fd = perf_event_open(&attr, getpid(), -1, -1, 0);
  if (fd < 0) {
    err(1, "cannot create event");
  }

  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

  return fd;
}

static inline void perf_shutdown(int fd) {
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

  pfm_terminate();
}

static inline void perf_get_count(int fd, uint64_t* values /* uint64_t[3] */) {
  int ret;
  ret = read(fd, values, 3 * sizeof(uint64_t));
  if (ret != 3 * sizeof(uint64_t)) {
    err(1, "cannot read results: %s", strerror(errno));
  }
}

static inline uint64_t
elapsed_perf_count(const uint64_t *stop_values /* uint64_t[3] */,
		   const uint64_t *start_values /* uint64_t[3] */) {
  return (uint64_t)((double)((stop_values[0] - start_values[0])
			     * (stop_values[1] - start_values[1])
			     / (stop_values[2] - start_values[2])));
}


static inline void start_strand(cilkview_perf_stack_t *stack) {
  perf_get_count(stack->fd, stack->start_values);
  gettime(&(stack->start));
}

static inline void stop_strand(cilkview_perf_stack_t *stack) {
  gettime(&(stack->stop));
  perf_get_count(stack->fd, stack->stop_values);
}

/*************************************************************************/

void cilk_tool_init(void) {

  TOOL_INITIALIZED = true;

  assert(1 == __cilkrts_get_nworkers());

  setlocale(LC_ALL, "");

  cilkview_perf_stack_init(&ctx_stack, MAIN);
  ctx_stack.in_user_code = true;

  ctx_stack.fd = perf_setup();
  
  start_strand(&ctx_stack);
}

void cilk_tool_destroy(void) {

  perf_shutdown(ctx_stack.fd);

  TOOL_INITIALIZED = false;
}

void cilk_tool_print(void) {

  assert(TOOL_INITIALIZED);

  assert(NULL != ctx_stack.bot);

  ctx_stack.bot->prefix_spn += ctx_stack.bot->contin_spn;
  ctx_stack.bot->prefix_spn_data += ctx_stack.bot->contin_spn_data;

  uint64_t span = ctx_stack.bot->prefix_spn;
  uint64_t work = ctx_stack.running_wrk;

  fprintf(stderr, "work = %fs, span = %fs, parallelism = %f\n",
	  work / (1000000000.0),
	  span / (1000000000.0),
	  (double)work / (double)span);

  uint64_t span_data = ctx_stack.bot->prefix_spn_data;
  uint64_t work_data = ctx_stack.running_wrk_data;
  fprintf(stderr, "work_data = %lu, span_data = %lu, parallelism = %f\n",
	  work_data,
	  span_data,
	  (double)work_data / (double)span_data);

}


/*************************************************************************/
/**
 * Hooks into runtime system.
 */

void cilk_enter_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  cilkview_perf_stack_t *stack;
  /* fprintf(stderr, "cilk_enter_begin(%p, %p, %p)\n", sf, this_fn, rip); */
  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */

  if (!TOOL_INITIALIZED) {
    /* cilk_tool_init(); */

    stack = &ctx_stack;

    cilkview_perf_stack_init(stack, MAIN);

    stack->fd = perf_setup();
    
    TOOL_INITIALIZED = true;

  } else {
    /* stack = &(REDUCER_VIEW(ctx_stack)); */
    stack = &ctx_stack;

    stop_strand(stack);

    assert(NULL != stack->bot);

    if (stack->bot->func_type != HELPER) {
      assert(stack->in_user_code);
      
      uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
      stack->running_wrk += strand_time;
      stack->bot->contin_spn += strand_time;
      
      /* Get performance data from perf counter. */
      strand_time = elapsed_perf_count(stack->stop_values, stack->start_values);
      stack->running_wrk_data += strand_time;
      stack->bot->contin_spn_data += strand_time;

      stack->in_user_code = false;
    } else {
      assert(!(stack->in_user_code));
    }
  }

  /* Push new frame onto the stack */
  cilkview_perf_stack_push(stack, SPAWN);
}

void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;
  /* fprintf(stderr, "cilk_enter_helper_begin(%p)\n", sf); */

  assert(NULL != stack->bot);

  /* Push new frame onto the stack */
  cilkview_perf_stack_push(stack, HELPER);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;
  /* fprintf(stderr, "cilk_enter_end(%p)\n", sf); */

  if (SPAWN == stack->bot->func_type) {
    // fprintf(stderr, "cilk_enter_end() from SPAWN\n");
    assert(!(stack->in_user_code));
    stack->in_user_code = true;

    /* gettime(&(stack->start)); */
    start_strand(stack);

  } else {
    // fprintf(stderr, "cilk_enter_end() from HELPER\n");
  }
}

void cilk_tool_c_function_enter(void *this_fn, void *rip) {
  /* fprintf(stderr, "C function enter %p.\n", rip); */
}

void cilk_tool_c_function_leave(void *rip) {
  /* fprintf(stderr, "C function leave %p.\n", rip); */
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;
  /* gettime(&(stack->stop)); */
  stop_strand(stack);

  // fprintf(stderr, "cilk_spawn_prepare()\n");

  uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
  stack->running_wrk += strand_time;
  stack->bot->contin_spn += strand_time;

  /* Get performance data from perf counter. */
  strand_time = elapsed_perf_count(stack->stop_values, stack->start_values);
  stack->running_wrk_data += strand_time;
  stack->bot->contin_spn_data += strand_time;

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

void cilk_spawn_or_continue(int in_continuation)
{
  if (in_continuation) {
    // In the continuation
    // fprintf(stderr, "cilk_spawn_or_continue() from continuation\n");

    /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
    cilkview_perf_stack_t *stack = &ctx_stack;
    assert(!(stack->in_user_code));
    stack->in_user_code = true;

    start_strand(stack);
    /* gettime(&(stack->start)); */
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
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;

  /* gettime(&(stack->stop)); */
  stop_strand(stack);

  if (SPAWN == stack->bot->func_type) {

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    /* Get performance data from perf counter. */
    strand_time = elapsed_perf_count(stack->stop_values, stack->start_values);
    stack->running_wrk_data += strand_time;
    stack->bot->contin_spn_data += strand_time;

    // fprintf(stderr, "cilk_sync_begin() from SPAWN\n");
    assert(stack->in_user_code);
    stack->in_user_code = false;
  } else {
    // fprintf(stderr, "cilk_sync_end() from HELPER\n");
  }
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;

  /* if (stack->bot->lchild_spn > stack->bot->contin_spn) { */
  if (stack->bot->lchild_spn_data > stack->bot->contin_spn_data) {
    stack->bot->prefix_spn += stack->bot->lchild_spn;
    stack->bot->prefix_spn_data += stack->bot->lchild_spn_data;
  } else {
    stack->bot->prefix_spn += stack->bot->contin_spn;
    stack->bot->prefix_spn_data += stack->bot->contin_spn_data;
  }
  stack->bot->lchild_spn = 0;
  stack->bot->contin_spn = 0;
  stack->bot->lchild_spn_data = 0;
  stack->bot->contin_spn_data = 0;
  
  if (SPAWN == stack->bot->func_type) {
    assert(!(stack->in_user_code));
    stack->in_user_code = true;

    // fprintf(stderr, "cilk_sync_end() from SPAWN\n");

    start_strand(stack);
    /* gettime(&(stack->start)); */
  } else {
    // fprintf(stderr, "cilk_sync_end() from HELPER\n");
  }
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;
  cilkview_perf_stack_frame_t *old_bottom;

  stop_strand(stack);
  /* gettime(&(stack->stop)); */

  if (SPAWN == stack->bot->func_type) {
    /* fprintf(stderr, "cilk_leave_begin(%p) from SPAWN\n", sf); */

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    /* Get performance data from perf counter. */
    strand_time = elapsed_perf_count(stack->start_values, stack->stop_values);
    stack->running_wrk_data += strand_time;
    stack->bot->contin_spn_data += strand_time;

    assert(stack->in_user_code);
    stack->in_user_code = false;

    assert(NULL != stack->bot->parent);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;
    stack->bot->prefix_spn_data += stack->bot->contin_spn_data;

    /* Pop the stack */
    old_bottom = cilkview_perf_stack_pop(stack);
    stack->bot->contin_spn += old_bottom->prefix_spn;
    stack->bot->contin_spn_data += old_bottom->prefix_spn_data;
    
  } else {
    /* fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf); */

    assert(HELPER != stack->bot->parent->func_type);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;
    stack->bot->prefix_spn_data += stack->bot->contin_spn_data;

    /* Pop the stack */
    old_bottom = cilkview_perf_stack_pop(stack);
    /* if (stack->bot->contin_spn + old_bottom->prefix_spn > stack->bot->lchild_spn) { */
    if (stack->bot->contin_spn_data + old_bottom->prefix_spn_data
	> stack->bot->lchild_spn_data) {

      // fprintf(stderr, "updating longest child\n");
      stack->bot->prefix_spn += stack->bot->contin_spn;
      stack->bot->prefix_spn_data += stack->bot->contin_spn_data;

      stack->bot->lchild_spn = old_bottom->prefix_spn;
      stack->bot->lchild_spn_data = old_bottom->prefix_spn_data;

      stack->bot->contin_spn = 0;
      stack->bot->contin_spn_data = 0;
    }
    
  }

  free(old_bottom);
}

void cilk_leave_end(void)
{
  /* cilkview_perf_stack_t *stack = &(REDUCER_VIEW(ctx_stack)); */
  cilkview_perf_stack_t *stack = &ctx_stack;

  /* struct cilkview_perf_stack_el *old_bottom = REDUCER_VIEW(ctx_stack); */

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
    start_strand(stack);
    /* gettime(&(stack->start)); */
  }
}

