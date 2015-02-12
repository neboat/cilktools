#include "cc_hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "call_sites.h"

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
static const int START_LG_CAPACITY = 1;

/* // Threshold fraction of table size that can be in the linked list. */
/* static const int LG_FRAC_SIZE_THRESHOLD = 0; */
static const int TABLE_CONSTANT = 1;

/* // Torlate entries being displaced by a constant amount */
/* /\* static const size_t MAX_DISPLACEMENT = 1024 / sizeof(cc_hashtable_entry_t); *\/ */
/* static const size_t MAX_DISPLACEMENT = 16; */

int MIN_CAPACITY = 1;

extern call_site_table_t *call_site_table;
cc_hashtable_list_el_t *ll_free_list = NULL;

// Use a standardish trick from hardware caching to hash rip-height
// pairs.
/* static const uint64_t TAG_OFFSET = 0; */
/* static const uint64_t PRIME = (uint32_t)(-5); */
/* static const uint64_t ASEED = 0x8c678e6b; */
/* static const uint64_t BSEED = 0x9c16f733; */
/* static const int MIX_ROUNDS = 4; */

/* static inline size_t hash(uintptr_t rip, int lg_capacity) { */
/*   uint64_t mask = (1 << lg_capacity) - 1; */
/*   /\* uint64_t key = (rip >> TAG_OFFSET) * (height + 1); *\/ */
/*   /\* return (size_t)(key & mask); *\/ */

/*   /\* uint64_t key = (uint32_t)(rip >> TAG_OFFSET) + height; *\/ */
/*   uint64_t key = (uint32_t)(rip >> TAG_OFFSET); */
/*   uint64_t h = (uint64_t)((ASEED * key + BSEED) % PRIME); */
/*   for (int i = 0; i < MIX_ROUNDS; ++i) { */
/*     h = h * (2 * h + 1); */
/*     h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2)); */
/*   } */
/*   return (size_t)(h & mask); */
/* } */

static inline uint32_t index(uintptr_t call_site) {
  call_site_record_t *cs_record
      = get_call_site_record_const(call_site, call_site_table);
  return cs_record->index;
}

// Return true if this entry is empty, false otherwise.
bool empty_cc_entry_p(const cc_hashtable_entry_t *entry) {
  return (0 == entry->rip);
}


// Create an empty hashtable entry
static void make_empty_cc_entry(cc_hashtable_entry_t *entry) {
  entry->rip = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
static cc_hashtable_t* cc_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  cc_hashtable_t *table =
    (cc_hashtable_t*)malloc(sizeof(cc_hashtable_t)
			    + (capacity * sizeof(cc_hashtable_entry_t)));

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;

  /* for (size_t i = 0; i < capacity; ++i) { */
  /*   make_empty_cc_entry(&(table->entries[i])); */
  /* } */

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
cc_hashtable_t* cc_hashtable_create(void) {
  cc_hashtable_t *tab = cc_hashtable_alloc(START_LG_CAPACITY);
  for (size_t i = 0; i < (1 << START_LG_CAPACITY); ++i) {
    make_empty_cc_entry(&(tab->entries[i]));
  }
  return tab;
}

static inline
int can_override_entry(cc_hashtable_entry_t *entry, uintptr_t new_rip) {
  // used to be this:
  // entry->rip == new_rip && entry->height == new_height
  return (entry->rip == new_rip);
}

static inline
void combine_entries(cc_hashtable_entry_t *entry,
                     const cc_hashtable_entry_t *entry_add) {
  entry->is_recursive |= entry_add->is_recursive;
  entry->local_wrk += entry_add->local_wrk;
  entry->local_spn += entry_add->local_spn;
  entry->local_count += entry_add->local_count;
  entry->wrk += entry_add->wrk;
  entry->spn += entry_add->spn;
  entry->count += entry_add->count;
  entry->top_wrk += entry_add->top_wrk;
  entry->top_spn += entry_add->top_spn;
  entry->top_count += entry_add->top_count;

  /* if ((INT32_MAX == entry->depth && INT32_MAX == entry_add->depth) || */
  /*     (INT32_MAX > entry->depth && INT32_MAX > entry_add->depth)) { */
  /*   // entry and entry_add are both leaf call sites or both recursive */
  /*   // call sites.  add them together */
  /*   entry->wrk += entry_add->wrk; */
  /*   entry->spn += entry_add->spn; */
  /*   entry->local_wrk += entry_add->local_wrk; */
  /*   entry->local_spn += entry_add->local_spn; */
  /*   entry->count += 1; */
  /* } else if (INT32_MAX == entry->depth && INT32_MAX > entry_add->depth) { */
  /*   // entry_add is recursive and should replace non-recursive entry */
  /*   uint64_t old_local_wrk = entry->local_wrk; */
  /*   uint64_t old_local_spn = entry->local_spn; */
  /*   uint64_t old_count = entry->count; */
  /*   *entry = *entry_add; */
  /*   entry->local_wrk += old_local_wrk; */
  /*   entry->local_spn += old_local_spn; */
  /*   entry->count = old_count + 1; */
  /* } else { */
  /*   // entry is recursive and entry_add is leaf; ignore entry_add */
  /*   entry->local_wrk += entry_add->local_wrk; */
  /*   entry->local_spn += entry_add->local_spn; */
  /*   entry->count += 1; */
  /* } */

  /* if (entry->depth == entry_add->depth) { */
  /*   // Add entries if they have the same depth */
  /*   entry->wrk += entry_add->wrk; */
  /*   entry->spn += entry_add->spn; */
  /*   entry->local_wrk += entry_add->local_wrk; */
  /*   entry->local_spn += entry_add->local_spn; */
  /*   entry->count += 1; */
  /* } else if (entry->depth > entry_add->depth) { */
  /*   // replace only if the entry to add has smaller depth */
  /*   uint64_t old_local_wrk = entry->local_wrk; */
  /*   uint64_t old_local_spn = entry->local_spn; */
  /*   uint64_t old_count = entry->count; */
  /*   *entry = *entry_add; */
  /*   entry->local_wrk += old_local_wrk; */
  /*   entry->local_spn += old_local_spn; */
  /*   entry->count = old_count + 1; */
  /* } else { */
  /*   entry->local_wrk += entry_add->local_wrk; */
  /*   entry->local_spn += entry_add->local_spn; */
  /*   entry->count += 1; */
  /* } */
}

// Helper function to get the entry in tab corresponding to rip.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
cc_hashtable_entry_t*
get_cc_hashtable_entry_const(uintptr_t rip, cc_hashtable_t *tab) {

  assert((uintptr_t)NULL != rip);

  uint32_t cs_index = index(rip);
  if (cs_index < (1 << tab->lg_capacity)) {
    return &(tab->entries[cs_index]);
  }
  return NULL;

  /* cc_hashtable_entry_t *entry = &(tab->entries[hash(rip, tab->lg_capacity)]); */

  /* assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity)); */

  /* int disp; */
  /* for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) { */
  /*   if (empty_cc_entry_p(entry) || can_override_entry(entry, rip)) { */
  /*     break; */
  /*   } */
  /*   ++entry; */
  /*   // Wrap around to the beginning */
  /*   if (&(tab->entries[1 << tab->lg_capacity]) == entry) { */
  /*     entry = &(tab->entries[0]); */
  /*   } */
  /* } */

  /* // Return false if the entry was not found in the target area. */
  /* if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) { */
  /*   return NULL; */
  /* } */

  /* return entry; */
}


// Return a hashtable with the contents of tab and more capacity.
static cc_hashtable_t* increase_table_capacity(const cc_hashtable_t *tab) {

  int new_lg_capacity;
  if ((1 << tab->lg_capacity) < MIN_CAPACITY) {
    uint32_t x = MIN_CAPACITY - 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    new_lg_capacity = __builtin_ctz(x + 1);
  } else {
    fprintf(stderr, "this should not be reachable\n");
    new_lg_capacity = tab->lg_capacity + 1;
  }
  int elements_added;
  cc_hashtable_t *new_tab;

  new_tab = cc_hashtable_alloc(new_lg_capacity);
  size_t i = 0;
  elements_added = 0;

  for (i = 0; i < (1 << tab->lg_capacity); ++i) {
    new_tab->entries[i] = tab->entries[i];
  }

  for ( ; i < (1 << new_tab->lg_capacity); ++i) {
    make_empty_cc_entry(&(new_tab->entries[i]));
  }

  /* do { */
  /*   new_tab = cc_hashtable_alloc(new_lg_capacity); */
  /*   elements_added = 0; */

  /*   for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) { */
  /*     const cc_hashtable_entry_t *old = &(tab->entries[i]); */
  /*     if (empty_cc_entry_p(old)) { */
  /*       continue; */
  /*     } */

  /*     cc_hashtable_entry_t *new */
  /*       = get_cc_hashtable_entry_const(old->rip, new_tab); */

  /*     if (NULL == new) { */
  /*       ++new_lg_capacity; */
  /*       free(new_tab); */
  /*       break; */
  /*     } else { */
  /*       assert(empty_cc_entry_p(new)); */
      
  /*       *new = *old; */
  /*       ++elements_added; */
  /*     } */
  /*   } */

  /* } while (elements_added < tab->table_size); */

  new_tab->table_size = tab->table_size;

  new_tab->list_size = tab->list_size;
  new_tab->head = tab->head;
  new_tab->tail = tab->tail;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static cc_hashtable_entry_t*
get_cc_hashtable_entry(uintptr_t rip, cc_hashtable_t **tab) {
#if DEBUG_RESIZE
  int old_table_cap = 1 << (*tab)->lg_capacity;
#endif

  cc_hashtable_entry_t *entry;
  while (NULL == (entry = get_cc_hashtable_entry_const(rip, *tab))) {

    cc_hashtable_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free(*tab);
    *tab = new_tab;
  }
#if DEBUG_RESIZE
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_cc_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
#endif
  return entry;
}

static inline
void flush_cc_hashtable_list(cc_hashtable_t **tab) {
  // Flush list into table
  cc_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    cc_hashtable_entry_t *entry = &(lst_entry->entry);

    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    cc_hashtable_entry_t *tab_entry;

    tab_entry = get_cc_hashtable_entry(entry->rip, tab);
    assert(NULL != tab_entry);
    assert(empty_cc_entry_p(tab_entry) || can_override_entry(tab_entry, entry->rip));

    if (empty_cc_entry_p(tab_entry)) {
      // the compiler will do a struct copy
      *tab_entry = *entry;
      ++(*tab)->table_size;
    } else {
      combine_entries(tab_entry, entry);
    }

    entries_added++;
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    // free(lst_entry);
    lst_entry = next_lst_entry;
  }

  if (NULL != (*tab)->head) {
    assert(NULL != (*tab)->tail);
    (*tab)->tail->next = ll_free_list;
    ll_free_list = (*tab)->head;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_cc_hashtable(cc_hashtable_t **tab) {
  if (NULL != (*tab)->head)
    flush_cc_hashtable_list(tab);
  else
    assert((*tab)->list_size == 0);
}

// Add the given cc_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_cc_hashtable(cc_hashtable_t **tab,
			 /* int32_t depth, bool is_top_level, */
                         InstanceType_t inst_type,
                         FunctionType_t func_type, uintptr_t rip, 
			 uint64_t wrk, uint64_t spn,
                         uint64_t local_wrk, uint64_t local_spn) {

  /* if ((*tab)->list_size + (*tab)->table_size */
  /*     < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) { */
  if ((1 << (*tab)->lg_capacity) < MIN_CAPACITY &&
      (*tab)->list_size < MIN_CAPACITY / TABLE_CONSTANT) {
    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.
    cc_hashtable_list_el_t *lst_entry;
    if (NULL != ll_free_list) {
      lst_entry = ll_free_list;
      ll_free_list = ll_free_list->next;
    } else {
      lst_entry = (cc_hashtable_list_el_t*)malloc(sizeof(cc_hashtable_list_el_t));
    }

    lst_entry->entry.is_recursive = (0 != (RECURSIVE & inst_type));
    lst_entry->entry.func_type = func_type;
    lst_entry->entry.rip = rip;
    lst_entry->entry.wrk = wrk;
    lst_entry->entry.spn = spn;
    lst_entry->entry.count = (0 != (RECORD & inst_type));
    if (TOP & inst_type) {
      lst_entry->entry.top_wrk = wrk;
      lst_entry->entry.top_spn = spn;
      assert(0 != wrk);
      lst_entry->entry.top_count = 1;
    } else {
      lst_entry->entry.top_wrk = 0;
      lst_entry->entry.top_spn = 0;
      lst_entry->entry.top_count = 0;
    }      
    lst_entry->entry.local_wrk = local_wrk;
    lst_entry->entry.local_spn = local_spn;
    lst_entry->entry.local_count = 1;
    lst_entry->next = NULL;

    if (NULL == (*tab)->tail) {
      (*tab)->tail = lst_entry;
      assert(NULL == (*tab)->head);
      (*tab)->head = lst_entry;
    } else {
      (*tab)->tail->next = lst_entry;
      (*tab)->tail = lst_entry;
    }

    // Increment list size
    ++(*tab)->list_size;

  } else {
    
    if ((*tab)->list_size > 0) {
      assert((*tab)->head != NULL);
      flush_cc_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    cc_hashtable_entry_t *entry = get_cc_hashtable_entry(rip, tab);
    assert(empty_cc_entry_p(entry) || can_override_entry(entry, rip));

    if (NULL == entry) {
      return false;
    }
  
    if (empty_cc_entry_p(entry)) {
      entry->is_recursive = (0 != (RECURSIVE & inst_type));
      entry->func_type = func_type;
      entry->rip = rip;
      entry->wrk = wrk;
      entry->spn = spn;
      entry->count = (0 != (RECORD & inst_type));
      if (TOP & inst_type) {
        entry->top_wrk = wrk;
        entry->top_spn = spn;
        entry->top_count = 1;
      } else {
        entry->top_wrk = 0;
        entry->top_spn = 0;
        entry->top_count = 0;
      }
      entry->local_wrk = local_wrk;
      entry->local_spn = local_spn;
      entry->local_count = 1;
      ++(*tab)->table_size;
    } else {
      entry->is_recursive |= (0 != (RECURSIVE & inst_type));
      entry->local_wrk += local_wrk;
      entry->local_spn += local_spn;
      entry->local_count += 1;
      entry->wrk += wrk;
      entry->spn += spn;
      entry->count += (0 != (RECORD & inst_type));
      if (TOP & inst_type) {
        entry->top_wrk += wrk;
        entry->top_spn += spn;
        entry->top_count += 1;
      }
      /* if ((INT32_MAX == entry->depth && INT32_MAX == depth) /\* Both leaves *\/ || */
      /*     (INT32_MAX > entry->depth && INT32_MAX > depth) /\* Both recursive *\/) { */
      /*   // Existing entry and new data are either both leaves or both */
      /*   // recursive.  Add them. */
      /*   entry->wrk += wrk; */
      /*   entry->spn += spn; */
      /*   entry->count += 1; */
      /* } else if (INT32_MAX == entry->depth && INT32_MAX > depth) { */
      /*   // New entry is recursive and should override old leaf entry */
      /*   assert(can_override_entry(entry, rip)); */
      /*   entry->depth = depth; */
      /*   entry->wrk = wrk; */
      /*   entry->spn = spn; */
      /* } else { */
      /*   // New entry is leaf, but existing entry is recursive.  Ignore */
      /*   // new entry. */
      /* } */
      /* if (entry->depth == depth) {  // same rip and same depth */
      /*   entry->wrk += wrk; */
      /*   entry->spn += spn; */
      /* } else if (entry->depth > depth) {  // replace only if new entry has smaller depth */
      /*   assert(can_override_entry(entry, rip)); */
      /*   entry->depth = depth; */
      /*   entry->wrk = wrk; */
      /*   entry->spn = spn; */
      /* } */
    }
  }

  return true;
}

// Add the cc_hashtable **right into the cc_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left, cc_hashtable_t **right) {

  /* fprintf(stderr, "add_cc_hashtables(%p, %p)\n", left, right); */

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    cc_hashtable_t *tmp = *left;
    *left = *right;
    *right = tmp;
  }

  /* fprintf(stderr, "\tleft list_size = %d, right list_size = %d\n", */
  /* 	  (*left)->list_size, (*right)->list_size); */

  if (NULL != (*left)->tail) {
    (*left)->tail->next = (*right)->head;
  } else {
    assert(NULL == (*left)->head);
    (*left)->head = (*right)->head;
    // XXX: Why not just do this?  Does it matter if both are NULL?
    /* (*left)->tail = (*right)->tail; */
  }
  (*left)->list_size += (*right)->list_size;
  if (NULL != (*right)->tail) {
    (*left)->tail = (*right)->tail;
  }
  (*right)->head = NULL;
  (*right)->tail = NULL;

  /* fprintf(stderr, "list_size = %d, table_size = %d, lg_capacity = %d\n", */
  /* 	  (*left)->list_size, (*left)->table_size, (*left)->lg_capacity); */

  /* if ((*left)->list_size > 0 && */
  /*     (*left)->list_size + (*left)->table_size */
  /*     >= (1 << ((*left)->lg_capacity - LG_FRAC_SIZE_THRESHOLD))) { */
  /*   /\* fprintf(stderr, "add_cc_hashtables: flush_cc_hashtable_list(%p)\n", left); *\/ */
  /*   flush_cc_hashtable_list(left); */
  /* } */

  if ((*left)->list_size >= MIN_CAPACITY / TABLE_CONSTANT) {
    flush_cc_hashtable_list(left);
  }
  cc_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (empty_cc_entry_p(r_entry)) {
      continue;
    }

    l_entry = &((*left)->entries[i]);
    assert(NULL != l_entry);
    assert(empty_cc_entry_p(l_entry) || can_override_entry(l_entry, r_entry->rip));
    if (empty_cc_entry_p(l_entry)) {
      // let the compiler do the struct copy
      *l_entry = *r_entry;
      ++(*left)->table_size;
    } else {
      combine_entries(l_entry, r_entry);
    }
  }

  /* for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) { */
  /*   r_entry = &((*right)->entries[i]); */
  /*   if (!empty_cc_entry_p(r_entry)) { */

  /*     l_entry = get_cc_hashtable_entry(r_entry->rip, left); */
  /*     assert (NULL != l_entry); */
  /*     assert(empty_cc_entry_p(l_entry) || can_override_entry(l_entry, r_entry->rip)); */

  /*     if (empty_cc_entry_p(l_entry)) { */
  /*       // let the compiler do the struct copy */
  /*       *l_entry = *r_entry; */
  /*       ++(*left)->table_size; */
  /*     } else { */
  /*       combine_entries(l_entry, r_entry); */
  /*     } */
  /*   } */
  /* } */

  return *left;
}

// Clear all entries in tab.
void clear_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  /* cc_hashtable_list_el_t *lst_entry = tab->head; */
  /* while (NULL != lst_entry) { */
  /*   cc_hashtable_list_el_t *next_lst_entry = lst_entry->next; */
  /*   free(lst_entry); */
  /*   lst_entry = next_lst_entry; */
  /* } */
  if (NULL != tab->head) {
    assert(NULL != tab->tail);
    tab->tail->next = ll_free_list;
    ll_free_list = tab->head;
  }

  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_cc_entry(&(tab->entries[i]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  /* cc_hashtable_list_el_t *lst_entry = tab->head; */
  /* while (NULL != lst_entry) { */
  /*   cc_hashtable_list_el_t *next_lst_entry = lst_entry->next; */
  /*   free(lst_entry); */
  /*   lst_entry = next_lst_entry; */
  /* } */
  if (NULL != tab->head) {
    assert(NULL != tab->tail);
    tab->tail->next = ll_free_list;
    ll_free_list = tab->head;
  }

  free(tab);
}

bool cc_hashtable_is_empty(const cc_hashtable_t *tab) {
  return tab->table_size == 0 && tab->list_size == 0;
}
