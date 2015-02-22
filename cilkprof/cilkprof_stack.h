#ifndef INCLUDED_CILKPROF_STACK_H
#define INCLUDED_CILKPROF_STACK_H

#include <stdbool.h>
#include <time.h>

#ifndef COMPUTE_STRAND_DATA
#define COMPUTE_STRAND_DATA 0
#endif

// TB: Use this instead of strand_time.h to count strands.  Unlike
// time, this counts the number of strands encountered, which should
// be deterministic.
/* #include "strand_count.h" */
/* #include "strand_time.h" */
#include "strand_time_rdtsc.h"
#include "cc_hashtable.h"
#if COMPUTE_STRAND_DATA
#include "strand_hashtable.h"
#endif

typedef struct c_fn_frame_t {
  // We don't have that many different flags yet,
  // so we can just use bools.
  bool top_cs;
  bool top_fn;

  // Return address of this function
  uintptr_t rip;
  // Address of this function
  uintptr_t function;

  // Work and span values to store for every function
  uint64_t local_wrk;
  /* uint64_t local_contin; */
  uint64_t running_wrk;
  uint64_t running_spn;

  // Parent of this C function on the same stack
  struct c_fn_frame_t *parent;
} c_fn_frame_t;

// Type for cilkprof stack frame
typedef struct cilkprof_stack_frame_t {
  /* // We don't have that many different flags yet, */
  /* // so we can just use bools. */
  /* bool top_cs; */
  /* bool top_fn; */

  // Function type
  FunctionType_t func_type;

  /* // Depth of the function */
  /* int32_t depth; */
  /* // Return address of this function */
  /* uintptr_t rip; */
  /* // Address of this function */
  /* uintptr_t function; */

  // Pointer to the frame's parent
  struct cilkprof_stack_frame_t *parent;

  /* // Running work of this function and its child C functions */
  /* uint64_t running_wrk; */
  /* // Span of the continuation of the function since the spawn of its */
  /* // longest child */
  /* uint64_t contin_spn; */

  // Local work and span of this function and its child C functions.  These
  // work and span values are maintained as a stack.
  c_fn_frame_t *c_fn_frame;

  /* // Local work of this function */
  /* uint64_t local_wrk; */

  // Local continuation span of this function
  uint64_t local_contin;
  // Local span of this function
  uint64_t local_spn;

  // Span of the prefix of this function and its child C functions
  uint64_t prefix_spn;

  // Span of the longest spawned child of this function observed so
  // far
  uint64_t lchild_spn;

  // The span of the continuation is stored in the running_spn
  // variable in the topmost c_fn_frame

  // Data associated with the function's prefix
  cc_hashtable_t* prefix_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with prefix
  strand_hashtable_t* strand_prefix_table;
#endif

  // Data associated with the function's longest child
  cc_hashtable_t* lchild_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with longest child
  strand_hashtable_t* strand_lchild_table;
#endif

  // Data associated with the function's continuation
  cc_hashtable_t* contin_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with continuation
  strand_hashtable_t* strand_contin_table;
#endif
} cilkprof_stack_frame_t;


// Type for a cilkprof stack
typedef struct {
  // Flag to indicate whether user code is being executed.  This flag
  // is mostly used for debugging.
  bool in_user_code;

#if COMPUTE_STRAND_DATA
  // Endpoints of currently executing strand
  uintptr_t strand_start;
  uintptr_t strand_end;
#endif

  // Tool for measuring the length of a strand
  strand_ruler_t strand_ruler;

  // Free list of C function frames
  c_fn_frame_t *c_fn_free_list;

  // Free list of cilkprof stack frames
  cilkprof_stack_frame_t *sf_free_list;

  // Pointer to bottom of the stack, onto which frames are pushed.
  cilkprof_stack_frame_t *bot;

  // Call-site data associated with the running work
  cc_hashtable_t* wrk_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with running work
  strand_hashtable_t* strand_wrk_table;
#endif
} cilkprof_stack_t;


/*----------------------------------------------------------------------*/

// Initializes C function frame *c_fn_frame
static inline
void cilkprof_c_fn_frame_init(c_fn_frame_t *c_fn_frame) {
  c_fn_frame->top_cs = false;
  c_fn_frame->top_fn = false;

  c_fn_frame->rip = (uintptr_t)NULL;
  c_fn_frame->function = (uintptr_t)NULL;

  c_fn_frame->local_wrk = 0;
  /* c_fn_frame->local_contin = 0; */
  c_fn_frame->running_wrk = 0;
  c_fn_frame->running_spn = 0;

  c_fn_frame->parent = NULL;
}

// Initializes the cilkprof stack frame *frame
static inline
void cilkprof_stack_frame_init(cilkprof_stack_frame_t *frame, FunctionType_t func_type)
{
  frame->parent = NULL;

  frame->func_type = func_type;

  cilkprof_c_fn_frame_init(frame->c_fn_frame);

  /* frame->top_cs = false; */
  /* frame->top_fn = false; */

  /* frame->rip = 0;  // (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  /* /\* frame->depth = 0; *\/ */

  /* frame->local_wrk = 0; */
  frame->local_spn = 0;
  frame->local_contin = 0;

  /* frame->running_wrk = 0; */

  frame->prefix_spn = 0; 
  frame->lchild_spn = 0;
  /* frame->contin_spn = 0; */

  assert(cc_hashtable_is_empty(frame->prefix_table));
  /* clear_cc_hashtable(frame->prefix_table); */
  /* frame->prefix_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
  assert(strand_hashtable_is_empty(frame->strand_prefix_table));
  /* clear_strand_hashtable(frame->strand_prefix_table); */
  /* frame->strand_prefix_table = strand_hashtable_create(); */
#endif
  assert(cc_hashtable_is_empty(frame->lchild_table));
  /* clear_cc_hashtable(frame->lchild_table); */
  /* frame->lchild_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
  assert(strand_hashtable_is_empty(frame->strand_lchild_table));
  /* clear_strand_hashtable(frame->strand_lchild_table); */
  /* frame->strand_lchild_table = strand_hashtable_create(); */
#endif
  assert(cc_hashtable_is_empty(frame->contin_table));
  /* clear_cc_hashtable(frame->contin_table); */
  /* frame->contin_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
  assert(strand_hashtable_is_empty(frame->strand_contin_table));
  /* clear_strand_hashtable(frame->strand_contin_table); */
  /* frame->strand_contin_table = strand_hashtable_create(); */
#endif
}


// Push new frame of C function onto the C function stack starting at
// stack->bot->c_fn_frame.
c_fn_frame_t* cilkprof_c_fn_push(cilkprof_stack_t *stack)
{
  assert(NULL != stack->bot);
  assert(NULL != stack->bot->c_fn_frame);

  c_fn_frame_t *new_frame;
  if (NULL != stack->c_fn_free_list) {
    new_frame = stack->c_fn_free_list;
    stack->c_fn_free_list = stack->c_fn_free_list->parent;
  } else {
    new_frame = (c_fn_frame_t *)malloc(sizeof(c_fn_frame_t));
  }

  cilkprof_c_fn_frame_init(new_frame);
  new_frame->parent = stack->bot->c_fn_frame;
  stack->bot->c_fn_frame = new_frame;

  return new_frame;
}


// Push new frame of function type func_type onto the stack *stack
cilkprof_stack_frame_t* cilkprof_stack_push(cilkprof_stack_t *stack, FunctionType_t func_type)
{
  cilkprof_stack_frame_t *new_frame;
  if (NULL != stack->sf_free_list) {
    new_frame = stack->sf_free_list;
    stack->sf_free_list = stack->sf_free_list->parent;
  } else {
    new_frame = (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));

    c_fn_frame_t *new_c_frame;
    if (NULL != stack->c_fn_free_list) {
      new_c_frame = stack->c_fn_free_list;
      stack->c_fn_free_list = stack->c_fn_free_list->parent;
    } else {
      new_c_frame = (c_fn_frame_t *)malloc(sizeof(c_fn_frame_t));
    }
    new_frame->c_fn_frame = new_c_frame;

    new_frame->prefix_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
    new_frame->strand_prefix_table = strand_hashtable_create();
#endif
    new_frame->lchild_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
    new_frame->strand_lchild_table = strand_hashtable_create();
#endif
    new_frame->contin_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
    new_frame->strand_contin_table = strand_hashtable_create();
#endif
  }
  
  cilkprof_stack_frame_init(new_frame, func_type);
  new_frame->parent = stack->bot;
  stack->bot = new_frame;
  if (new_frame->parent) {
    /* new_frame->depth = new_frame->parent->depth + 1; */
  }

  return new_frame;
}


// Initializes the cilkprof stack
void cilkprof_stack_init(cilkprof_stack_t *stack, FunctionType_t func_type)
{
  stack->sf_free_list = NULL;
  stack->bot = NULL;
  cilkprof_stack_push(stack, func_type);
  /* cilkprof_stack_frame_t *new_frame = */
  /*   (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t)); */
  /* cilkprof_stack_frame_init(new_frame, func_type); */
  /* stack->bot = new_frame; */

  stack->wrk_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  stack->strand_wrk_table = strand_hashtable_create();
#endif
  /* stack->unique_call_sites = NULL; */

  init_strand_ruler(&(stack->strand_ruler));
  stack->in_user_code = false;
}


// Pops the bottommost C frame off of the stack
// stack->bot->c_fn_frame, and returns a pointer to it.
c_fn_frame_t* cilkprof_c_fn_pop(cilkprof_stack_t *stack)
{
  c_fn_frame_t *old_c_bot = stack->bot->c_fn_frame;
  stack->bot->c_fn_frame = stack->bot->c_fn_frame->parent;
  assert(NULL != stack->bot->c_fn_frame);
  return old_c_bot;
}


// Pops the bottommost frame off of the stack *stack, and returns a
// pointer to it.
cilkprof_stack_frame_t* cilkprof_stack_pop(cilkprof_stack_t *stack)
{
  cilkprof_stack_frame_t *old_bottom = stack->bot;
  stack->bot = stack->bot->parent;
  // if (stack->bot && stack->bot->height < old_bottom->height + 1) {
  //  stack->bot->height = old_bottom->height + 1;
  // }

  return old_bottom;
}

#endif
