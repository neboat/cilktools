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
#include <execinfo.h>

/* #include <cilk/cilk_api.h> */
/* #include <cilk/common.h> */
/* #include <internal/abi.h> */

#include <cilktool.h>
#include "cilkprof_stack.h"

#ifndef SERIAL_TOOL
#define SERIAL_TOOL 1
#endif

#ifndef TRACE_CALLS
#define TRACE_CALLS 1
#endif

#if !SERIAL_TOOL
#include "cilkprof_stack_reducer.h"
#endif

#if TRACE_CALLS
#define WHEN_TRACE_CALLS(ex) ex
#else
#define WHEN_TRACE_CALLS(ex)
#endif

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */


#if SERIAL_TOOL
cilkprof_stack_t ctx_stack;
#else
CILK_C_DECLARE_REDUCER(cilkprof_stack_t) ctx_stack =
  CILK_C_INIT_REDUCER(cilkprof_stack_t,
		      reduce_cilkprof_stack,
		      identity_cilkprof_stack,
		      destroy_cilkprof_stack,
		      {NULL});
#endif

bool TOOL_INITIALIZED = false;

/*
 * Linked list of mappings.
 */

typedef struct mapping_t {
  uintptr_t low, high;
  char *path;
} mapping_t;

typedef struct mapping_list_el_t {
  mapping_t map;
  struct mapping_list_el_t* next;
} mapping_list_el_t;

typedef struct mapping_list_t {
  mapping_list_el_t *head;
  mapping_list_el_t *tail;
} mapping_list_t;

mapping_list_t maps = { .head = NULL, .tail = NULL };

/*************************************************************************/
/**
 * Helper methods.
 */

// Store the current "time" into TIMER.
static inline void gettime(struct timespec *timer) {
  // TB 2014-08-01: This is the "clock_gettime" variant I could get
  // working with -std=c11.  I want to use TIME_MONOTONIC instead, but
  // it does not appear to be supported on my system.
  // timespec_get(timer, TIME_UTC);
  clock_gettime(CLOCK_MONOTONIC, timer);
}

// Get the number of nanoseconds elapsed between STOP and START.
static inline uint64_t elapsed_nsec(const struct timespec *stop,
			     const struct timespec *start) {
  return (uint64_t)(stop->tv_sec - start->tv_sec) * 1000000000ll
    + (stop->tv_nsec - start->tv_nsec);
}

#if SERIAL_TOOL
// Ensure that this tool is run serially
static inline void ensure_serial_tool(void) {
  // assert(1 == __cilkrts_get_nworkers());
  fprintf(stderr, "Forcing CILK_NWORKERS=1.\n");
  char *e = getenv("CILK_NWORKERS");
  if (!e || 0!=strcmp(e, "1")) {
    // fprintf(err_io, "Setting CILK_NWORKERS to be 1\n");
    if( setenv("CILK_NWORKERS", "1", 1) ) {
      fprintf(stderr, "Error setting CILK_NWORKERS to be 1\n");
      exit(1);
    }
  }
}
#endif

// Arch-dependent method for translating a RIP into a call site
static uintptr_t rip2cc(uintptr_t rip) {
  return rip - 5;
}

static void read_proc_maps(void) {
  pid_t pid = getpid();
  char path[100];
  snprintf(path, sizeof(path), "/proc/%d/maps", pid);
  if (0) printf("path=%s\n", path);
  FILE *f = fopen(path, "r");
  if (0) printf("file=%p\n", f);
  assert(f);
  char *lineptr = NULL;
  size_t n;
  while (1) {
    ssize_t s = getline(&lineptr, &n, f);
    if (s==-1) break;
    if (0) printf("Got %ld line=%s size=%ld\n", s, lineptr, n);
    uintptr_t start, end;
    char c0, c1, c2, c3;
    int off, major, minor, inode;
    char *pathname;
    sscanf(lineptr, "%lx-%lx %c%c%c%c %x %x:%x %x %ms",
	   &start, &end, &c0, &c1, &c2, &c3, &off, &major, &minor, &inode, &pathname);
    if (0) printf(" start=%lx end=%lx path=%s\n", start, end, pathname);
    // Make new map
    mapping_list_el_t *m = (mapping_list_el_t*)malloc(sizeof(mapping_list_el_t));
    m->map.low = start;
    m->map.high = end;
    m->map.path = pathname;
    m->next = NULL;
    // Push map onto list
    if (NULL == maps.head) {
      maps.head = m;
      maps.tail = m;
    } else {
      maps.tail->next = m;
    }
  }
  /* if (0) printf("maps.size()=%lu\n", maps->size()); */
  fclose(f);
}

static void print_addr(uintptr_t a) {
  uintptr_t ai = a;
  /* if (1) printf(" PC= %lx\n", a); */
  mapping_list_el_t *map_lst_el = maps.head;
  while (NULL != map_lst_el) {
    mapping_t *map = &(map_lst_el->map);
    if (0) printf("Comparing %lx to %lx:%lx\n", ai, map->low, map->high);
    if (map->low <= ai && ai < map->high) {
      uintptr_t off = ai-map->low;
      const char *path = map->path;
      /* if (1) printf("%lx is offset 0x%lx in %s\n", a, off, path); */
      bool is_so = strcmp(".so", path+strlen(path)-3) == 0;
      if (0) {if (is_so) printf(" is so\n"); else printf(" not so\n");}
      char *command;
      if (is_so) {
	asprintf(&command, "echo %lx | addr2line -e %s", off, path);
      } else {
	asprintf(&command, "echo %lx | addr2line -e %s", ai, path);
      }
      /* if (debug_level>1) printf("Doing system(\"%s\");\n", command); */
      system(command);
      free(command);
      return;
    }
    map_lst_el = map_lst_el->next;
  }
  printf("%lx is not in range\n", a);
}

/* static void print_bt(void) { */
/*   const int N=10; */
/*   void* buf[N]; */
/*   int n = backtrace(buf, N); */
/*   if (1) { */
/*     for (int i = 0; i < n; i++) { */
/*       print_addr((uintptr_t)buf[i]); */
/*     } */
/*   } else { */
/*     assert(n>=2); */
/*     print_addr((uintptr_t)buf[2]); */
/*   } */
/* } */

/*************************************************************************/

void cilk_tool_init(void) {
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_init()\n"); )

  // This is a serial tool
#if SERIAL_TOOL
  ensure_serial_tool();
  cilkprof_stack_init(&ctx_stack, MAIN);
  ctx_stack.in_user_code = true;
  gettime(&(ctx_stack.start));
#else
  cilkprof_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);
  CILK_C_REGISTER_REDUCER(ctx_stack);
  REDUCER_VIEW(ctx_stack).in_user_code = true;
  gettime(&(REDUCER_VIEW(ctx_stack).start));
#endif

  TOOL_INITIALIZED = true;
}

void cilk_tool_destroy(void) {
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_destroy()\n"); )

#if !SERIAL_TOOL
  CILK_C_UNREGISTER_REDUCER(ctx_stack);

  free_cc_hashtable(REDUCER_VIEW(ctx_stack).wrk_table);
  free_cc_hashtable(REDUCER_VIEW(ctx_stack).bot->prefix_table);
  free_cc_hashtable(REDUCER_VIEW(ctx_stack).bot->lchild_table);
  free_cc_hashtable(REDUCER_VIEW(ctx_stack).bot->contin_table);
#else
  free_cc_hashtable(ctx_stack.wrk_table);
  free_cc_hashtable(ctx_stack.bot->prefix_table);
  free_cc_hashtable(ctx_stack.bot->lchild_table);
  free_cc_hashtable(ctx_stack.bot->contin_table);
#endif

  TOOL_INITIALIZED = false;
}

void cilk_tool_print(void) {
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_print()\n"); )

  assert(TOOL_INITIALIZED);

  cilkprof_stack_t *stack;
#if SERIAL_TOOL
  stack = &ctx_stack;
#else
  stack = &REDUCER_VIEW(ctx_stack);
#endif 

  assert(NULL != stack->bot);
  assert(MAIN == stack->bot->func_type);

  uint64_t span = stack->bot->prefix_spn + stack->bot->contin_spn;
  /* fprintf(stderr, "prefix_table->list_size = %d, prefix_table->table_size = %d\n", */
  /* 	  stack->bot->prefix_table->list_size, stack->bot->prefix_table->table_size); */
  /* fprintf(stderr, "contin_table->list_size = %d, contin_table->table_size = %d\n", */
  /* 	  stack->bot->contin_table->list_size, stack->bot->contin_table->table_size); */

  add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));
  clear_cc_hashtable(stack->bot->contin_table);
  /* fprintf(stderr, "contin_table->list_size = %d, contin_table->table_size = %d\n", */
  /* 	  stack->bot->contin_table->list_size, stack->bot->contin_table->table_size); */

  flush_cc_hashtable(&(stack->bot->prefix_table));

  cc_hashtable_t* span_table = stack->bot->prefix_table;
  fprintf(stderr, "span_table->list_size = %d, span_table->table_size = %d, span_table->lg_capacity = %d\n",
  	  span_table->list_size, span_table->table_size, span_table->lg_capacity);

  uint64_t work = stack->bot->running_wrk;
  flush_cc_hashtable(&(stack->wrk_table));
  cc_hashtable_t* work_table = stack->wrk_table;
  fprintf(stderr, "work_table->list_size = %d, work_table->table_size = %d, work_table->lg_capacity = %d\n",
  	  work_table->list_size, work_table->table_size, work_table->lg_capacity);
  
  /* fprintf(stderr, "work table:\n"); */
  /* for (size_t i = 0; i < (1 << work_table->lg_capacity); ++i) { */
  /*   if (!empty_entry_p(&(work_table->entries[i]))) { */
  /*     fprintf(stderr, "%lx:%d %lu %lu\n", */
  /* 	      work_table->entries[i].rip, */
  /* 	      work_table->entries[i].height, */
  /* 	      work_table->entries[i].wrk, */
  /* 	      work_table->entries[i].spn); */
  /*   } */
  /* } */

  /* fprintf(stderr, "span table:\n"); */
  /* for (size_t i = 0; i < (1 << span_table->lg_capacity); ++i) { */
  /*   if (!empty_entry_p(&(span_table->entries[i]))) { */
  /*     fprintf(stderr, "%lx:%d %lu %lu\n", */
  /* 	      span_table->entries[i].rip, */
  /* 	      span_table->entries[i].height, */
  /* 	      span_table->entries[i].wrk, */
  /* 	      span_table->entries[i].spn); */
  /*   } */
  /* } */

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
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_begin(%p, %p)\n", sf, rip); )

  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */
  cilkprof_stack_t *stack;

  if (!TOOL_INITIALIZED) {
    // This is not exactly the same as what cilk_tool_init() does.
#if SERIAL_TOOL
    ensure_serial_tool();
    cilkprof_stack_init(&ctx_stack, MAIN);
    stack = &ctx_stack;
#else
    cilkprof_stack_init(&(REDUCER_VIEW(ctx_stack)), MAIN);
    CILK_C_REGISTER_REDUCER(ctx_stack);
    stack = &(REDUCER_VIEW(ctx_stack));
#endif
    TOOL_INITIALIZED = true;

  } else {
#if SERIAL_TOOL
    stack = &ctx_stack;
#else
    stack = &(REDUCER_VIEW(ctx_stack));
#endif

    gettime(&(stack->stop));

    assert(NULL != stack->bot);

    // Doesn't matter whether we called or spawned this Cilk function,
    // attribute the work before cilk_enter_begin to the caller / spawner
    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->bot->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    stack->in_user_code = false;
  }

  /* Push new frame onto the stack */
  cilkprof_stack_push(stack, SPAWNER);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip);
}


void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *rip)
{
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif

  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_helper_begin(%p, %p) from SPAWNER\n", sf, rip); )

  assert(NULL != stack->bot);

  /* Push new frame onto the stack */
  cilkprof_stack_push(stack, HELPER);

  /* stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  stack->bot->rip = (uintptr_t)__builtin_extract_return_addr(rip);
}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif

  if (SPAWNER == stack->bot->func_type) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from SPAWNER\n", sf, rsp); )
    assert(!(stack->in_user_code));
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from HELPER\n", sf, rsp); )
  }

  stack->in_user_code = true;
  gettime(&(stack->start));
}

void cilk_tool_c_function_enter(void *rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_enter(%p)\n", rip); )
}

void cilk_tool_c_function_leave(void *rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_leave(%p)\n", rip); )
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_spawn_prepare(%p)\n", sf); )

  cilkprof_stack_t *stack;
  if (TOOL_INITIALIZED) {
#if SERIAL_TOOL
    stack = &ctx_stack;
#else
    stack = &(REDUCER_VIEW(ctx_stack));
#endif

    gettime(&(stack->stop));

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->bot->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    assert(stack->in_user_code);
    stack->in_user_code = false;
  }
}

void cilk_spawn_or_continue(int in_continuation)
{
  cilkprof_stack_t *stack;
#if SERIAL_TOOL
  stack = &ctx_stack;
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif

  if (in_continuation) {
    // In the continuation
    WHEN_TRACE_CALLS( 
      fprintf(stderr, "cilk_spawn_or_continue(%d) from continuation\n", in_continuation); )
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
    gettime(&(stack->start));
  } else {
    // In the spawned child
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_spawn_or_continue(%d) from spawn\n", in_continuation); )
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_begin(%p)\n", parent); )

  cilkprof_stack_t *stack;
#if SERIAL_TOOL
  stack = &ctx_stack;
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif
  
  gettime(&(stack->stop));

  uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
  stack->bot->running_wrk += strand_time;
  stack->bot->contin_spn += strand_time;

  assert(stack->in_user_code);
  stack->in_user_code = false;

  return;
}

void cilk_detach_end(void)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_end()\n"); )

  cilkprof_stack_t *stack;
#if SERIAL_TOOL
  stack = &ctx_stack;
#else
  stack = &(REDUCER_VIEW(ctx_stack));
#endif
  
  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  gettime(&(stack->start));

  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif
  gettime(&(stack->stop));

  if (SPAWNER == stack->bot->func_type) {
    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->bot->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_begin(%p) from SPAWNER\n", sf); )
    assert(stack->in_user_code);
    stack->in_user_code = false;

  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf); )
  }
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif

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
  
  if (SPAWNER == stack->bot->func_type) {
    assert(!(stack->in_user_code));
    stack->in_user_code = true;
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from SPAWNER\n", sf); )
    gettime(&(stack->start));
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from HELPER\n", sf); )
  }
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif

  cilkprof_stack_frame_t *old_bottom;
  bool add_success;
  
  gettime(&(stack->stop));

  assert(stack->in_user_code);
  stack->in_user_code = false;

  if (SPAWNER == stack->bot->func_type) {
    /* fprintf(stderr, "cilk_leave_begin(%p) from SPAWNER\n", sf); */

    uint64_t strand_time = elapsed_nsec(&(stack->stop), &(stack->start));
    stack->bot->running_wrk += strand_time;
    stack->bot->contin_spn += strand_time;

    assert(NULL != stack->bot->parent);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;

    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));

    /* Pop the stack */
    old_bottom = cilkprof_stack_pop(stack);

    stack->bot->running_wrk += old_bottom->running_wrk;
    stack->bot->contin_spn += old_bottom->prefix_spn;

    /* fprintf(stderr, "adding to wrk table\n"); */

    /* Update work table */
    add_success = add_to_cc_hashtable(&(stack->wrk_table),
				      old_bottom->height,
				      old_bottom->rip,
				      old_bottom->running_wrk,
				      old_bottom->prefix_spn);
    assert(add_success);

    /* fprintf(stderr, "adding tables\n"); */

    /* Update continuation span table */
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
    /* fprintf(stderr, "cilk_leave_begin(%p) from HELPER\n", sf); */

    assert(HELPER != stack->bot->parent->func_type);

    assert(0 == stack->bot->lchild_spn);
    stack->bot->prefix_spn += stack->bot->contin_spn;
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));

    /* Pop the stack */
    old_bottom = cilkprof_stack_pop(stack);

    stack->bot->running_wrk += old_bottom->running_wrk;

    /* Update running work table */
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

      /* Update lchild span table */
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
#if SERIAL_TOOL
  cilkprof_stack_t *stack = &ctx_stack;
#else
  cilkprof_stack_t *stack = &(REDUCER_VIEW(ctx_stack));
#endif

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
  }
#endif

  /* if (HELPER != stack->bot->func_type) { */
  /*   assert(!(stack->in_user_code)); */
  /*   stack->in_user_code = true; */
  /*   gettime(&(stack->start)); */
  /* } */
  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  gettime(&(stack->start));
}

