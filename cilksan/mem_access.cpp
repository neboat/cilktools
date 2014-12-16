#include <inttypes.h>
 
#include "cilksan_internal.h"
#include "mem_access.h"

extern void report_race(uint64_t first_inst, uint64_t second_inst, 
                        uint64_t addr, enum RaceType_t race_type); 

// get the start and end indices and gtype to use for accesing 
// the readers / writers lists; the gtype is the largest granularity
// that this memory access is aligned with 
enum GrainType_t 
MemAccessList_t::get_mem_index(uint64_t addr, size_t size, 
                               int& start, int& end) {
  cilksan_assert(size < MAX_GRAIN_SIZE || 
      (size == MAX_GRAIN_SIZE && IS_ALIGNED_WITH_GTYPE(addr, EIGHT)));

  start = (int) (addr & (uint64_t)(MAX_GRAIN_SIZE-1));
  end = (int) ((addr + size) & (uint64_t)(MAX_GRAIN_SIZE-1));
  if(end == 0) end = MAX_GRAIN_SIZE;

  enum GrainType_t gtype = mem_size_to_gtype(size);
  if( IS_ALIGNED_WITH_GTYPE(addr, gtype) == false ) { gtype = ONE; }
  cilksan_assert( (end-start) == size );

  return gtype;
}

void MemAccessList_t::break_list_into_smaller_gtype(bool for_read,
                                               enum GrainType_t new_gtype) {
  MemAccess_t **l = writers;
  enum GrainType_t gtype = writer_gtype;
  if(for_read) {
    l = readers;
    gtype = reader_gtype;
  }
  cilksan_assert(gtype > new_gtype && new_gtype != UNINIT);
  const int stride = gtype_to_mem_size[new_gtype];
  MemAccess_t *acc = l[0];

  for(int i = stride; i < MAX_GRAIN_SIZE; i += stride) {
    if( IS_ALIGNED_WITH_GTYPE(i, gtype) ) { 
      acc = l[i];
    } else if(acc) {
      cilksan_assert(l[i] == NULL);
      acc->inc_ref_count();
      l[i] = acc;
    }
  }

  if(for_read) {
    reader_gtype = new_gtype;
  } else {
    writer_gtype = new_gtype;
  }
}

// Check races on memory represented by this mem list with this read access
// Once done checking, update the mem list with this new read access
void MemAccessList_t::check_races_and_update_with_read(uint64_t inst_addr, 
                              uint64_t addr, size_t mem_size, bool on_stack,
                              enum AccContextType_t context, 
                              DisjointSet_t<SPBagInterface *> *curr_sbag,
                              DisjointSet_t<SPBagInterface *> *curr_top_pbag, 
                              uint64_t curr_view_id) {

  DBG_TRACE(DEBUG_MEMORY, "check race w/ read addr %lx and size %lu.\n",
            addr, mem_size);
  cilksan_assert( addr >= start_addr && 
                       (addr+mem_size) <= (start_addr+MAX_GRAIN_SIZE) );
  cilksan_assert( context != REDUCE || curr_top_pbag != NULL );

  // check races with the writers
  // start (inclusive) and end (exclusive) indices covered by this mem access; 
  int start, end; // implicitly initialized once only
  MemAccess_t *writer = NULL;
  const enum GrainType_t gtype = get_mem_index(addr, mem_size, start, end);
  enum GrainType_t min_gtype = gtype;

  cilksan_assert(start >= 0 && start < end && end <= MAX_GRAIN_SIZE);

  if(writer_gtype == UNINIT) {
    goto update; // no writers to check races with 
  } else if( writer_gtype < gtype ) {
    // stride the list using the min granularity of two
    min_gtype = writer_gtype;
  } else if(writer_gtype > gtype) { // find the writer that covers the start
    writer = writers[ get_prev_aligned_index(start, writer_gtype) ];
  }
  cilksan_assert(IS_ALIGNED_WITH_GTYPE(end, min_gtype)); 

  // walk through the indices that this memory access cover to check races
  // against any writer found within this range 
  for(int i = start; i < end; i += gtype_to_mem_size[min_gtype]) {
    if( IS_ALIGNED_WITH_GTYPE(i, writer_gtype) ) {
      // encountered the next writer within the range; update writer
      writer = writers[i]; // can be NULL
    }
    if( writer && writer->races_with(start_addr+i, on_stack,
                                     curr_top_pbag, context, curr_view_id) ) {
      report_race(writer->rip, inst_addr, start_addr+i, WR_RACE);
    }
  }

update:
  // now we update the readers list with this access 
  MemAccess_t *reader = NULL;
  if( reader_gtype == UNINIT ) {
    reader_gtype = gtype;
  } else if( reader_gtype > gtype ) {
    break_readers_into_smaller_gtype(gtype);
  }
  // now reader_gtype = min{ old reader_gtype, gtype };
  cilksan_assert(reader_gtype <= gtype); 
  cilksan_assert(IS_ALIGNED_WITH_GTYPE(end, reader_gtype)); 

  // update the readers list with this mem access; same start / end indices
  MemAccess_t *new_reader = new MemAccess_t(curr_sbag, inst_addr);
  for(int i = start; i < end; i += gtype_to_mem_size[reader_gtype]) {
    reader = readers[i];
    if(reader == NULL) {
      new_reader->inc_ref_count();
      readers[i] = new_reader;
    } else { // potentially update the last reader if it exists
      SPBagInterface *last_rset = reader->func->get_set_node();
      // replace it only if it is in series with this access, i.e., if it's
      // one of the following:
      // a) in a SBag 
      // b) in a PBage but should have been replaced because the access is
      // actually on the newly allocated stack frame (i.e., cactus stack abstraction) 
      // c) access is made by a REDUCE strand and previous access is in the 
      // top-most PBag.
      if( last_rset->is_SBag() || 
          (on_stack && last_rset->get_rsp() >= start_addr+i) ||
          (context == REDUCE && last_rset == curr_top_pbag->get_node()) ) {
        if(reader->dec_ref_count() == 0) {
          delete reader;
        }
        // note that ref count is decremented regardless
        new_reader->inc_ref_count();
        readers[i] = new_reader;
      }
    }
  }
  if(new_reader->ref_count == 0) delete new_reader;
}

// Check races on memory represented by this mem list with this write access
// Also, update the writers list.  
// Very similar to check_races_and_update_with_read function above.
void MemAccessList_t::check_races_and_update_with_write(uint64_t inst_addr,
                              uint64_t addr, size_t mem_size, bool on_stack,
                              enum AccContextType_t context,
                              DisjointSet_t<SPBagInterface *> *curr_sbag,
                              DisjointSet_t<SPBagInterface *> *curr_top_pbag,
                              uint64_t curr_view_id) {

  DBG_TRACE(DEBUG_MEMORY, "check race w/ write addr %lx and size %lu.\n",
            addr, mem_size);
  cilksan_assert( addr >= start_addr && 
                       (addr+mem_size) <= (start_addr+MAX_GRAIN_SIZE) );
  cilksan_assert( context != REDUCE || curr_top_pbag != NULL );

  int start, end;
  MemAccess_t *writer = NULL;
  const enum GrainType_t gtype = get_mem_index(addr, mem_size, start, end);
  enum GrainType_t min_gtype = gtype;
  cilksan_assert(start >= 0 && start < end && end <= MAX_GRAIN_SIZE);

  if( writer_gtype == UNINIT ) {
    writer_gtype = gtype;
  } else if( writer_gtype > gtype ) {
    break_writers_into_smaller_gtype(gtype);
  }
  // now writer_gtype = min{ old writer_gtype, gtype };
  cilksan_assert(writer_gtype <= gtype); 
  cilksan_assert(IS_ALIGNED_WITH_GTYPE(end, writer_gtype)); 

  MemAccess_t *new_writer = new MemAccess_t(curr_sbag, inst_addr);

  // now traverse through the writers list to both report race and update
  for(int i = start; i < end; i += gtype_to_mem_size[writer_gtype]) {
    writer = writers[i];
    if(writer == NULL) {
      new_writer->inc_ref_count();
      writers[i] = new_writer;
    } else { // last writer exists; possibly report race and replace it
      if( writer->races_with(start_addr+i, on_stack, 
                             curr_top_pbag, context, curr_view_id) ) { 
        // report race
        report_race(writer->rip, inst_addr, start_addr+i, WW_RACE);
      }
      // replace the last writer if it's logically in series with this writer, 
      // (same 3 conditions as update for last reader)
      SPBagInterface *last_wset = writer->func->get_set_node();
      if( last_wset->is_SBag() || 
          (on_stack && last_wset->get_rsp() >= start_addr+i) ||
          (context == REDUCE && last_wset == curr_top_pbag->get_node()) ) {
        if(writer->dec_ref_count() == 0) {
          delete writer;
        }
        // note that ref count is decremented regardless
        new_writer->inc_ref_count();
        writers[i] = new_writer;
      }
    }
  }
  if(new_writer->ref_count == 0) delete new_writer;

  // Now we detect races with the readers
  MemAccess_t *reader = NULL;
  if( reader_gtype == UNINIT ) {
    return; // we are done
  } else if( reader_gtype < gtype ) {
    min_gtype = reader_gtype;
  } else if(reader_gtype > gtype) {
    reader = readers[ get_prev_aligned_index(start, reader_gtype) ];
  }
  cilksan_assert(IS_ALIGNED_WITH_GTYPE(end, min_gtype)); 

  // traverse through the readers list to report race, using the 
  // min granularity between the reader_gtype and gtype
  for(int i = start; i < end; i += gtype_to_mem_size[min_gtype]) {
    if( IS_ALIGNED_WITH_GTYPE(i, reader_gtype) ) {
      reader = readers[i];
    }
    if( reader && reader->races_with(start_addr+i, on_stack,
                                     curr_top_pbag, context, curr_view_id) ) {
      report_race(reader->rip, inst_addr, start_addr+i, RW_RACE);
    }
  }
}

MemAccessList_t::MemAccessList_t(uint64_t addr, bool is_read, 
                                 MemAccess_t *acc, size_t mem_size) 
  : start_addr( ALIGN_BY_PREV_MAX_GRAIN_SIZE(addr) ), 
    reader_gtype(UNINIT), writer_gtype(UNINIT) {

  for(int i=0; i < MAX_GRAIN_SIZE; i++) {
    readers[i] = writers[i] = NULL;
  }

  int start, end;
  const enum GrainType_t gtype = get_mem_index(addr, mem_size, start, end);

  cilksan_assert(start >= 0 && start < end && end <= MAX_GRAIN_SIZE);

  MemAccess_t **l;
  if(is_read) {
    reader_gtype = gtype;
    l = readers;
  } else {
    writer_gtype = gtype;
    l = writers;
  }
  for(int i=start; i < end; i += gtype_to_mem_size[gtype]) {
    acc->inc_ref_count();
    l[i] = acc;
  }
}

MemAccessList_t::~MemAccessList_t() {
  MemAccess_t *acc;
  if(reader_gtype != UNINIT) {
    for(int i=0; i < MAX_GRAIN_SIZE; i+=gtype_to_mem_size[reader_gtype]) {
      acc = readers[i]; 
      if(acc && acc->ref_count == 0) {
        delete acc;
        readers[i] = 0;
      }
    }
  }

  if(writer_gtype != UNINIT) {
    for(int i=0; i < MAX_GRAIN_SIZE; i+=gtype_to_mem_size[writer_gtype]) {
      acc = writers[i]; 
      if(acc && acc->ref_count == 0) {
        delete acc;
        writers[i] = 0;
      }
    }
  }
}

#if CILKSAN_DEBUG 
void MemAccessList_t::check_invariants(uint64_t current_func_id) {
  SPBagInterface *lca;
  for(int i=0; i < MAX_GRAIN_SIZE; i++) {
    if(readers[i]) {
      lca = readers[i]->func->get_set_node();
      cilksan_assert(current_func_id >= lca->get_func_id());
      // if LCA is a P-node (Cilk function), its rsp must have been initialized
      cilksan_assert(lca->is_SBag() || lca->get_rsp() != UNINIT_STACK_PTR);
    }
    if(writers[i]) { // same checks for the writers
      lca = writers[i]->func->get_set_node();
      cilksan_assert(current_func_id >= lca->get_func_id());
      cilksan_assert(lca->is_SBag() || lca->get_rsp() != UNINIT_STACK_PTR);
    }
  }
}
#endif // CILKSAN_DEBUG

