#ifndef INCLUDED_VIEWREAD_STACK_H
#define INCLUDED_VIEWREAD_STACK_H

#include <stdio.h>
#include <stdbool.h>

static uint64_t next_func_id = 1;

// Enum for types of functions
typedef enum {
  MAIN,
  CILK,
  HELPER,
} FunctionType_t;

// Enum for types of bags
typedef enum {
  R,
  D,
} BagType_t;

typedef struct DisjointSet_t {
  BagType_t type;
  uint64_t init_func_id;
  uint64_t set_func_id;

  struct DisjointSet_t *parent;
  uint64_t rank;
} DisjointSet_t;

// Typedef of viewread stack frame
typedef struct viewread_stack_frame_t {
  FunctionType_t func_type;

  uint64_t ancestor_spawns;
  uint64_t local_spawns;

  DisjointSet_t* r_bag;
  DisjointSet_t* d_bag;

  /* DisjointSet_t* ss_bag; */
  /* DisjointSet_t* sp_bag; */
  /* DisjointSet_t* p_bag; */

  struct viewread_stack_frame_t *parent;

} viewread_stack_frame_t;

// Type for a viewread stack
typedef struct {
  /* Flag to indicate whether user code is being executed.  This flag
     is mostly used for debugging. */
  bool in_user_code;

  /* Pointer to bottom of the stack, onto which frames are pushed. */
  viewread_stack_frame_t *bot;

} viewread_stack_t;

void DisjointSet_init(DisjointSet_t *s) {
  s->parent = s;
  s->rank = 0;
}

/*
 * The only reason we need this function is to ensure that  
 * the _set_node returned for representing this set is the oldest 
 * node in the set.  
 */
void DisjointSet_swap_set_node(DisjointSet_t *s, DisjointSet_t *t) {
  uint64_t tmp = s->set_func_id;
  s->set_func_id = t->set_func_id;
  t->set_func_id = tmp;
}

void DisjointSet_link(DisjointSet_t *s, DisjointSet_t *t) {
  if (s->rank > t->rank) {
    t->parent = s;
  } else {
    s->parent = t;
    if (s->rank == t->rank) {
      ++t->rank;
    }
    DisjointSet_swap_set_node(s, t);
  }
}

DisjointSet_t* DisjointSet_find_set(DisjointSet_t *s) {
  if (s->parent != s) {
    s->parent = DisjointSet_find_set(s->parent);
  }
  return s->parent;
}

/*
 * Unions disjoint sets s and t.
 *
 * NOTE: implicitly, in order to maintain the oldest _set_node, one
 * should always combine younger set into this set (defined by
 * creation time).  Since we union by rank, we may end up linking
 * this set to the younger set.  To make sure that we always return
 * the oldest _node to represent the set, we use an additional
 * _set_node field to keep track of the oldest node and use that to
 * represent the set.
 *
 * @param s (older) disjoint set.
 * @param t (younger) disjoint set.
 */
// Called "combine," because "union" is a reserved keyword in C
void DisjointSet_combine(DisjointSet_t *s, DisjointSet_t *t) {
  assert(t);
  assert(DisjointSet_find_set(s) != DisjointSet_find_set(t));
  DisjointSet_link(DisjointSet_find_set(s), DisjointSet_find_set(t));
  assert(DisjointSet_find_set(s) == DisjointSet_find_set(t));
}


/* Initializes the viewread stack frame *frame */
void viewread_stack_frame_init(viewread_stack_frame_t *frame,
                               FunctionType_t func_type,
                               uint64_t ancestor_spawns)
{
  frame->parent = NULL;
  frame->func_type = func_type;

  frame->ancestor_spawns = ancestor_spawns;
  frame->local_spawns = 0;

  frame->r_bag = (DisjointSet_t*)malloc(sizeof(DisjointSet_t));
  DisjointSet_init(frame->r_bag);
  frame->r_bag->type = R;
  frame->r_bag->init_func_id = next_func_id++;
  frame->r_bag->set_func_id = frame->r_bag->init_func_id;

  frame->d_bag = NULL;
}


/* Initializes the viewread stack */
void viewread_stack_init(viewread_stack_t *stack, FunctionType_t func_type)
{
  viewread_stack_frame_t *new_frame
      = (viewread_stack_frame_t *)malloc(sizeof(viewread_stack_frame_t));
  viewread_stack_frame_init(new_frame, func_type, 0);
  stack->bot = new_frame;
  stack->in_user_code = false;
}


/* Push new frame of function type func_type onto the stack *stack */
viewread_stack_frame_t* viewread_stack_push(viewread_stack_t *stack,
                                            FunctionType_t func_type)
{
  viewread_stack_frame_t *new_frame
      = (viewread_stack_frame_t *)malloc(sizeof(viewread_stack_frame_t));
  if (HELPER == func_type) {  // Function was spawned
    stack->bot->local_spawns++;
  }
  // Initialize the new frame, using the current frame's current view
  // ID as the inital and current view ID of the new frame.
  viewread_stack_frame_init(new_frame, func_type,
                            stack->bot->ancestor_spawns + stack->bot->local_spawns);
  new_frame->parent = stack->bot;
  stack->bot = new_frame;

  return new_frame;
}


/* Pops the bottommost frame off of the stack *stack, and returns a
   pointer to it. */
viewread_stack_frame_t* viewread_stack_pop(viewread_stack_t *stack)
{
  viewread_stack_frame_t *old_bottom = stack->bot;
  stack->bot = stack->bot->parent;

  return old_bottom;
}

#endif
