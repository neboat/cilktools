#ifndef INCLUDED_CILKPROF_STACK_H
#define INCLUDED_CILKPROF_STACK_H

#include <stdbool.h>
#include <time.h>
/* #define _POSIX_C_SOURCE = 200112L */
/* #define _POSIX_C_SOURCE = 200809L */

#include "cc_hashtable.h"

/* Enum for types of functions */
typedef enum {
  MAIN,
  SPAWNER,
  HELPER,
  C_FUNCTION,
} cilk_function_type;

/* Type for a cilkprof stack frame */
typedef struct cilkprof_stack_frame_t {
  /* Function type */
  cilk_function_type func_type;

  /* Height of the function */
  int32_t height;

  /* Return address of this function */
  uintptr_t rip;

  /* Pointer to the frame's parent */
  struct cilkprof_stack_frame_t *parent;

  /* Running work of this function */
  uint64_t running_wrk;

  /* Span of the prefix of this function */
  uint64_t prefix_spn;

  /* Data associated with the function's prefix */
  cc_hashtable_t* prefix_table;

  /* Span of the longest spawned child of this function observed so
     far */
  uint64_t lchild_spn;

  /* Data associated with the function's longest child */
  cc_hashtable_t* lchild_table;

  /* Span of the continuation of the function since the spawn of its
     longest child */
  uint64_t contin_spn;

  /* Data associated with the function's continuation */
  cc_hashtable_t* contin_table;

} cilkprof_stack_frame_t;


/* Type for a cilkprof stack */
typedef struct {
  /* Flag to indicate whether user code is being executed.  This flag
     is mostly used for debugging. */
  bool in_user_code;

  /* Start and stop timers for measuring the execution time of a
     strand. */
  struct timespec start;
  struct timespec stop;

  /* Pointer to bottom of the stack, onto which frames are pushed. */
  cilkprof_stack_frame_t *bot;

  /* Data associated with the running work */
  cc_hashtable_t* wrk_table;

} cilkprof_stack_t;


/* Initializes the cilkprof stack frame *frame */
void cilkprof_stack_frame_init(cilkprof_stack_frame_t *frame, cilk_function_type func_type)
{
  frame->parent = NULL;
  frame->func_type = func_type;
  frame->rip = 0; // (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
  frame->height = 0;

  frame->running_wrk = 0;

  frame->prefix_spn = 0;
  frame->prefix_table = cc_hashtable_create();
  frame->lchild_spn = 0;
  frame->lchild_table = cc_hashtable_create();
  frame->contin_spn = 0;
  frame->contin_table = cc_hashtable_create();
}


/* Initializes the cilkprof stack */
void cilkprof_stack_init(cilkprof_stack_t *stack, cilk_function_type func_type)
{
  cilkprof_stack_frame_t *new_frame =
    (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));
  cilkprof_stack_frame_init(new_frame, func_type);
  stack->bot = new_frame;
  stack->wrk_table = cc_hashtable_create();
  stack->in_user_code = false;
}


/* Push new frame of function type func_type onto the stack *stack */
cilkprof_stack_frame_t* cilkprof_stack_push(cilkprof_stack_t *stack, cilk_function_type func_type)
{
  cilkprof_stack_frame_t *new_frame
    = (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));
  cilkprof_stack_frame_init(new_frame, func_type);
  new_frame->parent = stack->bot;
  stack->bot = new_frame;

  return new_frame;
}


/* Pops the bottommost frame off of the stack *stack, and returns a
   pointer to it. */
cilkprof_stack_frame_t* cilkprof_stack_pop(cilkprof_stack_t *stack)
{
  cilkprof_stack_frame_t *old_bottom = stack->bot;
  stack->bot = stack->bot->parent;
  if (stack->bot && stack->bot->height < old_bottom->height + 1) {
    stack->bot->height = old_bottom->height + 1;
  }

  return old_bottom;
}

#endif
