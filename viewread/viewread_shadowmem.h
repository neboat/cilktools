#ifndef INCLUDED_VIEWREAD_SHADOWMEM_H
#define INCLUDED_VIEWREAD_SHADOWMEM_H

#include <inttypes.h>

#include <assert.h>

typedef struct DisjointSet_t DisjointSet_t;
typedef uintptr_t reducer_t;

typedef struct {
  // ID of reducer
  reducer_t reducer;
  // Address of read or set call
  uintptr_t reader;
  // View ID when the reducer was last read
  uint64_t spawns;
  // Pointer to disjoint set node of reader
  DisjointSet_t *node;
} reader_t;

typedef struct {
  int lg_capacity;
  int size;
  reader_t entries[0];
} shadowmem_t;

static int min(int a, int b) {
  return (a < b) ? a : b;
}

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
const int START_LG_CAPACITY = 1;

// Torlate entries being displaced by a constant amount
const size_t MAX_DISPLACEMENT = 1024 / sizeof(reader_t);

// Use a standardish trick from hardware caching to hash call_site-height
// pairs.
const uint64_t TAG_OFFSET = 2;
const uint64_t PRIME = (uint32_t)(-5);
const uint64_t ASEED = 0x8c678e6b;
const uint64_t BSEED = 0x9c16f733;
const int MIX_ROUNDS = 4;

static size_t hash(reducer_t reducer, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  uint64_t key = (uint32_t)reducer;
  uint32_t h = (uint32_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
bool empty_reader_p(const reader_t *entry) {
  return (0 == entry->reducer);
}


// Create an empty hashtable entry
static void make_empty_reader(reader_t *entry) {
  entry->reducer = 0;
}

shadowmem_t* shadowmem_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  shadowmem_t *table =
      (shadowmem_t*)malloc(sizeof(shadowmem_t)
                           + (capacity * sizeof(reader_t)));

  table->lg_capacity = lg_capacity;
  table->size = 0;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_reader(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty shadowmemory.  Returns a pointer to the hashtable
// created.
shadowmem_t* shadowmem_create(void) {
  return shadowmem_alloc(START_LG_CAPACITY);
}


// Helper function to get the entry in tab corresponding to reducer.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
static reader_t*
get_reader_helper(reducer_t reducer, shadowmem_t *tab) {
  
  assert((reducer_t)NULL != reducer);

  reader_t *entry = &(tab->entries[hash(reducer, tab->lg_capacity)]);

  assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    /* fprintf(stderr, "get_span_hashtable_entry_targeted(): disp = %d\n", disp); */

    if (empty_reader_p(entry) || entry->reducer == reducer) {
      break;
    }
    ++entry;
    // Wrap around to the beginning
    if (&(tab->entries[1 << tab->lg_capacity]) == entry) {
      entry = &(tab->entries[0]);
    }
  }

  // Return false if the entry was not found in the target area.
  if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)) <= disp) {
    return NULL;
  }

  return entry;
}


// Return a hashtable with the contents of tab and more capacity.
static shadowmem_t* increase_table_capacity(const shadowmem_t *tab) {
  /* fprintf(stderr, "calling increase_table_capacity()\n"); */
  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  shadowmem_t *new_tab;

  do {
    new_tab = shadowmem_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const reader_t *old = &(tab->entries[i]);
      if (empty_reader_p(old)) {
	continue;
      }

      reader_t *new = get_reader_helper(old->reducer, new_tab);

      if (NULL == new) {
	++new_lg_capacity;
	free(new_tab);
	break;
      } else {
	assert(empty_reader_p(new));
      
	*new = *old;
	++elements_added;
      }
    }

  } while (elements_added < tab->size);

  new_tab->size = tab->size;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
reader_t*
get_reader(reducer_t reducer, shadowmem_t **tab) {
  int old_table_cap = 1 << (*tab)->lg_capacity;

  reader_t *entry;
  while (NULL == (entry = get_reader_helper(reducer, *tab))) {
    shadowmem_t *new_tab = increase_table_capacity(*tab);
    assert(new_tab);
    free(*tab);
    *tab = new_tab;
  }
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_span_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
  return entry;
}


// Update the shadowmem_t **tab.  Returns true if the **tab was
// successfully added, false otherwise.
bool update_shadowmem(shadowmem_t **tab, reducer_t reducer,
                      uintptr_t reader, uint64_t spawns,
                      DisjointSet_t *node) {
  reader_t *entry = get_reader(reducer, tab);

  if (NULL == entry) {
    return false;
  }
  
  if (empty_reader_p(entry)) {
    entry->reducer = reducer;
    entry->reader = reader;
    entry->spawns = spawns;
    entry->node = node;
    ++(*tab)->size;
  } else {
    entry->reader = reader;
    entry->spawns = spawns;
    entry->node = node;
  }

  return true;
}

#endif
