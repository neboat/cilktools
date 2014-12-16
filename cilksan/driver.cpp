#include <malloc.h>
#include <dlfcn.h> 
#include <execinfo.h>
#include <internal/abi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsan/rtl/tsan_interface.h>
#include <tsan/rtl/tsan_interface_atomic.h>
#include <unistd.h>

// runtime internal abi
#include <internal/abi.h>

#include "cilksan_internal.h"
#include "debug_util.h"
#include "mem_access.h"
#include "stack.h"


// HACK --- only works for linux jmpbuf
#define GET_RSP(sf) ((uint64_t) sf->ctx[2])

// global var: FILE io used to print error messages 
FILE *err_io;

// Defined in print_addr.cpp
extern void read_proc_maps(); 
extern void delete_proc_maps(); 
extern void print_addr(FILE *f, void *a);
// declared in cilksan; for debugging only
#if CILKSAN_DEBUG
extern enum EventType_t last_event;  
#endif

static bool TOOL_INITIALIZED = false;

// When either is set to false, no errors are output
static bool instrumentation = false;
// needs to be reentrant due to reducer operations; 0 means checking
static int checking_disabled = 0;


static inline void enable_instrumentation() {
  DBG_TRACE(DEBUG_BASIC, "Enable instrumentation.\n");
  instrumentation = true;
}

static inline void disable_instrumentation() {
  DBG_TRACE(DEBUG_BASIC, "Disable instrumentation.\n");
  instrumentation = false;
}

static inline void enable_checking() {
  checking_disabled--;
  DBG_TRACE(DEBUG_BASIC, "%d: Enable checking.\n", checking_disabled);
  cilksan_assert(checking_disabled >= 0);
}

static inline void disable_checking() {
  cilksan_assert(checking_disabled >= 0);
  checking_disabled++;
  DBG_TRACE(DEBUG_BASIC, "%d: Disable checking.\n", checking_disabled);
}

// outside world (including runtime).
// Non-inlined version for user code to use
extern "C" void __cilksan_enable_checking() {
  checking_disabled--;
  cilksan_assert(checking_disabled >= 0);
  DBG_TRACE(DEBUG_BASIC, "External enable checking (%d).\n", checking_disabled);
}

// Non-inlined version for user code to use
extern "C" void __cilksan_disable_checking() {
  cilksan_assert(checking_disabled >= 0);
  checking_disabled++;
  DBG_TRACE(DEBUG_BASIC, "External disable checking (%d).\n", checking_disabled);
}

static inline bool should_check() {
  return(instrumentation && checking_disabled == 0); 
}

extern "C" void cilk_spawn_prepare() {
  disable_checking();
  cilksan_assert(last_event == NONE);
  WHEN_CILKSAN_DEBUG( last_event = SPAWN_PREPARE; ) 
}

extern "C" void cilk_spawn_or_continue(int in_continuation) {
  cilksan_assert( (!in_continuation && last_event == SPAWN_PREPARE) 
                       || (in_continuation && last_event == RUNTIME_LOOP) );
  WHEN_CILKSAN_DEBUG( last_event = NONE; ) 
  enable_checking();
}

extern "C" void cilk_enter_begin() {
  disable_checking();
  static bool first_call = true;
  if(first_call) {
    cilksan_init();
    first_call = false;
  }
  cilksan_assert(TOOL_INITIALIZED);
  cilksan_do_enter_begin();
}

extern "C" void cilk_enter_helper_begin() {
  disable_checking();
  cilksan_do_enter_helper_begin();
}

extern "C" void 
cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp) {
  static bool first_call = true;

  if(__builtin_expect(first_call, 0)) {
    cilksan_do_enter_end(sf->worker, (uint64_t)rsp);
    first_call = false;
    // turn on instrumentation now
    enable_instrumentation();
  } else {
    cilksan_do_enter_end(NULL, (uint64_t)rsp);
  }
  enable_checking();
}

extern "C" void cilk_detach_begin(__cilkrts_stack_frame *parent_sf) {
  // fprintf(stderr, "PARENT sf: %p, fp: %p, rip: %p, sp: %p.\n", 
  // parent_sf, parent_sf->ctx[0], parent_sf->ctx[1], parent_sf->ctx[2]);
  disable_checking();
  cilksan_do_detach_begin();
}

extern "C" void cilk_detach_end() {
  cilksan_do_detach_end();
  enable_checking();
}

extern "C" void cilk_sync_begin() {
  disable_checking(); 
  cilksan_assert(TOOL_INITIALIZED);
  cilksan_do_sync_begin();
}

extern "C" void cilk_sync_end() {
  cilksan_do_sync_end();
  cilksan_assert(TOOL_INITIALIZED);
  enable_checking();
}

extern "C" void cilk_leave_begin(void *p) {
  disable_checking();
  cilksan_do_leave_begin();
}

extern "C" void cilk_leave_end() {
  cilksan_do_leave_end();
  enable_checking();
}

// Warning warning Will Robinson!
// __tsan_init() runs before any of the static objects' constructors run.  
// And calls into the tsan functions can occur after some of the static 
// objects' destructors have run.  So we really can use any static objects with 
// constructors or destructors.
// This is a case that motivates the Google Style Guide's rule forbidding static 
// variables of global type.
//    http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Static_and_Global_Variableswould 
//
// Our strategy will be to use pointers for the global variables; to initialize 
// them when needed; and never free the pointer.
// Getting valgrind to not complain about those pointers may be tricky.

// called upon process exit
static void tsan_destroy(void) {
    // fprintf(err_io, "tsan_destroy called.\n");
    cilksan_deinit();

    fflush(stdout);
    delete_proc_maps();
}

static void init_internal() {
    read_proc_maps();
    if (ERROR_FILE) {
        FILE *tmp = fopen(ERROR_FILE, "w+");
        if(tmp) err_io = tmp;
    }
    if (err_io == NULL) err_io = stderr;

    char *e = getenv("CILK_NWORKERS");
    if (!e || 0!=strcmp(e, "1")) {
        // fprintf(err_io, "Setting CILK_NWORKERS to be 1\n");
        if( setenv("CILK_NWORKERS", "1", 1) ) {
            fprintf(err_io, "Error setting CILK_NWORKERS to be 1\n");
            exit(1);
        }
    }
}

void __tsan_init() {
    // kind of a hack, but sometimes __tsan_init gets called twice ...
    if(TOOL_INITIALIZED) return;

    atexit(tsan_destroy);
    init_internal();
    // moved this later when we enter the first Cilk frame
    // cilksan_init();
    // enable_instrumentation();
    TOOL_INITIALIZED = true;
    // fprintf(err_io, "tsan_init called.\n");
}

// invoked whenever a function enters; no need for this
void __tsan_func_entry(void *pc) { 
    cilksan_assert(TOOL_INITIALIZED);
    // DBG_TRACE(DEBUG_BASIC, "%s rip %p frame addr = %p\n", 
    //           __FUNCTION__, pc, __builtin_frame_address(0));
    // XXX Let's focus on Cilk function for now; maybe put it back later
    // cilksan_do_function_entry((uint64_t)__builtin_frame_address(0));
}

void __tsan_func_exit() {
    cilksan_assert(TOOL_INITIALIZED);
    // XXX Let's focus on Cilk function for now; maybe put it back later
    // cilksan_do_function_exit();
}

void __tsan_vptr_update(void **vptr_p, void *new_val) {
    // XXX: Not doing anything at the moment.
}

// get_user_code_rip calls the system backtrace to walk the stack to obtain 
// list of return addresses sitting on the cucrrent stack.  
// Buffer[0] always stores the return address for the backtrace call.  
// Where we find the rip for the access performed in the user program 
// depends on how deeply the backtrace call is nested, which is provided
// as an arg to get_user_code_rip. 
/*
static inline void *get_user_code_rip(int depth) {
    disable_checking();
    void *buffer[depth];
    int res = backtrace(buffer, depth);
    cilksan_assert(res == depth);
    enable_checking();
    return buffer[depth-1];
} */

// Assuming tsan_read/write is inlined, the stack should look like this:
//
// -------------------------------------------
// | user func that is about to do a memop   |
// -------------------------------------------
// | __tsan_read/write[1-16]                 |
// -------------------------------------------
// | backtrace (assume __tsan_read/write and | 
// |            get_user_code_rip is inlined)|
// -------------------------------------------
//
// In the user program, __tsan_read/write[1-16] are inlined
// right before the corresponding read / write in the user code.
// the return addr of __tsan_read/write[1-16] is the rip for the read / write
static inline void tsan_read(void *addr, size_t size, void *rip) {
    cilksan_assert(TOOL_INITIALIZED);
    if(should_check()) {
        disable_checking();
        DBG_TRACE(DEBUG_MEMORY, "%s read %p\n", __FUNCTION__, addr);
        cilksan_do_read((uint64_t)rip, (uint64_t)addr, size);
        enable_checking();
    } else {
        DBG_TRACE(DEBUG_MEMORY, "SKIP %s read %p\n", __FUNCTION__, addr);
    }
}

static inline void tsan_write(void *addr, size_t size, void *rip) {
    cilksan_assert(TOOL_INITIALIZED);
    if(should_check()) {
        disable_checking();
        DBG_TRACE(DEBUG_MEMORY, "%s wrote %p\n", __FUNCTION__, addr);
        cilksan_do_write((uint64_t)rip, (uint64_t)addr, size);
        enable_checking();
    } else {
        DBG_TRACE(DEBUG_MEMORY, "SKIP %s wrote %p\n", __FUNCTION__, addr);
    }
}

void __tsan_read1(void *addr) {
    tsan_read(addr, 1, __builtin_return_address(0));
}

void __tsan_read2(void *addr) {
    tsan_read(addr, 2, __builtin_return_address(0));
}

void __tsan_read4(void *addr) {
    tsan_read(addr, 4, __builtin_return_address(0));
}

void __tsan_read8(void *addr) {
    tsan_read(addr, 8, __builtin_return_address(0));
}

void __tsan_read16(void *addr) {
    tsan_read(addr, 16, __builtin_return_address(0));
}

void __tsan_write1(void *addr) {
    tsan_write(addr, 1, __builtin_return_address(0));
}

void __tsan_write2(void *addr) {
    tsan_write(addr, 2, __builtin_return_address(0));
}

void __tsan_write4(void *addr) {
    tsan_write(addr, 4, __builtin_return_address(0));
}

void __tsan_write8(void *addr) {
    tsan_write(addr, 8, __builtin_return_address(0));
}

void __tsan_write16(void *addr) {
    tsan_write(addr, 16, __builtin_return_address(0));
}

extern "C"
int __tsan_atomic32_fetch_add(volatile int *a, int v, __tsan_memory_order mo) {
    // not doing anything right now
    // fprintf(stderr, "XXX atomic fetch add called: int: %p, v %d, mo %d.\n", a, v, mo);
  return 0;
}

typedef void*(*malloc_t)(size_t);
static malloc_t real_malloc = NULL;

extern "C" void* malloc(size_t s) {

    // make it 8-byte aligned; easier to erase from shadow mem
    uint64_t new_size = ALIGN_BY_NEXT_MAX_GRAIN_SIZE(s);

    // Don't try to init, since that needs malloc.  
    if (real_malloc==NULL) {
        real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");
        char *error = dlerror();
        if (error != NULL) {
            fputs(error, err_io);
            fflush(err_io);
            abort();
        }
    }
    void *r = real_malloc(new_size);

    if(TOOL_INITIALIZED && should_check()) {
        // cilksan_clear_shadow_memory((size_t)r, (size_t)r+malloc_usable_size(r)-1);
        cilksan_clear_shadow_memory((size_t)r, (size_t)r+new_size);
    }

    return r;
}
