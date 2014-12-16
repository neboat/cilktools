/*************************************************************************/
/** This file is included in cilksan.cpp; contains all reducer related stuff 
/*************************************************************************/

#include <internal/abi.h>

/*************************************************************************/
/**  Helper functions for Events
/*************************************************************************/
static inline void swap(uint32_t *x, uint32_t *y) {
  uint32_t tmp = *x;
  *x = *y;
  *y = tmp;
}

// max_sync_block_size == the max number of continuations within a sync block 
// randomly choose 3 numbers, i1, i2 and i3 between [0-max_sb] to indicate the
// intervals that we want to check reduce op on.  
// we will be checking [i1, i2-1] op [i2, i3-1] (inclusive both ends)
// to check it, it boils down to simulating steal at continuation points where
// current_sync_block_size == i1, i2, and i3.
static void randomize_steal_points(FrameData_t *f) {

  DBG_TRACE(DEBUG_REDUCER, "Randomize steals for frame %ld: ",
                     f->Sbag->get_node()->get_func_id());
  if(max_sync_block_size < 2) { // special case
    f->steal_points[0] = NOP_STEAL; // NOP steal points will be ignored 
    f->steal_points[1] = NOP_STEAL;  
    f->steal_points[2] = (rand() & 0x1); // the cont may or may not be stolen
    f->steal_index = 2;
    // we are skipping the first interval, so keep the Pbag index in sync
    f->Pbag_index = 1; 
    DBG_TRACE(DEBUG_REDUCER, "0, 0, %d.\n", f->steal_points[2]);
    return;
  }

  uint32_t *arr = f->steal_points; // just for convenience 
  // +1 for picking a value between [0, max_sb] and 
  // +1 to allow last interval to be picked
  uint32_t intervals = max_sync_block_size + 2;

  arr[0] = rand() % intervals; 
  // need to pick unique numbers
  do { arr[1] = rand() % intervals; } while(arr[0] == arr[1]);
  if(arr[0] > arr[1]) { swap(&arr[0], &arr[1]); } // keep sorted 
  do { arr[2] = rand() % intervals; } while(arr[0] == arr[2] 
                                         || arr[1] == arr[2]);
  if(arr[1] > arr[2]) { // keep sorted 
    swap(&arr[1], &arr[2]);
    if(arr[0] > arr[1]) { swap(&arr[0], &arr[1]); }
  } 
}

static void set_steal_points(FrameData_t *f) {

  f->steal_index = 0;
  f->Pbag_index = 0;
  if(steal_point1 != steal_point2) { // use user-specified steal points
    f->steal_points[0] = steal_point1;
    f->steal_points[1] = steal_point2;
    f->steal_points[2] = steal_point3;
  } else {
    randomize_steal_points(f);
  }

  DBG_TRACE(DEBUG_REDUCER, "%d, %d, %d.\n", f->steal_points[0],
                     f->steal_points[1], f->steal_points[2]);
  // assert that the steal points are unique and sorted in increasing order
  cilksan_assert(f->steal_points[0] < f->steal_points[1] 
              && f->steal_points[1] < f->steal_points[2]); 

  // we are skipping the first interval, so keep the Pbag index in sync
  if(f->steal_points[0] == NOP_STEAL) {
    f->Pbag_index = 1;
  }
  // skip over Nop steals
  while(f->steal_index < MAX_NUM_STEALS && 
        f->steal_points[f->steal_index] < f->current_sync_block_size) {
    f->steal_index++;
  }
}

// return true if the next continuation within the spwaning funciton
// ought to be stolen to check for reducer races; otherwise, return false.
static bool should_steal_next_continuation() {
  FrameData_t *f = frame_stack.head();
  bool should_steal = false;

  if( cont_depth_to_check ) {
    cilksan_assert(!check_reduce);
    uint64_t curr_cont_depth = f->init_cont_depth + f->current_sync_block_size; 
    if( curr_cont_depth == cont_depth_to_check ) {
      DBG_TRACE(DEBUG_REDUCER, 
              "Should steal frame %ld, sb size %d, with cont depth %d.\n",
              f->Sbag->get_node()->get_func_id(), f->current_sync_block_size,
              curr_cont_depth);
      should_steal = true;
    }

  } else if(check_reduce) {
    if(f->current_sync_block_size == 1) {
      // about to enter first spawn child; now we need those steal points
      set_steal_points(f);
    }
    
    // check if the next steal point matches
    if(f->steal_index < MAX_NUM_STEALS &&
       f->steal_points[f->steal_index] == f->current_sync_block_size) {
      f->steal_index++;
      DBG_TRACE(DEBUG_REDUCER, 
        "Should steal frame %ld, cont %d.\n",
        f->Sbag->get_node()->get_func_id(), f->current_sync_block_size);
      should_steal = true;
    }
    cilksan_assert(f->steal_index == MAX_NUM_STEALS ||
       f->steal_points[f->steal_index] > f->current_sync_block_size);

  } else if(simulate_all_steals) {
    should_steal = true;
  }

  return should_steal;
}

// return true if function f's next continuation was stolen
static inline bool next_continuation_was_stolen(FrameData_t *f) {
  
  bool ret = false;

  if(cont_depth_to_check &&
     cont_depth_to_check == (f->init_cont_depth + f->current_sync_block_size)) {
    ret = true;

  } else if(check_reduce) {
    if(f->steal_index == 0) {
      cilksan_assert( 
          f->steal_points[f->steal_index] > f->current_sync_block_size); 
      ret = false;
    } else if(f->steal_points[f->steal_index-1] == f->current_sync_block_size) {
      cilksan_assert( (f->steal_index == MAX_NUM_STEALS || 
          f->steal_points[f->steal_index] > f->current_sync_block_size) &&
          f->steal_points[f->steal_index-1] <= f->current_sync_block_size ); 
      ret = true;
    }

  } else if(simulate_all_steals) {
    ret = true;
  }

  return ret;
}

// This function gets called when the runtime is performing:
//
// A. the return protocol for a spawned child whose parent (call it f) has 
// been stolen; and 
// B. a non-trivial cilk_sync
//
// Call the steal points i1, i2, and i3, as chosen by
// randomize_steal_points() function (and stored in f->steal_points[]).
// interval 1 will be [i1, i2-1] (inclusive both ends), and 
// interval 2 will be [i2, i3-1] (inclusive both ends).
// interval 0 will be what comes before i1 and interval 3 will be what 
// comes after i3; these two intervals could be empty.
//
// In case A., at the point of invocation, we have seen leave_frame_begin 
// but not leave_frame_end for the spawn helper (and we won't, because 
// __cilkrts_leave_frame won't return in this case), so the helper has been 
// popped off frame_stack.  Function f (the spawner)'s current_sync_block_size 
// is the index of the continuation strand after the spawn statement (assume 
// 0 based indexing).  
//
// In case B., at the point of invocation, we have seen cilk_sync_begin
// but not cilk_sync_end, and the spawner frame has not reset its
// current_sync_block_size to 0 yet (that happens in cilk_sync_end). 
//
// This function returns (current interval << 1) & end_of_interval
static unsigned int get_current_reduce_interval(int spawn_ret) {
  // should be invoked only when we are simulating steals
  cilksan_assert(cont_depth_to_check || check_reduce || 
                      simulate_all_steals);

#define INTERVAL_MASK(interval, end) (((interval) << 1) | end)
#define MASK_TO_INTERVAL(mask) ((mask) >> 1)
#define MASK_TO_INTERVAL_END(mask) ((mask) & 0x1)
  unsigned int ret = 0;
  // f is the function that contains the spwan / sync statement
  FrameData_t *f = frame_stack.head();
  uint32_t sb_size = f->current_sync_block_size;
  DBG_TRACE(DEBUG_REDUCER, 
    "Get interval for frame %ld, spawn return? %d",
    f->Sbag->get_node()->get_func_id(), spawn_ret);
  DBG_TRACE(DEBUG_REDUCER, 
    ", sb size %d, steal index %d, pindex: %u.\n",
    sb_size, f->steal_index, f->Pbag_index);

  if(cont_depth_to_check) {
    // we are simulating steals to check updates, so there should be only a
    // single steal point per sync block.  Let's call the interval before the
    // steal point as 0 and after as 1, and performs the reduction at sync. 
    uint64_t curr_cont_depth = f->init_cont_depth + sb_size; 
    if(curr_cont_depth == cont_depth_to_check) {
      ret = INTERVAL_MASK(0, 0x1); // end of interval 0
    } else if(curr_cont_depth > cont_depth_to_check) {
      ret = INTERVAL_MASK(1, 0x0); // middle of interval 1 (not ended yet)
    }
  
  } else if(check_reduce) { // we are simulating steals to check reduce ops
    // we are in interval 0 if we haven't passed any steal points
    if(f->steal_index > 0) {
      // special case: we are returning from a spawn where the next
      // continuation is a steal point that has already been processed
      if(f->steal_points[f->steal_index - 1] == sb_size) {
        ret = INTERVAL_MASK( f->steal_index-1, 0x1 );
      } else {
        // otherwise, we are within an interval indicated by the steal index`
        ret = INTERVAL_MASK( f->steal_index, 0x0 );
      }
    }

  } else if(simulate_all_steals) {
    // we are simulating all steals and performing eager reduce ops, so treat
    // the first spawn child returning as end of interval 1, and the rest as
    // end of interval 2; this way, we will always perform one single reduce
    // op everything this function is invoked.
    if(f->current_sync_block_size == 1) {
      ret = INTERVAL_MASK(0, 0x1); // end of interval 0
    } else {
      // special encoding: ene of interval 3; usually we never reach the end
      // of interval 3, but simulate_all_steals is encoded specially
      ret = INTERVAL_MASK(3, 0x1); 
    }
  }

  DBG_TRACE(DEBUG_REDUCER, 
      "Check interval returns interval %u, end: %u.\n", 
      MASK_TO_INTERVAL(ret), MASK_TO_INTERVAL_END(ret));
  // the PBag index should be the same as the number of intervals at this point
  // or, PBag index == 1 and we are at the end of first interval
#if CILKSAN_DEBUG
  // The PBag index should be the same 
  if(spawn_ret) {
    // at spawn return, the PBag index should be the same as the interval
    // index, or PBag index == 1 if we are in interval 3. 
    cilksan_assert( f->Pbag_index == MASK_TO_INTERVAL(ret) || 
        (f->Pbag_index == 1 && MASK_TO_INTERVAL(ret) == MAX_NUM_STEALS) );
  } else {
    // at sync, the PBag index should be the same as the interval
    // index, or PBag index == 1 if we are in end of interval 2 or 3. 
    cilksan_assert( f->Pbag_index == 
                         (MASK_TO_INTERVAL(ret) + MASK_TO_INTERVAL_END(ret)) || 
        (f->Pbag_index == 1 && 
         MASK_TO_INTERVAL(ret) + MASK_TO_INTERVAL_END(ret) >= MAX_NUM_STEALS) );
  }
#endif

  return ret;
}

// This is invoked right before the runtime performs a merge, merging the
// right-most reducer map into the reducer map to its left.  The tool
// correspoindingly update the disjointset data structure to reflect that ---
// any memory access performed during the merge operation is now logically
// in-series with the memory accesses stored in the top two PBags.
//
// This is called when either the runtime performs the return protocol 
// for a stolen spawned child or when the runtime performs cilk_sync.
static void update_disjointsets() {

  // f is the function that contains the spwan / sync statement
  FrameData_t *f = frame_stack.head();
  DBG_TRACE(DEBUG_REDUCER, 
      "frame %ld update disjoint set, merging PBags in index %d and %d, curr vid: %d.\n",
      f->Sbag->get_node()->get_func_id(), f->Pbag_index-1, f->Pbag_index,
      f->curr_view_id);

  cilksan_assert(f->Pbag_index > 0 && f->Pbag_index < MAX_NUM_STEALS);
  // pop the top-most Pbag
  DisjointSet_t<SPBagInterface *> *top_pbag = f->Pbags[f->Pbag_index];
  f->Pbags[f->Pbag_index] = NULL;
  f->Pbag_index--;
  if(top_pbag) {
    DisjointSet_t<SPBagInterface *> *next_pbag = f->Pbags[f->Pbag_index];
    if(next_pbag) { // merge with the next Pbag if there is one
      cilksan_assert( top_pbag->get_set_node()->is_PBag() );
      next_pbag->combine(top_pbag);
      cilksan_assert( next_pbag->get_set_node()->is_PBag() );
      cilksan_assert( next_pbag->get_node()->get_func_id() == 
                           next_pbag->get_set_node()->get_func_id() );
    } else {
      f->Pbags[f->Pbag_index] = top_pbag;
    }
  }
}

// Check that the entry_stack indeed mirrors the worker's runtime deque
#if CILKSAN_DEBUG
static void check_deque_invariants() {

  DBG_TRACE(DEBUG_DEQUE, 
    "%ld: check deque invariants, range: [%d, %d), entry stack size: %u.\n", 
    frame_stack.head()->Sbag->get_node()->get_func_id(), 
    rts_deque_begin, rts_deque_end, entry_stack.size());

  // when this function is invoked, we must be inside a helper function
  cilksan_assert(entry_stack.ancestor(0)->entry_type == HELPER);
  cilksan_assert(rts_deque_begin <= rts_deque_end);
  if(rts_deque_begin == rts_deque_end) { // nothing to check except levels
    cilksan_assert( worker->head == worker->tail );
    return;
  }

  // check that everything above the first frame in the deque is FULL
  for(uint32_t i=1; i < rts_deque_begin; i++) {
    cilksan_assert(entry_stack.at(i)->frame_type == FULL_FRAME); 
  }

  // Check deque content: rts_deque_end must be pointing to a HELPER frame
  cilksan_assert( entry_stack.size() > rts_deque_end && 
                       entry_stack.at(rts_deque_end)->entry_type == HELPER);

  uint32_t levels = 0;
  // The frame type at the top of the deque could be anything.
  if(entry_stack.at(rts_deque_begin)->entry_type != HELPER) {
    Entry_t *f = entry_stack.at(rts_deque_begin);
    cilksan_assert( rts_deque_begin == 1 || f->frame_type == FULL_FRAME ); 
    if(f->frame_type == FULL_FRAME) {
      // Angelina: not true; if a Cilk function calls another Cilk function,
      // the head will be pointing to the callee; only the oldest Cilk
      // function at the beginning of the stacklet would be a FULL frame
      // cilksan_assert((*worker->head)->flags & CILK_FRAME_STOLEN);
      cilksan_assert(f->entry_type != HELPER);
    }
    levels++; // if it's not a HELPER frame, it counds as its own stacklet 
  }
  for(uint32_t i=rts_deque_begin; i < rts_deque_end; i++) {
    Entry_t *f = entry_stack.at(i);
    cilksan_assert(i==rts_deque_begin || f->frame_type == SHADOW_FRAME);
    // each HELPER frame forms a new stacklet
    if(f->entry_type == HELPER) { levels++; }
  }
  // check that the levels we counted in entry_stack matches that of the runtime
  cilksan_assert(levels == (worker->tail - worker->head));

  // check that everything not on deque is shadow and not marked as LAZY
  for(uint32_t i=rts_deque_end; i < entry_stack.size(); i++) {
    cilksan_assert(entry_stack.at(i)->frame_type == SHADOW_FRAME); 
  }

  /*
   * We can't actually check if the number of shadow frames match; in the
   * cilk-for implementation, the tool sees an "enter_frame," but the shadow
   * frame is not actually pushed onto the runtime deque if that particular
   * recursive call reaches the base case (i.e., invoking loop body).
  // check the number of frame in entry_stack matches that of the runtime deque
  uint32_t num_frames = 0;
  // tail-1 is valid only if the deque is not empty
  __cilkrts_stack_frame *sf = *(worker->tail-1);
  while( sf && (sf->flags & CILK_FRAME_STOLEN) == 0 ) { 
    // count number of frames in the deque, including the top FULL frame
    num_frames++; 
    sf = sf->call_parent;
  } 

  // the last FULL frame may or may not be on the deque; can't tell from 
  // runtime deque, because its child has not been unlinked from it. 
  // Even if it's stolen, let's just count it if entry_stack says it's on 
  // the deque (there can be at most one such FULL frame on the deque)
  if(sf && entry_stack.at(rts_deque_begin)->frame_type == FULL_FRAME) {
    num_frames++;
  }*/

  DBG_TRACE(DEBUG_DEQUE, 
    "Done checking deque invariants, levels: %d.\n", levels);
}

static void update_deque_for_simulating_steals() {
  // promote everything except for the last stacklet; 
  for(uint32_t i=rts_deque_begin; i < rts_deque_end; i++) {
    entry_stack.at(i)->frame_type = FULL_FRAME; // mark everything as FULL
  }
  rts_deque_begin = rts_deque_end;
  cilksan_assert(rts_deque_begin == entry_stack.size()-1 );

  DBG_TRACE(DEBUG_DEQUE, 
    "After simulating steal, new deque range: entry_stack[%d, %d).\n", 
    rts_deque_begin, rts_deque_end);
}

static void update_deque_for_entering_helper() {
  // rts_deque_end points to the slot where this HELPER is inserted
  rts_deque_end = entry_stack.size() - 1; 
  entry_stack.head()->prev_helper = youngest_active_helper;
  youngest_active_helper = rts_deque_end;
  // entry_stack always gets pushed slightly before frame_id gets incremented
  // entry_stack.head()->frame_id = frame_id+1;

  DBG_TRACE(DEBUG_DEQUE, "Enter helper %ld, deque range [%d, %d).\n", 
    entry_stack.head()->frame_id, rts_deque_begin, rts_deque_end);
}

static void update_deque_for_leaving_spawn_helper() {

  uint64_t exiting_frame = entry_stack.head()->frame_id;
  uint32_t prev_helper = entry_stack.head()->prev_helper;
  // the new size after we pop the entry stack
  uint32_t new_estack_size = entry_stack.size() - 1;

  // update rts_deque_begin if we are about to pop off the last frame on deque
  if(new_estack_size == rts_deque_begin) {
    rts_deque_begin--;
    cilksan_assert(
        entry_stack.at(rts_deque_begin)->frame_type == FULL_FRAME);
  }
  cilksan_assert(new_estack_size == rts_deque_end);
  cilksan_assert(rts_deque_end > rts_deque_begin && 
                      rts_deque_end-prev_helper >= 2);

  // move the rts_deque_end to either the previous helper frame or where
  // the deque begin if the previous helper has been stolen.
  uint32_t new_rts_deque_end = prev_helper >= rts_deque_begin ?
                             prev_helper : rts_deque_begin;
  // check that indeed there is not helper in between
  for(uint32_t i=rts_deque_end-1; i > new_rts_deque_end; i--) {
    cilksan_assert(entry_stack.at(i)->entry_type != HELPER);
  }

  // update the youngest helper
  cilksan_assert(rts_deque_end == youngest_active_helper);
  youngest_active_helper = prev_helper;
  rts_deque_end = new_rts_deque_end;

  cilksan_assert(rts_deque_end >= rts_deque_begin);
  DBG_TRACE(DEBUG_DEQUE, "Leave helper frame %ld, deque range [%d-%d).\n",
      exiting_frame, rts_deque_begin, rts_deque_end); 
}

static void update_deque_for_leaving_cilk_function() {

  uint64_t exiting_frame = entry_stack.head()->frame_id;
  enum FrameType_t exiting_frame_type = entry_stack.head()->frame_type;
  // the new size after we pop the entry stack
  uint32_t new_estack_size = entry_stack.size() - 1;

  if(new_estack_size == rts_deque_begin) { 
    // Either the worker has one FULL frame (not on deque), and we are 
    // popping that FULL frame off, or we returning from the last Cilk frame.
    cilksan_assert( rts_deque_end-rts_deque_begin == 0 && 
        (exiting_frame_type == FULL_FRAME || rts_deque_begin == 1) &&
        entry_stack.ancestor(1)->frame_type == FULL_FRAME ); 

    // we are about to pop off the last frame on deque; update deque_begin/end
    rts_deque_begin--;
    rts_deque_end--;
  }

  cilksan_assert(rts_deque_end >= rts_deque_begin);
  DBG_TRACE(DEBUG_DEQUE, "Leave Cilk frame %ld, deque range [%d-%d).\n",
    exiting_frame, rts_deque_begin, rts_deque_end); 
}
#endif

// this is a runtime function
// Note that extern "C" is important, as this file is compiled as a CPP file.
extern "C" void __cilkrts_promote_own_deque(struct __cilkrts_worker *worker);


// These are called from cilksan.cpp
static void simulate_steal() {
   
  DBG_TRACE(DEBUG_DEQUE, "Simulate steal; promote entry_stack[%d, %d).\n",
      rts_deque_begin, rts_deque_end);
  WHEN_CILKSAN_DEBUG( check_deque_invariants(); )
  
  // simulate steals in runtime deque; call __cilkrts_promote_own_deque
  /* __cilkrts_promote_own_deque(worker); */
  // update the debugging bookkeeping info to reflect steals
  WHEN_CILKSAN_DEBUG( update_deque_for_simulating_steals(); )
}

// Check if the spawner that we are about to return back to has its upcoming
// continuation stolen.  If so, we need to update the PBag_index (i.e., push
// an "empty" PBag onto the stack of PBags) and the view_id for the spawner.
// At this point, both entry_stack and frame has both popped off the HELPER,
// so what's on top is basically the spawner we are checking.  
static void update_reducer_view() {

  FrameData_t *spawner = frame_stack.head();
  if( next_continuation_was_stolen(spawner) ) {
    // update the current view id if a steal occurred at this continuation
    spawner->curr_view_id = view_id++;
    spawner->Pbag_index++;
    cilksan_assert(spawner->Pbag_index < MAX_NUM_STEALS);

    DBG_TRACE(DEBUG_REDUCER, 
        "Increment frame %ld to Pbag index %u and view id %lu.\n",
        spawner->Sbag->get_set_node()->get_func_id(),
        spawner->Pbag_index, spawner->curr_view_id);
  }
}

