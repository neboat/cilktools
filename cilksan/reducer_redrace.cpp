// Cilk fake lock related stuff
typedef struct LockData_t {
    LockData_t():lock(NULL) {}
    void *lock;
} LockData_t;
// a stack the locks we get from cilkscreen_acquire/release_lock
static Stack_t<LockData_t> lock_stack;


static inline void acquire_lock(void *lock) {
  racedetector_assert(lock_stack.head()->lock == NULL);
  lock_stack.head()->lock = lock;
  lock_stack.push();
  disable_checking();
}

static inline void release_lock(void *lock) {
  lock_stack.pop();
  racedetector_assert(lock_stack.head()->lock == lock);
  lock_stack.head()->lock = NULL;
  enable_checking();
  racedetector_assert(lock_stack.size() > 1 || checking_disabled == 0);
}


// XXX Stuff that we no longer need 

/* There will be 2 ignore range total, one for worker state and one for
 * worker's deque.  Any additional worker causes an error, since this tool is
 * meant to be used with single-thread execution. */
#define EXPECTED_IGNORES 1
ADDRINT ignore_begin[EXPECTED_IGNORES] = { static_cast<ADDRINT>(0ULL) };
ADDRINT ignore_end[EXPECTED_IGNORES] = { static_cast<ADDRINT>(0ULL) };

static inline void 
get_info_on_inst_addr(ADDRINT inst_addr, INT32 *line_no, std::string *file) {
  PIN_LockClient();
  PIN_GetSourceLocation(inst_addr, NULL, line_no, file);
  PIN_UnlockClient();
}

int main(int argc, char *argv[]) {

  // reducer related annotation
  zca.insert_annotation_calls("cilkscreen_begin_reduce_strand",
                              (AFUNPTR)begin_view_aware_strand,
                              IARG_uint32_t, ((enum AccContextType_t)REDUCE),
                              IARG_END);

  zca.insert_annotation_calls("cilkscreen_end_reduce_strand",
                              (AFUNPTR)end_view_aware_strand,
                              IARG_uint32_t, ((enum AccContextType_t)REDUCE),
                              IARG_END);
  
  zca.insert_annotation_calls("cilkscreen_begin_update_strand",
                              (AFUNPTR)begin_view_aware_strand,
                              IARG_uint32_t, ((enum AccContextType_t)UPDATE),
                              IARG_END);

  zca.insert_annotation_calls("cilkscreen_end_update_strand",
                              (AFUNPTR)end_view_aware_strand,
                              IARG_uint32_t, ((enum AccContextType_t)UPDATE),
                              IARG_END);
  
  // Not supported at the moment 
  zca.insert_annotation_calls("cilkscreen_clean",
                              (AFUNPTR)metacall_error_exit,
                              IARG_PTR,
                              (char *)"cilkscreen_clean",
                              IARG_END);

}
