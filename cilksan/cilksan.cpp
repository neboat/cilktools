#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unordered_map>
// #include <map>
#include <vector>

#include <execinfo.h>
#include <inttypes.h> 

#include "cilksan_internal.h"
#include "debug_util.h"
#include "disjointset.h"
#include "mem_access.h"
#include "stack.h"
#include "spbag.h"


#if CILKSAN_DEBUG
enum EventType_t last_event = NONE;  
static bool CILKSAN_INITIALIZED = false;
#endif

// declared in driver.cpp
extern FILE *err_io;

// --------------------- stuff from racedetector ---------------------------

// -------------------------------------------------------------------------
//  Analysis data structures and fields
// -------------------------------------------------------------------------

#if CILKSAN_DEBUG
template<typename DISJOINTSET_DATA_T> 
long DisjointSet_t<DISJOINTSET_DATA_T>::debug_count = 0;

long SBag_t::debug_count = 0;
long PBag_t::debug_count = 0;
#endif

#define NOP_STEAL 0      // a steal point that skips over the first interval
#define MAX_NUM_STEALS 3 // max number of steal points; just need 3 to 
                         // define the unit reduce operation to check

// range of stack used by the process 
uint64_t stack_low_addr = 0; 
uint64_t stack_high_addr = 0;

// small helper functions
static inline bool is_on_stack(uint64_t addr) {
    cilksan_assert(stack_high_addr != stack_low_addr);
    return (addr <= stack_high_addr && addr >= stack_low_addr);
}

// macro for address manipulation for shadow mem
#define ADDR_TO_KEY(addr) ((uint64_t) ((uint64_t)addr >> 3))

// A sync block size is defined by the number of continuations within a 
// sync block that can potentially be stolen.  
//
// A sync block is created when: 
// - passing a cilk_sync
// - entering a Cilk context (including spawn helper at the point of detach)
// A sync block is terminated when:
// - encountering a cilk_sync 
// - returning from a Cilk function (called or spawned)
// - returning from a spawned C function 
// 
// the max sync block size we have encountered thus far
static uint32_t accounted_max_sync_block_size = 0;
// accumulates the sync block size for calcuating average 
static uint32_t accum_sync_block_size = 0;
static uint32_t num_of_sync_blocks = 0;

// Continuation depth (cont_depth) of a strand is defined by the number of
// continuations that can be stolen (i.e., after spawn) between root node in
// the dag and that strand along the "right-most" path, i.e., the path that
// takes the continuation edge instead of the spawn edge whenever possible. 
//
// the max continuation depth we have encountered thus far
static uint64_t accounted_max_cont_depth = 0;


// if set, we are stealing every continuation that can be stolen
static bool simulate_all_steals = false;
// if set, we are checking the reduce functions instead of updates
static bool check_reduce = false;
// check updates with simulated steals that occur at contiuation w/ this depth
static uint64_t cont_depth_to_check = 0;

// user specified steal points; 
// if set, every sync block simulate steals at these points.
static uint32_t steal_point1 = NOP_STEAL;
static uint32_t steal_point2 = NOP_STEAL;
static uint32_t steal_point3 = NOP_STEAL;

// the maximum sync block size from user input, which we will use for checking
// reduce / update functions 
static uint32_t max_sync_block_size = 0;

// ANGE: Each function that causes a Disjoint set to be created has a 
// unique ID (i.e., Cilk function and spawned C function).
// If a spawned function is also a Cilk function, a Disjoint Set is created
// for code between the point where detach finishes and the point the Cilk
// function calls enter_frame, which may be unnecessary in some case. 
// (But potentially necessary in the case where the base case is executed.)  
static uint64_t frame_id = 0;

// The view_id is used to identify the unique view that a bag is associated with 
// If two bags have the same value for their view_id, the memory accesses
// contained in the bags are based off the same view.  We use view_id to keep
// track the fact a view is created only upon a successful steal and that 
// called functions and the first spawned child inherits the most recent view
// created within the parent context.
static uint64_t view_id = 1;

// Struct for keeping track of shadow frame
typedef struct FrameData_t {
  DisjointSet_t<SPBagInterface *> *Sbag;
  // should never have more than 3 active intervals
  DisjointSet_t<SPBagInterface *> *Pbags[3];
  // index for the Pbag for current strand; the index should closely follow
  // the steal_index, where Pbag_index should points to the PBag for the
  // interval that's currently being executed
  uint16_t Pbag_index;

  // The following fields are needed for checking reducers
  uint32_t current_sync_block_size;
  // the continuation depth when entering the function
  uint32_t init_cont_depth;

  // at each sync block, there are at most 3 steals points that define the 
  // unique reduce operation that we are checking.  On the other hand, there 
  // is only one steal point if we are checking updates, which is always fixed
  // and determined by cont_depth_to_check.
  uint32_t steal_points[MAX_NUM_STEALS];
  uint16_t steal_index; // index for steal_points array

  int64_t curr_view_id; // the view_id for the currently executing strand

  FrameData_t() :
    Sbag(NULL), Pbags{NULL, NULL, NULL}, Pbag_index(0),
    current_sync_block_size(0), init_cont_depth(0), steal_index(0),
    curr_view_id(UNINIT_VIEW_ID) { }

  // remember to update this whenever new fields are added
  inline void init_new_function(DisjointSet_t<SPBagInterface *> *_sbag, 
                                uint32_t _init_cont_depth, 
                                uint64_t parent_vid) {
    Sbag = _sbag; // Pbags should be NULL already
    current_sync_block_size = 0;
    init_cont_depth = _init_cont_depth; 
    cilksan_assert(parent_vid != UNINIT_VIEW_ID);
    curr_view_id = parent_vid; // init view id with parent's current view id
    // steal points related stuff will be reset in randomize_steal_points
  }

} FrameData_t;

// Data associated with the stack of Cilk frames or spawned C frames.
// head contains the SP bags for the function we are currently processing
static Stack_t<FrameData_t> frame_stack;

// A container to keep track a set of disjointsets created that must be 
// freed at the end of the execution
static std::vector< DisjointSet_t<SPBagInterface *> * > dset_nodes; 

// Shadow memory, or the unordered hashmap that maps a memory address to its 
// last reader and writer
typedef std::unordered_map<uint64_t, MemAccessList_t *>::iterator ShadowMemIter_t;
// XXX throw float point exception at size 8844858
static std::unordered_map<uint64_t, MemAccessList_t *> shadow_mem;
// typedef std::map<uint64_t, MemAccessList_t *>::iterator ShadowMemIter_t;
// static std::map<uint64_t, MemAccessList_t *> shadow_mem;

/// The following are needed to determine when we enter and exit a
/// spawned function and their status in the runtime.
enum EntryType_t { SPAWNER = 1, HELPER = 2 };
enum FrameType_t { SHADOW_FRAME = 1, FULL_FRAME = 2 };

typedef struct Entry_t {
  enum EntryType_t entry_type;
  enum FrameType_t frame_type;
// fields that are for debugging purpose only
#if CILKSAN_DEBUG
  uint64_t frame_id;
  uint32_t prev_helper; // index of the HELPER frame right above this entry
                      // initialized only for a HELPER frame
#endif
} Entry_t;

// Stack to track entries into Cilk contexts and spawn helpers.
// This entry_stack will mirror the deque for this worker in the runtime 
//
// The head contains the most recent Cilk context --- 
// SPAWNER: we are in a Cilk function (or, in some serial code called by a
// Cilk function).
// HELPER: we are in a spawn helper (or, in some serial code called by the
// helper). 
//
// We simulate steals to check races for reducers.  The frame type mirrors 
// what's going on in the runtime deque.  
//
// SHADOW_FRAME: This frame has not been stolen.
// FULL_FRAME: This frame has been stolen before and promoted into a full frame.
static Stack_t<Entry_t> entry_stack;

// A stack keeping track of the current context (i.e., USER, UPDATE, or
// REDUCE).  We need a stack of context, because they can nest.  For
// example, we can invoke update operation in the user code, which in
// turn invokes the identity function.  Both are UPDATE, but we
// need to stay in UPDATE when the inner UPDATE context ends.
// XXX: Not doing anything with this right now; FIXME later
// declared in driver.c
static Stack_t<enum AccContextType_t> context_stack; 

// The following fields are for debugging purpose only, used by the tool to
// keep track of the runtime deque structure.
//
// The rts_deque_begin and rts_deque_end basically follows the head and tail
// pointers in the runtime, which specifies the frames on the deque.  
// Invariants: rts_deque_begin is always smaller than or equal to rts_deque_end.
// Case 1: rts_deque_begin < rts_deque_end: 
//   rts_deque_end must be pointing to a HELPER frame, and there is at least 
//   one stacklet on the runtime deque (possibly consist of a single frame
//   only --- either the root frame, or a FULL frame).  
// Case 2: rts_deque_begin == rts_deque_end: 
//   the worker has only one FULL frame that it is working on, which has not 
//   yet been pushed onto the deque yet (i.e., nothing can be stolen from this 
//   worker).
#if CILKSAN_DEBUG
static int rts_deque_begin;
static int rts_deque_end;
// The entry_stack index of the youngest active HELPER frame
static uint32_t youngest_active_helper = 0;
#endif

static struct __cilkrts_worker *worker;

// probably not good practice, but ...
#include "cilksan_reducer.h"


// extern functions, defined in print_addr.cpp
extern void print_race_report();
extern int get_num_races_found(); 


/*************************************************************************/
/**  Interfacing functions (stuff defined by cilksan lib called by 
 **  either user code or runtime)
/*************************************************************************/
extern "C" void __cilksan_begin_reduce_strand() {
  enum AccContextType_t cur = *(context_stack.head());
  DBG_TRACE(DEBUG_REDUCER, "Enter REDUCE context %u -> %u.\n", cur, REDUCE);
  context_stack.push();
  *(context_stack.head()) = REDUCE;
}

extern "C" void __cilksan_end_reduce_strand() {
  enum AccContextType_t cur = *(context_stack.head());
  cilksan_assert(cur == REDUCE);
  context_stack.pop();
  DBG_TRACE(DEBUG_REDUCER, "Leave REDUCE context %u -> %u.\n", cur, *context_stack.head());
}

extern "C" void __cilksan_begin_update_strand() {
  enum AccContextType_t cur = *(context_stack.head());
  context_stack.push();
  if(cur == REDUCE) {
    *(context_stack.head()) = REDUCE; // REDUCE subsumes UPDATE
    DBG_TRACE(DEBUG_REDUCER, 
              "Enter UPDATE context, subsumed by REDUCE %u -> %u.\n", cur, REDUCE);
  } else {
    *(context_stack.head()) = UPDATE;
    DBG_TRACE(DEBUG_REDUCER, "Enter UPDATE context %u -> %u.\n", cur, UPDATE);
  }
}

extern "C" void __cilksan_end_update_strand() {
  enum AccContextType_t cur = *(context_stack.head());
  cilksan_assert(cur == UPDATE || cur == REDUCE);
  context_stack.pop();
  DBG_TRACE(DEBUG_REDUCER, "Leave UPDATE context %u -> %u.\n", cur, *context_stack.head());
}

extern "C" int __cilksan_is_running() {
  return 1;
}

// This function gets called at the beginning of runtime startup.
// The runtime calls this function to see if cilksan intends to 
// simulate steals in this particular execution.
extern "C" int __cilksan_check_for_simulate_steals() {
  return (cont_depth_to_check || check_reduce || simulate_all_steals);
}

// This function gets called when the runtime is performing:
//
// A. the return protocol for a spawned child whose parent (call it f) has 
// been stolen; and 
// B. a non-trivial cilk_sync
// to figure out which "interval" we are in right now.
//
// Call the steal points i1, i2, and i3, as chosen by
// randomize_steal_points() function (and stored in f->steal_points[]).
// interval 1 will be [i1, i2-1] (inclusive both ends), and 
// interval 2 will be [i2, i3-1] (inclusive both ends).
// interval 0 will be what comes before i1 and interval 3 will be what 
// comes after i3; these two intervals could be empty.
//
// This function returns (current interval << 1) & end_of_interval
// see get_current_reduce_interval. 
extern "C" unsigned int __cilksan_check_for_reduce_interval(int spawn_ret) {
  return get_current_reduce_interval(spawn_ret);
}

// This function gets called when the runtime is about to 
// perform a merge of hypermaps.
extern "C" void __cilksan_invoke_reduce() {
  update_disjointsets(); 
}
 
/*************************************************************************/
/**  Events functions
/*************************************************************************/
/// Helper function for handling the start of a new sync block.
static inline void start_new_sync_block() {
  uint32_t sb_size = frame_stack.head()->current_sync_block_size;
  uint64_t current_cont_depth = frame_stack.head()->init_cont_depth + sb_size;

  num_of_sync_blocks++;
  accum_sync_block_size += sb_size;
  if( accounted_max_sync_block_size < sb_size ) {
    accounted_max_sync_block_size = sb_size;
  }
  if( accounted_max_cont_depth < current_cont_depth ) {
    accounted_max_cont_depth = current_cont_depth;
  } 
  frame_stack.head()->current_sync_block_size = 0; // reset
}

/// Helper function for merging returning child's bag into parent's 
static inline void merge_bag_from_returning_child(bool returning_from_spawn) {

  FrameData_t *parent = frame_stack.ancestor(1);
  FrameData_t *child = frame_stack.head();
  cilksan_assert(parent->Sbag);
  cilksan_assert(child->Sbag && child->Pbag_index == 0);
  cilksan_assert(!child->Pbags[0] && !child->Pbags[1] && !child->Pbags[2]);
  // should have encountered a cilk_sync before exit
  cilksan_assert(child->current_sync_block_size == 0);

  if(returning_from_spawn) {

    DBG_TRACE(DEBUG_BAGS, 
        "Merge bag from spawned child %ld to parent %ld, pindex: %d.\n",
        child->Sbag->get_set_node()->get_func_id(),
        parent->Sbag->get_set_node()->get_func_id(), parent->Pbag_index);

    DisjointSet_t<SPBagInterface *> *parent_pbag = 
                                        parent->Pbags[parent->Pbag_index];
    cilksan_assert(parent_pbag && parent_pbag->get_set_node()->is_PBag());
    cilksan_assert(child->Sbag->get_set_node()->is_SBag());
    parent_pbag->combine(child->Sbag);
    cilksan_assert(child->Sbag->get_set_node()->is_PBag());

  } else { // otherwise we are returning from a call
    DBG_TRACE(DEBUG_BAGS, "Merge bag from called child %ld to parent %ld.\n",
              child->Sbag->get_set_node()->get_func_id(), 
              parent->Sbag->get_set_node()->get_func_id());
    cilksan_assert( parent->Sbag->get_set_node()->is_SBag() ); 
    parent->Sbag->combine(child->Sbag);
  }
  DBG_TRACE(DEBUG_BAGS, "After merge, parent set node func id: %ld.\n", 
            parent->Sbag->get_set_node()->get_func_id());
  cilksan_assert(parent->Sbag->get_node()->get_func_id() == 
                      parent->Sbag->get_set_node()->get_func_id());

  child->Sbag = NULL;
}

/// Helper function for handling the start of a new function.
/// This function can be a spawned or called Cilk function or a 
/// spawned C function.  A called C function is treated as inlined.
static inline void start_new_function() {

  frame_id++;
  frame_stack.push(); 
  DBG_TRACE(DEBUG_CALLBACK, "Enter frame %ld.\n", frame_id);

  // get the parent pointer after we push, because once pused, the pointer 
  // may no longer be valid due to resize
  FrameData_t *parent = frame_stack.ancestor(1);
  DisjointSet_t<SPBagInterface *> *child_sbag, *parent_sbag = parent->Sbag;

  FrameData_t *child = frame_stack.head();
  cilksan_assert(child->Sbag == NULL && child->Pbag_index == 0);

  child_sbag = new DisjointSet_t<SPBagInterface *>( 
                   new SBag_t(frame_id, parent_sbag->get_node()) );
  cilksan_assert( child_sbag->get_set_node()->is_SBag() ); 
  dset_nodes.push_back(child_sbag);

  // reset necessary fields
  uint32_t init_cont_depth = parent->init_cont_depth + 
                           parent->current_sync_block_size;
  child->init_new_function(child_sbag, init_cont_depth, parent->curr_view_id); 
  
  DBG_TRACE(DEBUG_CALLBACK,
      "Enter function id %ld, init view id %lu and init cont depth %ld.\n", 
      frame_id, child->curr_view_id, init_cont_depth);
  start_new_sync_block();
}

/// Helper function for exiting a function; counterpart of start_new_function.
static inline void exit_function() {
  frame_stack.pop();
}

/// Action performed on entering a Cilk function (excluding spawn helper).
static inline void enter_cilk_function() {

  DBG_TRACE(DEBUG_CALLBACK, "entering a Cilk function, push frame_stack\n");
  start_new_function();
}

/// Action performed on leaving a Cilk function (excluding  spawn helper).
static inline void leave_cilk_function() {

  DBG_TRACE(DEBUG_CALLBACK, "leaving a Cilk function, pop frame_stack\n");

  /* param: not returning from a spawn */
  merge_bag_from_returning_child(0);
  exit_function();
}

/// Action performed on entering a spawned child.
/// (That is, right after detach.)
static inline void enter_spawn_child() {

  DBG_TRACE(DEBUG_CALLBACK, "done detach, push frame_stack\n");
  start_new_function();
}

/// Action performed when returning from a spawned child.
/// (That is, returning from a spawn helper.)
static inline void return_from_spawn() {

  DBG_TRACE(DEBUG_CALLBACK, "return from spawn helper, pop frame_stack\n");

  /* param: we are returning from a spawn */
  merge_bag_from_returning_child(1);
  exit_function();
} 

/// Action performed immediately after passing a sync.
static void complete_sync() {

  FrameData_t *f = frame_stack.head();
  DBG_TRACE(DEBUG_CALLBACK, "frame %d done sync\n", 
            f->Sbag->get_node()->get_func_id());

  // this was a special case: we skipped over the first interval
  if(f->steal_points[0] == NOP_STEAL && f->Pbag_index) {
    cilksan_assert(!f->Pbags[0] && f->Pbags[1] && f->Pbag_index == 1);
    f->Pbags[0] = f->Pbags[1];
    f->Pbags[1] = NULL;
    f->Pbag_index--; 
  }

  // should be only one PBag left to combine
  cilksan_assert(f->Pbag_index == 0);
  cilksan_assert(f->Sbag->get_set_node()->is_SBag()); 
  // Pbags[0] could be NULL if we encounter a sync without any spawn
  // (i.e., any Cilk function that executes the base case)
  if(f->Pbags[0]) {
    cilksan_assert( f->Pbags[0]->get_set_node()->is_PBag() );
    f->Sbag->combine( f->Pbags[0] );
    cilksan_assert( f->Pbags[0]->get_set_node()->is_SBag() );
    cilksan_assert( f->Sbag->get_node()->get_func_id() == 
                         f->Sbag->get_set_node()->get_func_id() );
    f->Pbags[0] = NULL;
  }

  start_new_sync_block();
}

//---------------------------------------------------------------
// Callback functions
//---------------------------------------------------------------
void cilksan_do_enter_begin() {

  cilksan_assert(CILKSAN_INITIALIZED);
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = ENTER_FRAME; )
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_enter_frame_begin\n", frame_id+1);

/*
  if(entry_stack.size()==1) { 
    // we are entering the top-level Cilk function; everything we did
    // before can be cleared, since we can't possibly be racing with
    // anything old at this point
    shadow_mem.clear();
  }
*/
  entry_stack.push();
  entry_stack.head()->entry_type = SPAWNER;
  entry_stack.head()->frame_type = SHADOW_FRAME;
  // entry_stack always gets pushed slightly before frame_id gets incremented
  WHEN_CILKSAN_DEBUG( entry_stack.head()->frame_id = frame_id+1; )
  enter_cilk_function();
}

void cilksan_do_enter_helper_begin() {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_enter_helper_begin\n", frame_id+1);
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = ENTER_HELPER; )

  entry_stack.push();
  entry_stack.head()->entry_type = HELPER;
  entry_stack.head()->frame_type = SHADOW_FRAME;
  // entry_stack always gets pushed slightly before frame_id gets incremented
  WHEN_CILKSAN_DEBUG( entry_stack.head()->frame_id = frame_id+1; )
  WHEN_CILKSAN_DEBUG( update_deque_for_entering_helper(); )
}

void cilksan_do_enter_end(struct __cilkrts_worker *w, uint64_t stack_ptr) {

  if(__builtin_expect(w != NULL, 0)) { worker = w; } 

  cilksan_assert(CILKSAN_INITIALIZED);
  FrameData_t *cilk_func = frame_stack.head();
  cilk_func->Sbag->get_node()->set_rsp(stack_ptr);
  cilksan_assert(last_event == ENTER_FRAME || last_event == ENTER_HELPER);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  DBG_TRACE(DEBUG_CALLBACK, "cilk_enter_end, frame stack ptr: %p\n", stack_ptr);
}

void cilksan_do_detach_begin() {

  cilksan_assert(CILKSAN_INITIALIZED);
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = DETACH; )
}

void cilksan_do_detach_end() {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "cilk_detach\n");

  cilksan_assert(entry_stack.head()->entry_type == HELPER);
  cilksan_assert(last_event == DETACH);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )

  // At this point, the frame_stack.head is still the parent (spawning) frame 
  FrameData_t *parent = frame_stack.head();
  parent->current_sync_block_size++; 

  DBG_TRACE(DEBUG_CALLBACK, 
      "frame %ld about to spawn, sb size %d with cont depth %d.\n",
      parent->Sbag->get_node()->get_func_id(),
      parent->current_sync_block_size, 
      parent->init_cont_depth + parent->current_sync_block_size); 

  if( should_steal_next_continuation() ) {
    simulate_steal();
  }

  if( !parent->Pbags[parent->Pbag_index] ) { // lazily create PBags when needed
    DBG_TRACE(DEBUG_BAGS,
        "frame %ld creates a PBag with index %d and view %lu.\n",
        parent->Sbag->get_set_node()->get_func_id(),
        parent->Pbag_index, parent->curr_view_id);
    DisjointSet_t<SPBagInterface *> *parent_pbag = 
      new DisjointSet_t<SPBagInterface *>( 
            new PBag_t(parent->Sbag->get_node(), parent->curr_view_id) );
    dset_nodes.push_back( parent_pbag );
    parent->Pbags[parent->Pbag_index] = parent_pbag;
  }
  enter_spawn_child();
}

void cilksan_do_sync_begin() {
  
  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_sync_begin\n", 
            frame_stack.head()->Sbag->get_node()->get_func_id());
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = CILK_SYNC; )
}

void cilksan_do_sync_end() {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "cilk_sync_end\n");
  cilksan_assert(last_event == CILK_SYNC);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  complete_sync();
}

void cilksan_do_leave_begin() {

  cilksan_assert(CILKSAN_INITIALIZED);
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = LEAVE_FRAME_OR_HELPER; )
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_leave_begin\n", 
            entry_stack.head()->frame_id);

  if(entry_stack.head()->entry_type == HELPER) {
    DBG_TRACE(DEBUG_CALLBACK, "cilk_leave_helper_begin\n");
    return_from_spawn();
  } else {
    DBG_TRACE(DEBUG_CALLBACK, "cilk_leave_frame_begin\n");
    leave_cilk_function();
  }
}

void cilksan_do_leave_end() {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_leave_end\n", 
            entry_stack.head()->frame_id);
  cilksan_assert(last_event == LEAVE_FRAME_OR_HELPER);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  cilksan_assert(entry_stack.size() > 1);

  if(entry_stack.head()->entry_type == HELPER) {
    WHEN_CILKSAN_DEBUG( update_deque_for_leaving_spawn_helper(); )

    // we are about to leave a spawn helper, i.e., we've reached end of a 
    // spawn statement in a Cilk function.  Update PBag index and view_id.
    // we do this in leave_end instead of leave_begin, because at 
    // leave_begin, the runtime has not had the chance to do reductions yet, 
    // and we may overflow the PBag_index (>= MAX_NUM_STEALS) if we had done 
    // this operation in leave_begin. 
    update_reducer_view();
  } else {
    WHEN_CILKSAN_DEBUG( update_deque_for_leaving_cilk_function(); )
  }

  // we delay the pop of the entry_stack much later than frame_stack (in 
  // leave_end instead of leave_begin), because we need to know whether the
  // exiting frame is a helper or a spawner at this point.  Also NOTE, for
  // parallel reduce, when the reduce is called, the shadow frame 
  // for the helper is still on the runtime deque to simulate that the 
  // reduce being called by the helper function (see comments for function   
  // restore_frame_for_spawn_return_reduction in runtime/scheduler.c.)
  entry_stack.pop();
}

// This is the case when __cilkrts_leave_frame never returns; instead we go
// back to the runtime, and the runtime is issuing the matching leave_end.
// So we need to do the same operation as in cilk_leave_end_callback.
void __cilksan_do_leave_stolen_callback() {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_leave_stolen\n", 
            entry_stack.head()->frame_id);
  cilksan_assert(last_event == LEAVE_FRAME_OR_HELPER);
  // This callback should only be invoked when we are leaving a HELPER frame
  cilksan_assert(entry_stack.size() > 1 && 
                 entry_stack.head()->entry_type == HELPER);
  WHEN_CILKSAN_DEBUG( update_deque_for_leaving_spawn_helper(); )
  
  // Update the PBag_index and view_id
  update_reducer_view();
  entry_stack.pop();

  // we are going back to runtime loop next
  // keep the checking off, until we resume user code
  WHEN_CILKSAN_DEBUG( last_event = RUNTIME_LOOP; ) 
}

// This (unsuccessful sync) should never happen during serial execution, 
// even under the simulated steals.
//
// static inline void cilk_sync_abandon_callback() {
//
//   DBG_TRACE(DEBUG_CALLBACK, "cilk_sync_abandon\n");
//   cilksan_assert(0);
// }
//
// This should never happen during serial execution, even under the simulated
// steals.  (Grep for "cilk_continue" in runtime code.) 
// static inline void cilk_continue_callback() {
// 
//   DBG_TRACE(DEBUG_CALLBACK, "cilk_continue\n");
//   cilksan_assert(0);
// }


// called by record_memory_read/write, with the access broken down 
// into 8-byte aligned memory accesses
static void 
record_mem_helper(bool is_read, uint64_t inst_addr, uint64_t addr,
                  uint32_t mem_size, bool on_stack) {

  FrameData_t *f = frame_stack.head();
  ShadowMemIter_t val = shadow_mem.find( ADDR_TO_KEY(addr) );

  if( val == shadow_mem.end() ) {
    // not in shadow memory; create a new MemAccessList_t and insert
    MemAccess_t *acc = new MemAccess_t(f->Sbag, inst_addr);
    MemAccessList_t *mem_list = 
        new MemAccessList_t(addr, is_read, acc, mem_size);
    std::pair<uint64_t, MemAccessList_t *> new_pair(ADDR_TO_KEY(addr), mem_list); 
    shadow_mem.insert(new_pair);
  } else {
    // else check for race and update the existing MemAccessList_t 
    MemAccessList_t *mem_list = val->second;
    cilksan_assert(mem_list != NULL);
    cilksan_assert(f->Pbag_index >= 0 && f->Pbag_index < MAX_NUM_STEALS);
    DisjointSet_t<SPBagInterface *> *top_pbag = f->Pbags[f->Pbag_index];
    WHEN_CILKSAN_DEBUG( 
      mem_list->check_invariants(f->Sbag->get_node()->get_func_id()); )

    mem_list->check_races_and_update(is_read, inst_addr, addr, mem_size, 
                                     on_stack, *(context_stack.head()), 
                                     f->Sbag, top_pbag, f->curr_view_id);
  }
}

// XXX: We can only read 1,2,4,8,16 bytes; optimize later
void cilksan_do_read(uint64_t inst_addr, uint64_t addr, size_t mem_size) {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_MEMORY, "record read of %lu bytes at addr %p and rip %p.\n", 
            mem_size, addr, inst_addr);

  // for now we assume the stack doesn't change
  bool on_stack = is_on_stack(addr); 
  // handle the prefix
  uint64_t next_addr = ALIGN_BY_NEXT_MAX_GRAIN_SIZE(addr); 
  size_t prefix_size = next_addr - addr;
  cilksan_assert(prefix_size >= 0 && prefix_size < MAX_GRAIN_SIZE);

  if(prefix_size >= mem_size) { // access falls within a max grain sized block
    record_mem_helper(true, inst_addr, addr, mem_size, on_stack);
  } else { 
    cilksan_assert( prefix_size <= mem_size );
    if(prefix_size) { // do the prefix first
      record_mem_helper(true, inst_addr, addr, prefix_size, on_stack);
      mem_size -= prefix_size;
    }
    addr = next_addr;
    // then do the rest of the max-grain size aligned blocks
    uint32_t i=0;
    for(i=0; (i+MAX_GRAIN_SIZE) < mem_size; i += MAX_GRAIN_SIZE) {
      record_mem_helper(true, inst_addr, addr + i, MAX_GRAIN_SIZE, on_stack);
    }
    // trailing bytes
    record_mem_helper(true, inst_addr, addr+i, mem_size-i, on_stack);
  }
}

// XXX: We can only read 1,2,4,8,16 bytes; optimize later
void cilksan_do_write(uint64_t inst_addr, uint64_t addr, size_t mem_size) {

  cilksan_assert(CILKSAN_INITIALIZED);
  DBG_TRACE(DEBUG_MEMORY, "record write of %lu bytes at addr %p and rip %p.\n", 
            mem_size, addr, inst_addr);

  bool on_stack = is_on_stack(addr); 
  // handle the prefix
  uint64_t next_addr = ALIGN_BY_NEXT_MAX_GRAIN_SIZE(addr); 
  size_t prefix_size = next_addr - addr;
  cilksan_assert(prefix_size >= 0 && prefix_size < MAX_GRAIN_SIZE);

  if(prefix_size >= mem_size) { // access falls within a max grain sized block
    record_mem_helper(false, inst_addr, addr, mem_size, on_stack);
  } else {
    cilksan_assert( prefix_size <= mem_size );
    if(prefix_size) { // do the prefix first
      record_mem_helper(false, inst_addr, addr, prefix_size, on_stack);
      mem_size -= prefix_size;
    }
    addr = next_addr;
    // then do the rest of the max-grain size aligned blocks
    uint32_t i=0;
    for(i=0; (i+MAX_GRAIN_SIZE) < mem_size; i += MAX_GRAIN_SIZE) {
      record_mem_helper(false, inst_addr, addr + i, MAX_GRAIN_SIZE, on_stack);
    }
    // trailing bytes
    record_mem_helper(false, inst_addr, addr+i, mem_size-i, on_stack);
  }
}

// clear the memory block at [start-end) (end is exclusive).
void cilksan_clear_shadow_memory(size_t start, size_t end) {

  DBG_TRACE(DEBUG_MEMORY, "Clear shadow memory %p--%p (%u).\n", 
            start, end, end-start);
  cilksan_assert(ALIGN_BY_NEXT_MAX_GRAIN_SIZE(end) == end); 

  while(start != end) {
    // DBG_TRACE(DEBUG_MEMORY, "Erasing mem %p.\n", (void *)start);
    shadow_mem.erase( ADDR_TO_KEY(start) );
    start += MAX_GRAIN_SIZE;
  }
}

static void print_cilksan_stat() {

  std::cout << "max sync block size seen: " 
            << accounted_max_sync_block_size
            << "    (from user input: " << max_sync_block_size << ", average: "
            << ( (float)accum_sync_block_size / num_of_sync_blocks ) << ")"
            << std::endl;
  std::cout << "max continuation depth seen: " 
            << accounted_max_cont_depth << std::endl;
}

void cilksan_deinit() {

  static bool deinit = false;
  // XXX: kind of a hack, but somehow this gets called twice.
  if(!deinit) { deinit = true; } 
  else { return; /* deinit-ed already */ }

  MemAccessList_t *acc_list;
  ShadowMemIter_t iter;

  print_race_report();
  print_cilksan_stat();

  cilksan_assert(frame_stack.size() == 1);
  cilksan_assert(entry_stack.size() == 1);
  cilksan_assert(context_stack.size() == 1);

  shadow_mem.clear();
  /*
  for( iter = shadow_mem.begin(); iter != shadow_mem.end(); iter++ ) {
    acc_list = iter->second;
    cilksan_assert(acc_list);
    delete acc_list;
  } */

  DisjointSet_t<SPBagInterface *> *ds_node = NULL;
  while( !dset_nodes.empty() ) {
    ds_node = dset_nodes.back();
    dset_nodes.pop_back();
    cilksan_assert(ds_node);
    cilksan_assert(ds_node->get_node());
    delete ds_node->get_node();
    delete ds_node;
  }
  cilksan_assert(DisjointSet_t<SPBagInterface *>::debug_count == 0);
  cilksan_assert(SBag_t::debug_count == 0);
  cilksan_assert(PBag_t::debug_count == 0);

  // if(first_error != 0) exit(first_error);
}

void cilksan_init() {
  cilksan_assert(stack_high_addr != 0 && stack_low_addr != 0);
  
  // these are true upon creation of the stack
  cilksan_assert(frame_stack.size() == 1);
  cilksan_assert(entry_stack.size() == 1);
  // actually only used for debugging of reducer race detection
  WHEN_CILKSAN_DEBUG( rts_deque_begin = rts_deque_end = 1; )

  // for the main function before we enter the first Cilk context
  DisjointSet_t<SPBagInterface *> *sbag;
  sbag = new DisjointSet_t<SPBagInterface *>( new SBag_t(frame_id, NULL) );
  dset_nodes.push_back(sbag);
  cilksan_assert( sbag->get_set_node()->is_SBag() ); 

  frame_stack.head()->Sbag = sbag;
  frame_stack.head()->curr_view_id = view_id++;
  WHEN_CILKSAN_DEBUG( entry_stack.head()->frame_type = FULL_FRAME; )
  *(context_stack.head()) = USER;

#if CILKSAN_DEBUG
  CILKSAN_INITIALIZED = true;
#endif
}

extern "C" int __cilksan_error_count() {
  return get_num_races_found();
}

// This funciton parse the input supplied to the user program
// and get the params meant for cilksan (everything after "--").  
// It return the index in which it found "--" so the user program
// knows when to stop parsing inputs.
extern "C" int __cilksan_parse_input(int argc, char *argv[]) {

  int i = 0;
  uint32_t seed = 0;
  int stop = 0;

  while(i < argc) {
    if(!strncmp(argv[i], "--", strlen("--")+1)) {
      stop = i++;
      break;
    }
    i++;
  }

  while(i < argc) {
    char *arg = argv[i];
    if(!strncmp(arg, "-cr", strlen("-cr")+1)) {
      i++;
      check_reduce = true;
      continue;

    } else if(!strncmp(arg, "-update", strlen("-update")+1)) {
      i++;
      cont_depth_to_check = (uint64_t) atol(argv[i++]);
      continue; 

    } else if(!strncmp(arg, "-sb_size", strlen("-sb_size")+1)) {
      i++;
      max_sync_block_size = (uint32_t) atoi(argv[i++]);
      continue;

    } else if(!strncmp(arg, "-s", strlen("-s")+1)) {
      i++;
      seed = (uint32_t) atoi(argv[i++]);
      continue;

    } else if(!strncmp(arg, "-steal", strlen("-steal")+1)) {
      i++;
      steal_point1 = (uint32_t) atoi(argv[i++]);
      steal_point2 = (uint32_t) atoi(argv[i++]);
      steal_point3 = (uint32_t) atoi(argv[i++]);
      cilksan_assert(steal_point1 < steal_point2 
                  && steal_point2 < steal_point3);
      check_reduce = true;
      continue;

    } else {
      i++; 
      std::cout << "Unrecognized input " << arg << ", ignore and continue."
                << std::endl;
    }
  }

  std::cout << "==============================================================="
            << std::endl;
  if(cont_depth_to_check != 0) {
    check_reduce = false;
    std::cout << "This run will check updates for races with " << std::endl
              << "steals at continuation depth " << cont_depth_to_check; 
  } else if(check_reduce) {
    std::cout << "This run will check reduce functions for races " << std::endl
              << "with simulated steals ";
    if(max_sync_block_size > 1) {
      std::cout << "at randomly chosen continuation points \n"
                << "(assume block size "
                << max_sync_block_size << ")";
      if(seed) {
        std::cout << ", chosen using seed " << seed;
        srand(seed);
      } else {
        // srand(time(NULL));
      }
    } else {
      if(steal_point1 != steal_point2 && steal_point2 != steal_point3) {
        std::cout << "at steal points: " << steal_point1 << ", " 
                  << steal_point2 << ", " << steal_point3 << ".";
      } else {
        simulate_all_steals = true;
        check_reduce = false; 
        std::cout << "at every continuation point.";
      }
    }
  } else {
    // cont_depth_to_check == 0 and check_reduce = false 
    std::cout << "This run will check for races without simulated steals.";
  }
  std::cout << std::endl;
  std::cout << "==============================================================="
            << std::endl;

  cilksan_assert(!check_reduce || cont_depth_to_check == 0);
  cilksan_assert(!check_reduce || max_sync_block_size > 1 || steal_point1 != steal_point2);

  return (stop == 0 ? argc : stop);
}

// XXX: Should really be in print_addr.cpp, but this will do for now 
void print_current_function_info() {
  FrameData_t *f = frame_stack.head();
  std::cout << "steal points: " << f->steal_points[0] << ", " 
            << f->steal_points[1] << ", " << f->steal_points[2] << std::endl;
  std::cout << "curr sync block size: " << f->current_sync_block_size << std::endl;
  std::cout << "frame id: " << f->Sbag->get_node()->get_func_id() << std::endl;
}

