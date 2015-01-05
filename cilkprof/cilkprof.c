#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* #define _POSIX_C_SOURCE = 200112L */
/* #define _POSIX_C_SOURCE = 200809L */
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <sys/types.h>

#include <cilktool.h>
#include "cilkprof_stack.h"
#include "util.h"

#ifndef SERIAL_TOOL
#define SERIAL_TOOL 1
#endif

#ifndef TRACE_CALLS
#define TRACE_CALLS 0
#endif

#if SERIAL_TOOL
#define GET_STACK(ex) ex
#else
#define GET_STACK(ex) REDUCER_VIEW(ex)
#include "cilkprof_stack_reducer.h"
#endif

// TB: Adjusted so I can terminate WHEN_TRACE_CALLS() with semicolons.
// Emacs gets confused about tabbing otherwise.
#if TRACE_CALLS
#define WHEN_TRACE_CALLS(ex) do { ex } while (0)
#else
#define WHEN_TRACE_CALLS(ex) do {} while (0)
#endif

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */


#if SERIAL_TOOL
static cilkprof_stack_t ctx_stack;
#else
static CILK_C_DECLARE_REDUCER(cilkprof_stack_t) ctx_stack =
  CILK_C_INIT_REDUCER(cilkprof_stack_t,
		      reduce_cilkprof_stack,
		      identity_cilkprof_stack,
		      destroy_cilkprof_stack,
		      {NULL});
#endif

static bool TOOL_INITIALIZED = false;

/*************************************************************************/
/**
 * Helper methods.
 */

static inline void initialize_tool(cilkprof_stack_t *stack) {
#if SERIAL_TOOL
  // This is a serial tool
  ensure_serial_tool();
#else
  // probably need to register the reducer here as well.
  CILK_C_REGISTER_REDUCER(ctx_stack);
#endif
  cilkprof_stack_init(stack, MAIN);
  TOOL_INITIALIZED = true;
}

static inline void measure_and_add_strand_length(cilkprof_stack_t *stack) {
  // Measure strand length
  uint64_t strand_len = measure_strand_length(&(stack->strand_ruler));

  assert(NULL != stack->bot);

  // Accumulate strand length
  stack->bot->running_wrk += strand_len;
  stack->bot->contin_spn += strand_len;
}

/*************************************************************************/

void cilk_tool_init(void) {
  // Do the initialization only if it hasn't been done. 
  // It could have been done if we instrument C functions, and the user 
  // separately calls cilk_tool_init in the main function.
  if(!TOOL_INITIALIZED) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_init()\n"); );

    initialize_tool(&GET_STACK(ctx_stack));

    GET_STACK(ctx_stack).in_user_code = true;

    start_strand(&(GET_STACK(ctx_stack).strand_ruler));
  }
}

/* Cleaningup; note that these cleanup may not be performed if
 * the user did not include cilk_tool_destroy in its main function and the
 * program is not compiled with -fcilktool_instr_c.
 */
void cilk_tool_destroy(void) {
  // Do the destroy only if it hasn't been done. 
  // It could have been done if we instrument C functions, and the user 
  // separately calls cilk_tool_destroy in the main function.
  if(TOOL_INITIALIZED) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_destroy()\n"); );

    cilkprof_stack_t *stack = &GET_STACK(ctx_stack);
    // Pop the stack
    cilkprof_stack_frame_t *old_bottom = cilkprof_stack_pop(stack);

    assert(old_bottom && MAIN == old_bottom->func_type);

#if !SERIAL_TOOL
    CILK_C_UNREGISTER_REDUCER(ctx_stack);
#endif
    free_cc_hashtable(stack->wrk_table);
    free_cc_hashtable(old_bottom->prefix_table);
    free_cc_hashtable(old_bottom->lchild_table);
    free_cc_hashtable(old_bottom->contin_table);
    free(old_bottom);

    TOOL_INITIALIZED = false;
  }
}

void cilk_tool_print(void) {
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_print()\n"); );

  assert(TOOL_INITIALIZED);

  cilkprof_stack_t *stack;
  stack = &GET_STACK(ctx_stack);

  assert(NULL != stack->bot);
  assert(MAIN == stack->bot->func_type);

  uint64_t span = stack->bot->prefix_spn + stack->bot->contin_spn;

  add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));
  clear_cc_hashtable(stack->bot->contin_table);

  flush_cc_hashtable(&(stack->bot->prefix_table));

  cc_hashtable_t* span_table = stack->bot->prefix_table;
  fprintf(stderr, "span_table->list_size = %d, span_table->table_size = %d, span_table->lg_capacity = %d\n",
  	  span_table->list_size, span_table->table_size, span_table->lg_capacity);

  uint64_t work = stack->bot->running_wrk;
  flush_cc_hashtable(&(stack->wrk_table));
  cc_hashtable_t* work_table = stack->wrk_table;
  fprintf(stderr, "work_table->list_size = %d, work_table->table_size = %d, work_table->lg_capacity = %d\n",
  	  work_table->list_size, work_table->table_size, work_table->lg_capacity);

  // Read the proc maps list
  read_proc_maps();

  // Parse tables
  size_t span_table_entries_read = 0;
  for (size_t i = 0; i < (1 << work_table->lg_capacity); ++i) {
    cc_hashtable_entry_t *entry = &(work_table->entries[i]);
    if (!empty_entry_p(entry)) {
      uint64_t wrk_wrk = entry->wrk;
      uint64_t spn_wrk = entry->spn;
      uint64_t wrk_spn = 0;
      uint64_t spn_spn = 0;

      for (size_t j = 0; j < (1 << span_table->lg_capacity); ++j) {
	cc_hashtable_entry_t *st_entry = &(span_table->entries[j]);
	if (empty_entry_p(st_entry)) {
	  continue;
	}
	if (st_entry->rip == entry->rip &&
	    st_entry->height == entry->height) {
	  ++span_table_entries_read;
	  wrk_spn = st_entry->wrk;
	  spn_spn = st_entry->spn;
	}
      }

      fprintf(stdout, "%lx:%d ", rip2cc(entry->rip), entry->height);
      print_addr(rip2cc(entry->rip));
      fprintf(stdout, " %lu %lu %lu %lu\n",
	      wrk_wrk, spn_wrk, wrk_spn, spn_spn);
    }
  }

  fprintf(stderr, "work = %fs, span = %fs, parallelism = %f\n",
	  work / (1000000000.0),
	  span / (1000000000.0),
	  work / (float)span);

  /* fprintf(stderr, "%ld read, %d size\n", span_table_entries_read, span_table->table_size); */
  assert(span_table_entries_read == span_table->table_size);

  // Free the proc maps list
  mapping_list_el_t *map_lst_el = maps.head;
  mapping_list_el_t *next_map_lst_el;
  while (NULL != map_lst_el) {
    next_map_lst_el = map_lst_el->next;
    free(map_lst_el);
    map_lst_el = next_map_lst_el;
  }
}


/*************************************************************************/
/**
 * Hooks into runtime system.
 */

void cilk_enter_begin(__cilkrts_stack_frame *sf, void* rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_begin(%p, %p)\n", sf, rip); );

  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if (!TOOL_INITIALIZED) {
    initialize_tool(&(ctx_stack));

  } else {
    stack = &(GET_STACK(ctx_stack));

    measure_and_add_strand_length(stack);

    stack->in_user_code = false;
  }

  // Push new frame onto the stack
  cilkprof_stack_push(stack, SPAWNER);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip);
}


void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *rip)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_helper_begin(%p, %p)\n", sf, rip); );

  // We should have reached this after passing a cilk_spawn_or_continue(0)
  assert(!stack->in_user_code);
  /* measure_and_add_strand_length(stack); */
  /* stack->in_user_code = false; */

  // Push new frame onto the stack
  cilkprof_stack_push(stack, HELPER);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if (SPAWNER == stack->bot->func_type) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from SPAWNER\n", sf, rsp); );
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from HELPER\n", sf, rsp); );
  }
  assert(!(stack->in_user_code));

  stack->in_user_code = true;
  start_strand(&(stack->strand_ruler));
}

void cilk_tool_c_function_enter(void *rip)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_enter(%p)\n", rip); );

  if(!TOOL_INITIALIZED) { // We are entering main.
    cilk_tool_init(); // this will push the frame for MAIN and do a gettime
  } else {
    assert(stack->in_user_code);

    measure_and_add_strand_length(stack);

    // Push new frame for this C function onto the stack
    cilkprof_stack_push(stack, C_FUNCTION);
    stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip);
    /* the stop time is also the start time of this function */
    // stack->start = stack->stop; /* TB: Want to exclude the length
    // (e.g. time or instruction count) of this function */
    start_strand(&(stack->strand_ruler));
  }
}

void cilk_tool_c_function_leave(void *rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_leave(%p)\n", rip); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if(stack->bot && MAIN == stack->bot->func_type) {
    cilk_tool_destroy();
  }
  if(!TOOL_INITIALIZED) {
    // either user code already called cilk_tool_destroy, or we are leaving
    // main; in either case, nothing to do here;
    return;
  }

  bool add_success;
  cilkprof_stack_frame_t *old_bottom;

  assert(stack->in_user_code);
  // stop the timer and attribute the elapsed time to this returning
  // function
  measure_and_add_strand_length(stack);

  assert(NULL != stack->bot->parent);
  // Given this is a C function, everything should be accumulated in
  // contin_spn and contin_table, so let's just deposit that into the
  // parent.
  assert(0 == stack->bot->prefix_spn);
  assert(0 == stack->bot->lchild_spn);
  assert(cc_hashtable_is_empty(stack->bot->prefix_table));
  assert(cc_hashtable_is_empty(stack->bot->lchild_table));

  // Pop the stack
  old_bottom = cilkprof_stack_pop(stack);

  stack->bot->running_wrk += old_bottom->running_wrk;
  stack->bot->contin_spn += old_bottom->contin_spn;

  // Update work table
  /* fprintf(stderr, "adding to wrk table\n"); */
  add_success = add_to_cc_hashtable(&(stack->wrk_table),
                                    old_bottom->height,
				    old_bottom->rip,
				    old_bottom->running_wrk,
				    old_bottom->contin_spn);
  assert(add_success);

  // Update continuation span table
  /* fprintf(stderr, "adding tables\n"); */
  add_cc_hashtables(&(stack->bot->contin_table), &(old_bottom->contin_table));

  /* fprintf(stderr, "adding to contin table\n"); */
  add_success = add_to_cc_hashtable(&(stack->bot->contin_table),
				    old_bottom->height,
				    old_bottom->rip,
				    old_bottom->running_wrk,
				    old_bottom->contin_spn);
  assert(add_success);

  // clean up
  free(old_bottom->prefix_table);
  free(old_bottom->contin_table);
  free(old_bottom->lchild_table);
  free(old_bottom);

  start_strand(&(stack->strand_ruler));
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_spawn_prepare(%p)\n", sf); );

  // Tool must have been initialized as this is only called in a SPAWNER, and 
  // we would have at least initialized the tool in the first cilk_enter_begin.
  assert(TOOL_INITIALIZED);

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  measure_and_add_strand_length(stack);

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

// If in_continuation == 0, we just did setjmp and about to call the spawn helper.  
// If in_continuation == 1, we are resuming after setjmp (via longjmp) at the continuation 
// of a spawn statement; note that this is possible only if steals can occur.
void cilk_spawn_or_continue(int in_continuation)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  assert(!(stack->in_user_code));
  if (in_continuation) {
    // In the continuation
    WHEN_TRACE_CALLS(
        fprintf(stderr, "cilk_spawn_or_continue(%d) from continuation\n", in_continuation); );
    stack->in_user_code = true;

    start_strand(&(stack->strand_ruler));

  } else {
    // In the spawned child
    WHEN_TRACE_CALLS(
        fprintf(stderr, "cilk_spawn_or_continue(%d) from spawn\n", in_continuation); );
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_begin(%p)\n", parent); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));
  assert(HELPER == stack->bot->func_type);

  measure_and_add_strand_length(stack);

  assert(stack->in_user_code);
  stack->in_user_code = false;

  return;
}

void cilk_detach_end(void)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_end()\n"); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));
  
  assert(!(stack->in_user_code));
  stack->in_user_code = true;

  start_strand(&(stack->strand_ruler));

  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  measure_and_add_strand_length(stack);

  if (SPAWNER == stack->bot->func_type) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_begin(%p) from SPAWNER\n", sf); );
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf); );
  }

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if (stack->bot->lchild_spn > stack->bot->contin_spn) {
    stack->bot->prefix_spn += stack->bot->lchild_spn;
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->lchild_table));
  } else {
    stack->bot->prefix_spn += stack->bot->contin_spn;
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));
  }

  stack->bot->lchild_spn = 0;
  stack->bot->contin_spn = 0;
  clear_cc_hashtable(stack->bot->lchild_table);
  clear_cc_hashtable(stack->bot->contin_table);
  
  // can't be anything else; only SPAWNER have sync
  assert(SPAWNER == stack->bot->func_type); 
  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from SPAWNER\n", sf); );
  start_strand(&(stack->strand_ruler));
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_leave_begin(%p)\n", sf); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  cilkprof_stack_frame_t *old_bottom;
  bool add_success;

  measure_and_add_strand_length(stack);

  assert(stack->in_user_code);
  stack->in_user_code = false;

  if (SPAWNER == stack->bot->func_type) {
    // This is the case we are returning to a call, since a SPAWNER
    // is always called by a spawn helper.
    /* fprintf(stderr, "cilk_leave_begin(%p) from SPAWNER\n", sf); */

    assert(NULL != stack->bot->parent);
    // We are at leave, so this function must have sync-ed, so lchild
    // should be 0 / empty; prefix could contain value, however, if
    // this function is Cilk function that spawned before.
    assert(0 == stack->bot->lchild_spn);
    assert(cc_hashtable_is_empty(stack->bot->lchild_table));
    stack->bot->prefix_spn += stack->bot->contin_spn;

    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));

    // Pop the stack
    old_bottom = cilkprof_stack_pop(stack);

    stack->bot->running_wrk += old_bottom->running_wrk;
    stack->bot->contin_spn += old_bottom->prefix_spn;

    /* fprintf(stderr, "adding to wrk table\n"); */

    // Update work table
    add_success = add_to_cc_hashtable(&(stack->wrk_table),
				      old_bottom->height,
				      old_bottom->rip,
				      old_bottom->running_wrk,
				      old_bottom->prefix_spn);
    assert(add_success);

    /* fprintf(stderr, "adding tables\n"); */

    // Update continuation span table
    add_cc_hashtables(&(stack->bot->contin_table), &(old_bottom->prefix_table));

    free(old_bottom->prefix_table);
    free(old_bottom->lchild_table);
    free(old_bottom->contin_table);

    /* fprintf(stderr, "adding to contin table\n"); */

    add_success = add_to_cc_hashtable(&(stack->bot->contin_table),
				      old_bottom->height,
				      old_bottom->rip,
				      old_bottom->running_wrk,
				      old_bottom->prefix_spn);
    assert(add_success);

  } else {
    // This is the case we are returning to a spawn, since a HELPER 
    // is always invoked due to a spawn statement.
    /* fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf); */

    assert(HELPER != stack->bot->parent->func_type);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));

    // Pop the stack
    old_bottom = cilkprof_stack_pop(stack);

    stack->bot->running_wrk += old_bottom->running_wrk;

    // Update running work table
    add_success = add_to_cc_hashtable(&(stack->wrk_table),
				      old_bottom->height,
				      old_bottom->rip,
				      old_bottom->running_wrk,
				      old_bottom->prefix_spn);
    assert(add_success);

    if (stack->bot->contin_spn + old_bottom->prefix_spn > stack->bot->lchild_spn) {
      // fprintf(stderr, "updating longest child\n");
      stack->bot->prefix_spn += stack->bot->contin_spn;

      // This needs a better data structure to be implemented more
      // efficiently.
      add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));

      stack->bot->lchild_spn = old_bottom->prefix_spn;
      free(stack->bot->lchild_table);
      stack->bot->lchild_table = old_bottom->prefix_table;
      free(old_bottom->lchild_table);
      free(old_bottom->contin_table);

      // Update lchild span table
      add_success = add_to_cc_hashtable(&(stack->bot->lchild_table),
					old_bottom->height,
					old_bottom->rip,
					old_bottom->running_wrk,
					old_bottom->prefix_spn);
      assert(add_success);

      stack->bot->contin_spn = 0;
      clear_cc_hashtable(stack->bot->contin_table);

    } else {
      free(old_bottom->prefix_table);
      free(old_bottom->lchild_table);
      free(old_bottom->contin_table);
    }
  }

  free(old_bottom);
}

void cilk_leave_end(void)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

#if TRACE_CALLS
  switch(stack->bot->func_type) {
  case HELPER:
    fprintf(stderr, "cilk_leave_end() from HELPER\n");
    break;
  case SPAWNER:
    fprintf(stderr, "cilk_leave_end() from SPAWNER\n");
    break;
  case MAIN:
    fprintf(stderr, "cilk_leave_end() from MAIN\n");
    break;
  case C_FUNCTION:
    // We can have leave_end from C_FUNCTION because leave_begin
    // popped the stack already
    fprintf(stderr, "cilk_leave_end() from C_FUNCTION\n");
    break;
  }
#endif

  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  start_strand(&(stack->strand_ruler));
}

