#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#include <execinfo.h>
#include <inttypes.h> 

#include "cilksan_internal.h"
#include "debug_util.h"
#include "disjointset.h"
#include "red_access.h"
#include "stack.h"
#include "viewread_bags.h"


#if CILKSAN_DEBUG
enum EventType_t last_event = NONE;  
#endif

// declared in driver.cpp
extern FILE *err_io;

static int error_count;


// -------------------------------------------------------------------------
//  Analysis data structures and fields
// -------------------------------------------------------------------------

#if CILKSAN_DEBUG
template<typename DISJOINTSET_DATA_T> 
long DisjointSet_t<DISJOINTSET_DATA_T>::debug_count = 0;

long SBag_t::debug_count = 0;
long PBag_t::debug_count = 0;
#endif

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

// ANGE: Each function that causes a Disjoint set to be created has a 
// unique ID (i.e., Cilk function and spawned C function).
// If a spawned function is also a Cilk function, a Disjoint Set is created
// for code between the point where detach finishes and the point the Cilk
// function calls enter_frame, which may be unnecessary in some case. 
// (But potentially necessary in the case where the base case is executed.)  
static uint64_t frame_id = 0;

// Struct for keeping track of shadow frame
typedef struct FrameData_t {
  DisjointSet_t<Bag_t *> *SSbag;
  DisjointSet_t<Bag_t *> *SPbag;
  DisjointSet_t<Bag_t *> *Pbag;

  FrameData_t() :
      SSbag(NULL), SPbag(NULL), Pbag(NULL)
  { }

  inline void init_new_function(DisjointSet_t<Bag_t *> *_SSbag, 
                                DisjointSet_t<Bag_t *> *_SPbag,
                                DisjointSet_t<Bag_t *> *_Pbag) {
    SSbag = _SSbag;
    SPbag = _SPbag;
    Pbag = _Pbag;
  }
} FrameData_t;

// Data associated with the stack of Cilk frames or spawned C frames.
// head contains the SP bags for the function we are currently processing
static Stack_t<FrameData_t> frame_stack;

// A container to keep track a set of disjointsets created that must be 
// freed at the end of the execution
static std::vector< DisjointSet_t<Bag_t *> * > dset_nodes; 

// Shadow memory, or the unordered hashmap that maps a memory address to its 
// last reader and writer
typedef std::unordered_map<uint64_t, RedAccess_t *>::iterator ShadowMemIter_t;
static std::unordered_map<uint64_t, RedAccess_t *> shadow_mem;


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

/*************************************************************************/
/**  Events functions
/*************************************************************************/
/// Helper function for handling the start of a new sync block.
static inline void start_new_sync_block() {
  frame_stack.head()->SSbag->get_node()->num_local_spawns = 0;
}

/// Helper function for merging returning child's bag into parent's 
static inline void merge_bag_from_returning_child(bool returning_from_spawn) {

  FrameData_t *parent = frame_stack.ancestor(1);
  FrameData_t *child = frame_stack.head();

  cilksan_assert(parent->SSbag);
  cilksan_assert(parent->SPbag);
  cilksan_assert(child->SSbag);
  cilksan_assert(child->SPbag);
  // should have encountered a cilk_sync before exit
  cilksan_assert(NULL == child->Pbag);
  cilksan_assert(0 == child->SSbag->get_node()->num_local_spawns);

  if(returning_from_spawn) {

    DBG_TRACE(DEBUG_BAGS, 
        "Merge bags from spawned child %ld to parent %ld.\n",
        child->SSbag->get_set_node()->get_func_id(),
        parent->SSbag->get_set_node()->get_func_id());

    DisjointSet_t<Bag_t *> *parent_pbag = parent->Pbag;
    cilksan_assert(parent_pbag && parent_pbag->get_set_node()->is_PBag());
    cilksan_assert(child->SSbag->get_set_node()->is_SBag());
    parent_pbag->combine(child->SSbag);
    cilksan_assert(child->SSbag->get_set_node()->is_PBag());
    cilksan_assert(child->SPbag->get_set_node()->is_SBag());
    parent_pbag->combine(child->SPbag);
    cilksan_assert(child->SPbag->get_set_node()->is_PBag());

  } else { // otherwise we are returning from a call
    DBG_TRACE(DEBUG_BAGS, "Merge bags from called child %ld to parent %ld.\n",
              child->SSbag->get_set_node()->get_func_id(), 
              parent->SSbag->get_set_node()->get_func_id());
    if ( 0 == parent->SSbag->get_node()->num_local_spawns ) {
      // The parent has no unsynchronized children of its own, so add
      // the node to the SS bag.
      cilksan_assert( parent->SSbag->get_set_node()->is_SSBag() ); 
      parent->SSbag->combine(child->SSbag);
      cilksan_assert( parent->SPbag->get_set_node()->is_SPBag() ); 
      parent->SPbag->combine(child->SPbag);
    } else {
      // The parent has unsynchronized children, so add the node to
      // the SP bag.
      cilksan_assert( parent->SPbag->get_set_node()->is_SPBag() ); 
      parent->SPbag->combine(child->SSbag);
      parent->SPbag->combine(child->SPbag);
    }
  }
  DBG_TRACE(DEBUG_BAGS, "After merge, parent set node func id: %ld.\n", 
            parent->SSbag->get_set_node()->get_func_id());
  cilksan_assert(parent->SSbag->get_node()->get_func_id() == 
                 parent->SSbag->get_set_node()->get_func_id());

  child->SSbag = NULL;
  child->SPbag = NULL;
}

/// Helper function for handling the start of a new function.
/// This function can be a spawned or called Cilk function or a 
/// spawned C function.  A called C function is treated as inlined.
static inline void start_new_function() {

  frame_id++;
  frame_stack.push(); 
  DBG_TRACE(DEBUG_BASIC, "Enter frame %ld.\n", frame_id);

  // get the parent pointer after we push, because once pushed, the
  // pointer may no longer be valid due to resize
  FrameData_t *parent = frame_stack.ancestor(1);
  DisjointSet_t<Bag_t *> *parent_ssbag = parent->SSbag;
  DisjointSet_t<Bag_t *> *child_ssbag, *child_spbag, *child_pbag;

  FrameData_t *child = frame_stack.head();
  cilksan_assert(NULL == child->SSbag);
  cilksan_assert(NULL == child->SPbag);

  child_ssbag = new DisjointSet_t<Bag_t *>( 
      make_SSBag(frame_id,
                 parent_ssbag->get_node()->num_ancestor_spawns
                 + parent_ssbag->get_node()->num_local_spawns,
                 parent_ssbag->get_node()) );
  child_spbag = new DisjointSet_t<Bag_t *>( 
      make_SPBag(child_ssbag->get_node()) );
  child_pbag = new DisjointSet_t<Bag_t *>( 
      make_PBag(child_ssbag->get_node()) );
  
  cilksan_assert( child_ssbag->get_set_node()->is_SSBag() ); 
  cilksan_assert( child_spbag->get_set_node()->is_SPBag() ); 
  cilksan_assert( child_spbag->get_set_node()->is_PBag() ); 
  dset_nodes.push_back(child_ssbag);
  dset_nodes.push_back(child_spbag);
  dset_nodes.push_back(child_pbag);

  child->init_new_function(child_ssbag, child_spbag, child_pbag);
  start_new_sync_block();
}

/// Helper function for exiting a function; counterpart of start_new_function.
static inline void exit_function() {
  frame_stack.pop();
}

/// Action performed on entering a Cilk function (excluding spawn helper).
static inline void enter_cilk_function() {

  DBG_TRACE(DEBUG_BASIC, "entering a Cilk function, push frame_stack\n");
  start_new_function();
}

/// Action performed on leaving a Cilk function (excluding spawn helper).
static inline void leave_cilk_function() {

  DBG_TRACE(DEBUG_BASIC, "leaving a Cilk function, pop frame_stack\n");

  /* param: not returning from a spawn */
  merge_bag_from_returning_child(0);
  exit_function();
}

/// Action performed on entering a spawned child.
/// (That is, right after detach.)
static inline void enter_spawn_child() {

  DBG_TRACE(DEBUG_BASIC, "done detach, push frame_stack\n");
  // Increment the number of local spawns, so child can know that it
  // has additional peeers from this function.
  start_new_function();
}

/// Action performed when returning from a spawned child.
/// (That is, returning from a spawn helper.)
static inline void return_from_spawn() {

  DBG_TRACE(DEBUG_BASIC, "return from spawn helper, pop frame_stack\n");

  /* param: we are returning from a spawn */
  merge_bag_from_returning_child(1);
  exit_function();
} 

/// Action performed immediately after passing a sync.
static void complete_sync() {

  FrameData_t *f = frame_stack.head();
  DBG_TRACE(DEBUG_BASIC, "frame %d done sync\n", 
            f->SSbag->get_node()->get_func_id());
  
  cilksan_assert( f->Pbag->get_set_node()->isPBag() );
  f->SPbag->combine( f->Pbag );
  cilksan_assert( f->Pbag->get_set_node()->isSPBag() );

  start_new_sync_block();
}


//---------------------------------------------------------------
// Callback functions
//---------------------------------------------------------------
void cilksan_do_enter_begin() {
  cilksan_assert(NONE == last_event);
  WHEN_CILKSAN_DEBUG( last_event = ENTER_FRAME; )
  DBG_TRACE(DEBUG_BASIC, "frame %ld cilk_enter_frame_begin\n", frame_id+1);

  entry_stack.push();
  entry_stack.head()->entry_type = SPAWNER;
  entry_stack.head()->frame_type = SHADOW_FRAME;
  // entry_stack always gets pushed slightly before frame_id gets incremented
  WHEN_CILKSAN_DEBUG( entry_stack.head()->frame_id = frame_id+1; )
  enter_cilk_function();
}

void cilksan_do_enter_helper_begin() {

  DBG_TRACE(DEBUG_BASIC, "frame %ld cilk_enter_helper_begin\n", frame_id+1);
  cilksan_assert(NONE == last_event);
  WHEN_CILKSAN_DEBUG( last_event = ENTER_HELPER; )

  entry_stack.push();
  entry_stack.head()->entry_type = HELPER;
  entry_stack.head()->frame_type = SHADOW_FRAME;

  // WHEN_CILKSAN_DEBUG( update_deque_for_entering_helper(); )
}

void cilksan_do_enter_end(uint64_t stack_ptr) {

  FrameData_t *cilk_func = frame_stack.head();
  cilk_func->Sbag->get_node()->set_rsp(stack_ptr);
  cilksan_assert(ENTER_FRAME == last_event || ENTER_HELPER == last_event);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  DBG_TRACE(DEBUG_BASIC, "cilk_enter_end, frame stack ptr: %p\n", stack_ptr);
}

void cilksan_do_detach_begin() {
  cilksan_assert(NONE == last_event);
  WHEN_CILKSAN_DEBUG( last_event = DETACH; )
}

void cilksan_do_detach_end() {

  DBG_TRACE(DEBUG_BASIC, "cilk_detach\n");

  cilksan_assert(HELPER == entry_stack.head()->entry_type);
  cilksan_assert(DETACH == last_event);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )

  // At this point, the frame_stack.head is still the parent (spawning) frame 
  cilksan_assert(parent->Pbag);

  enter_spawn_child();

  frame_stack.ancestor(1)->SSbag->get_node()->num_local_spawns++;
}

void cilksan_do_sync_begin() {
  
  DBG_TRACE(DEBUG_BASIC, "frame %ld cilk_sync_begin\n", 
            frame_stack.head()->Sbag->get_node()->get_func_id());
  cilksan_assert(NONE == last_event);
  WHEN_CILKSAN_DEBUG( last_event = CILK_SYNC; )
}

void cilksan_do_sync_end() {
  DBG_TRACE(DEBUG_BASIC, "cilk_sync_end\n");
  cilksan_assert(CILK_SYNC == last_event);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  complete_sync();
}

void cilksan_do_leave_begin() {

  cilksan_assert(NONE == last_event);
  WHEN_CILKSAN_DEBUG( last_event = LEAVE_FRAME_OR_HELPER; )
  DBG_TRACE(DEBUG_BASIC, "frame %ld cilk_leave_begin\n", 
            entry_stack.head()->frame_id);

  if(HELPER == entry_stack.head()->entry_type) {
    DBG_TRACE(DEBUG_BASIC, "cilk_leave_helper_begin\n");
    return_from_spawn();
  } else {
    DBG_TRACE(DEBUG_BASIC, "cilk_leave_frame_begin\n");
    leave_cilk_function();
  }
}

void cilksan_do_leave_end() {

  DBG_TRACE(DEBUG_BASIC, "frame %ld cilk_leave_end\n", 
            entry_stack.head()->frame_id);
  cilksan_assert(LEAVE_FRAME_OR_HELPER == last_event);
  WHEN_CILKSAN_DEBUG( last_event = NONE; )
  cilksan_assert(entry_stack.size() > 1);

  // XXX: Put it back later
#if 0
  if(HELPER == entry_stack.head()->entry_type) {
    WHEN_CILKSAN_DEBUG( update_deque_for_leaving_spawn_helper(); )

    // we are about to leave a spawn helper, i.e., we've reached end of a 
    // spawn statement in a Cilk function.  Update PBag index and view_id.
    // we do this in leave_end instead of leave_begin, because at 
    // leave_begin, the runtime has not had the chance to do reductions yet, 
    // and we may overflow the PBag_index (>= MAX_NUM_STEALS) if we had done 
    // this operation in leave_begin. 
    // update_reducer_view();
  } else {
    WHEN_CILKSAN_DEBUG( update_deque_for_leaving_cilk_function(); )
  }
#endif

  // we delay the pop of the entry_stack much later than frame_stack (in 
  // leave_end instead of leave_begin), because we need to know whether the
  // exiting frame is a helper or a spawner at this point.  Also NOTE, for
  // parallel reduce, when the reduce is called, the shadow frame 
  // for the helper is still on the runtime deque to simulate that the 
  // reduce being called by the helper function (see comments for function   
  // restore_frame_for_spawn_return_reduction in runtime/scheduler.c.)
  entry_stack.pop();
}

// // This is the case when __cilkrts_leave_frame never returns; instead we go
// // back to the runtime, and the runtime is issuing the matching leave_end.
// // So we need to do the same operation as in cilk_leave_end_callback.
// void cilksan_do_leave_stolen_callback() {
//   DBG_TRACE(DEBUG_CALLBACK, "frame %ld cilk_leave_stolen\n", 
//             entry_stack.head()->frame_id);
//   cilksan_assert(LEAVE_FRAME_OR_HELPER == last_event);
//   // This callback should only be invoked when we are leaving a HELPER frame
//   cilksan_assert(entry_stack.size() > 1 && 
//                  HELPER == entry_stack.head()->entry_type == HELPER);
//   // WHEN_CILKSAN_DEBUG( update_deque_for_leaving_spawn_helper(); )
  
//   // Update the PBag_index and view_id
//   // XXX: Put it back later
//   // update_reducer_view();
//   entry_stack.pop();

//   // we are going back to runtime loop next
//   // keep the checking off, until we resume user code
//   WHEN_CILKSAN_DEBUG( last_event = RUNTIME_LOOP; ) 
// }

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

void
set_reducer(uint64_t addr, uint64_t rip) {
  FrameData_t *f = frame_stack.head();
  ShadowMemIter_t val = shadow_mem.find( ADDR_TO_KEY(addr) );

  RedAccess_t *access = new RedAccess_t(f->SSbag, rip);
  std::pair<uint64_t, RedAccess_t *> access_pair(ADDR_TO_KEY(addr), access);

  if( val == shadow_mem.end() ) {
    // not in shadow memory; create a new RedAccess_t and insert
    shadow_mem.insert(access_pair);
  } else {
    *val = access_pair;
  }
}


void
get_reducer(uint64_t addr, uint64_t rip) {
  FrameData_t *f = frame_stack.head();
  ShadowMemIter_t val = shadow_mem.find( ADDR_TO_KEY(addr) );

  if ( val == shadow_mem.end() ) {
    debug_printf(DEBUG_BASIC,
                 "Reducer %lu read at %p without being set.  This should never happen.\n",
                 addr, rip);
  } else {
    Bag_t *lca = val->second->func->get_set_node();
    if (!lca->is_SSBag()) {
      // There is a race
      report_viewread_race(val->second->rip, rip, addr);
    } else {
      Bag_t *current_ssbag = f->SSbag;
      if (current_ssbag->num_local_spawns != 0 ||
          current_ssbag->num_ancestor_spawns !=
          lca->num_ancestor_spawns + lca->num_local_spawns) {
        // There is a race
        report_viewread_race(val->second->rip, rip, addr);
      }
    }
  }
}


// Defined in print_addr.cpp
extern void print_addr(FILE *f, void *a);
extern void print_race_report();
extern int get_num_races_found(); 

void cilksan_deinit() {
  RedAccess_t *access;
  ShadowMemIter_t iter;

  print_race_report();
  print_cilksan_stat();

  cilksan_assert(frame_stack.size() == 1);
  cilksan_assert(entry_stack.size() == 1);
  cilksan_assert(context_stack.size() == 1);

  for( iter = shadow_mem.begin(); iter != shadow_mem.end(); iter++ ) {
    access = iter->second;
    cilksan_assert(access);
    delete access;
  }

  DisjointSet_t<SPBagInterface *> *ds_node = NULL;
  while( !dset_nodes.empty() ) {
    ds_node = dset_nodes.back();
    dset_nodes.pop_back();
    cilksan_assert(ds_node);
    cilksan_assert(ds_node->get_node());
    delete ds_node->get_node();
    delete ds_node;
  }
  cilksan_assert(DisjointSet_t<Bag_t *>::debug_count == 0);
  cilksan_assert(SBag_t::debug_count == 0);
  cilksan_assert(PBag_t::debug_count == 0);

  // if(first_error != 0) exit(first_error);
}

void cilksan_init() {
  cilksan_assert(stack_high_addr != 0 && stack_low_addr != 0);
  // XXX Need to figure out how to pass args from user code to tool
  // parse_input(); 
  
  // these are true upon creation of the stack
  cilksan_assert(frame_stack.size() == 1);
  cilksan_assert(entry_stack.size() == 1);
  // XXX actually only used for debugging of reducer race detection
  WHEN_CILKSAN_DEBUG( rts_deque_begin = rts_deque_end = 1; )

  // for the main function before we enter the first Cilk context
  DisjointSet_t<Bag_t *> *ssbag, *spbag, *pbag;
  ssbag = new DisjointSet_t<Bag_t *>( make_SSBag(frame_id, 0, NULL) );
  spbag = new DisjointSet_t<Bag_t *>( make_SPBag(ssbag) );
  pbag = new DisjointSet_t<Bag_t *>( make_PBag(ssbag) );

  dset_nodes.push_back(ssbag);
  dset_nodes.push_back(spbag);
  dset_nodes.push_back(pbag);
  cilksan_assert( ssbag->get_set_node()->is_SSBag() ); 
  cilksan_assert( spbag->get_set_node()->is_SPBag() ); 
  cilksan_assert( pbag->get_set_node()->is_PBag() ); 

  frame_stack.head()->SSbag = ssbag;
  frame_stack.head()->SPbag = spbag;
  frame_stack.head()->Pbag = pbag;
  WHEN_CILKSAN_DEBUG( entry_stack.head()->frame_type = FULL_FRAME; )
  *(context_stack.head()) = USER;
}

// extern "C" int __cilksan_error_count() {
//   return get_num_races_found();
// }

/* XXX: Put it back later
static void parse_input() {

  check_reduce = KnobCheckReduce.Value();
  cont_depth_to_check = KnobContDepth.Value();
  max_sync_block_size = KnobSBSize.Value();

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
      std::cout << "at randomly chosen continuation points.";
    } else {
      simulate_all_steals = true;
      check_reduce = false; 
      std::cout << "at every continuation point.";
    }
  } else {
    // cont_depth_to_check == 0 and check_reduce = false 
    std::cout << "This run will check for races without simulated steals.";
  }
  std::cout << std::endl;
  std::cout << "==============================================================="
            << std::endl;

  racedetector_assert(!check_reduce || cont_depth_to_check == 0);
  racedetector_assert(!check_reduce || max_sync_block_size > 1);
}
*/

