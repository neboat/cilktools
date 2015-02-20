#include "call_sites.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// #include "SFMT-src-1.4.1/SFMT.h"

#ifndef DEBUG_RESIZE
#define DEBUG_RESIZE 0
#endif

/* static int min(int a, int b) { */
/*   return a < b ? a : b; */
/* } */

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
static const int START_LG_CAPACITY = 4;

// Torlate entries being displaced by a constant amount
/* static const size_t MAX_DISPLACEMENT = 64 / sizeof(call_site_record_t); */
/* static const size_t MAX_DISPLACEMENT = 16; */
static const int CALL_SITE_CACHE_SIZE = 8;

// Use a standardish trick from hardware caching to hash rip-height
// pairs.
static const uint64_t TAG_OFFSET = 2;
static const uint64_t PRIME = (uint32_t)(-5);
static const uint64_t ASEED = 0x8c678e6b;
static const uint64_t BSEED = 0x9c16f733;
static const int MIX_ROUNDS = 4;

static size_t hash(uintptr_t call_site, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  uint64_t key = (uint64_t)(call_site >> TAG_OFFSET);
  uint64_t h = (uint64_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
static inline bool empty_record_p(const call_site_record_t *entry) {
  return (0 == entry->call_site);
}


// Create an empty hashtable entry
static void make_empty_call_site_record(call_site_record_t *entry) {
  entry->call_site = (uintptr_t)NULL;
}


// Allocate an empty hash table with 2^lg_capacity entries
static call_site_table_t* call_site_table_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  call_site_table_t *table =
    (call_site_table_t*)malloc(sizeof(call_site_table_t)
                               + (capacity * sizeof(call_site_record_t*)));

  table->lg_capacity = lg_capacity;
  table->table_size = 0;
  table->call_site_cache = NULL;

  for (size_t i = 0; i < capacity; ++i) {
    /* make_empty_call_site_record(&(table->records[i])); */
    table->records[i] = NULL;
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
call_site_table_t* call_site_table_create(void) {
  call_site_table_t *tab = call_site_table_alloc(START_LG_CAPACITY);
  tab->call_site_cache
      = (call_site_cache_el_t*)malloc(sizeof(call_site_cache_el_t)
                                      * CALL_SITE_CACHE_SIZE);

  // Initialize cache
  call_site_cache_el_t *cache_el = tab->call_site_cache;
  for (int i = 0; i < CALL_SITE_CACHE_SIZE - 1; ++i) {
    cache_el->next = cache_el + 1;
    cache_el->record = NULL;
    cache_el = cache_el->next;
  }
  cache_el->next = NULL;
  cache_el->record = NULL;

  return tab;
}

// Helper function to get the entry in tab corresponding to call_site.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
call_site_record_t*
get_call_site_record_const(uintptr_t call_site, call_site_table_t *tab) {

  assert((uintptr_t)NULL != call_site);

  // Search for call site in cache
  call_site_cache_el_t *cache_el = tab->call_site_cache;
  call_site_cache_el_t *last_cache_el = NULL;
  while (NULL != cache_el->record) {
    if (call_site == cache_el->record->call_site) {
      // Move record to front of cache
      if (NULL != last_cache_el) {
        last_cache_el->next = cache_el->next;
        cache_el->next = tab->call_site_cache;
        tab->call_site_cache = cache_el;
      }
      return cache_el->record;
    }
    if (NULL == cache_el->next) {
      // No more elements in cache
      break;
    }
    last_cache_el = cache_el;
    cache_el = cache_el->next;
  }

  // Call site not found in cache.  Hash the call site and search the
  // table.
  call_site_record_t **first_record = &(tab->records[hash(call_site, tab->lg_capacity)]);

  // Scan linked list
  call_site_record_t *record = *first_record;
  call_site_record_t *last_record = NULL;
  while (NULL != record && call_site != record->call_site) {
    last_record = record;
    record = record->next;
  }

  if (NULL == record) {
    // Allocate a new record
    record = (call_site_record_t*)malloc(sizeof(call_site_record_t));
    make_empty_call_site_record(record);
    record->next = *first_record;
    *first_record = record;    
  } else if (NULL != last_record) {
    // Move record to front
    last_record->next = record->next;
    record->next = *first_record;
    *first_record = record;
  }

  // Place record at front of cache
  cache_el->record = record;
  // Move record to front of cache
  if (NULL != last_cache_el) {
    last_cache_el->next = cache_el->next;
    cache_el->next = tab->call_site_cache;
    tab->call_site_cache = cache_el;
  }

  /* int disp; */
  /* for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) { */
  /*   if (empty_record_p(record) || call_site == record->call_site) { */
  /*     break; */
  /*   } */
  /*   ++record; */
  /*   // Wrap around to the beginning */
  /*   if (&(tab->records[1 << tab->lg_capacity]) == record) { */
  /*     record = &(tab->records[0]); */
  /*   } */
  /* } */

  /* // Return false if the record was not found in the target area. */
  /* if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) { */
  /*   return NULL; */
  /* } */

  return record;
}


// Return a hashtable with the contents of tab and more capacity.
static call_site_table_t* increase_table_capacity(const call_site_table_t *tab) {

  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  call_site_table_t *new_tab;

  do {
    new_tab = call_site_table_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      call_site_record_t *old = tab->records[i];
      call_site_record_t *next_old;
      while (NULL != old) {
        next_old = old->next;
        call_site_record_t **new = 
            &(new_tab->records[hash(old->call_site, new_tab->lg_capacity)]);
        old->next = *new;
        *new = old;
        old = next_old;
        ++elements_added;
      }
    }

  } while (elements_added < tab->table_size);

  new_tab->table_size = tab->table_size;
  new_tab->call_site_cache = tab->call_site_cache;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static call_site_record_t*
get_call_site_record(uintptr_t call_site, call_site_table_t **tab) {
  return get_call_site_record_const(call_site, *tab);
/* #if DEBUG_RESIZE */
/*   int old_table_cap = 1 << (*tab)->lg_capacity; */
/* #endif */

/*   call_site_record_t *entry; */
/*   while (NULL == (entry = get_call_site_record_const(call_site, *tab))) { */

/*     call_site_table_t *new_tab = increase_table_capacity(*tab); */

/*     assert(new_tab); */

/*     free(*tab); */
/*     *tab = new_tab; */
/*   } */
/* #if DEBUG_RESIZE */
/*   if (1 << (*tab)->lg_capacity > 2 * old_table_cap) { */
/*     fprintf(stderr, "get_call_site_record: new table capacity %d\n", */
/*     	    1 << (*tab)->lg_capacity); */
/*   } */
/* #endif */
/*   return entry; */
}

// Add the given call_site_record_t data to **tab.  Returns 1 if
// call_site was not previously on the stack, 0 if call_site is
// already in the stack, -1 on error.
int32_t add_to_call_site_table(call_site_table_t **tab,
                               uintptr_t call_site) {

  call_site_record_t *record = get_call_site_record(call_site, tab);
  assert(NULL != record);

  if (NULL == record) {
    return -1;
  }
  
  if (empty_record_p(record)) {
    // Grow table if capacity exceeds 50%
    if ((*tab)->table_size + 1 > (1 << ((*tab)->lg_capacity - 1))) {
      call_site_table_t *new_tab = increase_table_capacity(*tab);
      assert(new_tab);
      free(*tab);
      *tab = new_tab;
      record = get_call_site_record(call_site, tab);
      assert(empty_record_p(record));
    }
    record->call_site = call_site;
    record->on_stack = true;
    record->recursive = false;
    record->index = (*tab)->table_size++;
    return 1;
  }
  assert(record->call_site == call_site);
  if (record->on_stack) {
    record->recursive = true;
    return 0;
  }
  record->on_stack = true;
  return 1;
}
