#ifndef INCLDUED_CC_HASHTABLE_H
#define INCLUDED_CC_HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Function height
  int32_t height;

  // Return address that identifies call site
  uintptr_t rip;

  // Work associated with rip
  uint64_t wrk;

  // Span associated with rip
  uint64_t spn;

} cc_hashtable_entry_t;

// Structure for making a linked list of cc_hashtable entries
typedef struct cc_hashtable_list_el_t {
  // Hashtable entry data
  cc_hashtable_entry_t entry;

  // Pointer to next entry in table
  struct cc_hashtable_list_el_t* next;

} cc_hashtable_list_el_t;

// Structure for the hashtable
typedef struct {
  // Lg of capacity of hash table
  int lg_capacity;

  // Number of elements in list
  int list_size;

  // Number of elements in table
  int table_size;

  // Linked list of entries to add to hashtable
  cc_hashtable_list_el_t *head;
  cc_hashtable_list_el_t *tail;

  // Entries of the hash table
  cc_hashtable_entry_t entries[0];

} cc_hashtable_t;
  

/**
 * Forward declaration of nonstatic methods
 */
bool empty_entry_p(const cc_hashtable_entry_t *entry);
cc_hashtable_t* cc_hashtable_create(void);
void clear_cc_hashtable(cc_hashtable_t *tab);
void flush_cc_hashtable(cc_hashtable_t **tab);
bool add_to_cc_hashtable(cc_hashtable_t **tab,
			 int32_t height, uintptr_t rip,
			 uint64_t wrk, uint64_t spn);
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left,
				  cc_hashtable_t **right);

/*************************************************************************/

static int min(int a, int b) {
  return a < b ? a : b;
}

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
const int START_LG_CAPACITY = 2;

// Threshold fraction of table size that can be in the linked list.
const int LG_FRAC_SIZE_THRESHOLD = 0;

// Torlate entries being displaced by a constant amount
const size_t MAX_DISPLACEMENT = 1024 / sizeof(cc_hashtable_entry_t);


// Use a standardish trick from hardware caching to hash rip-height
// pairs.
const uint64_t TAG_OFFSET = 2;
const uint64_t PRIME = (uint32_t)(-5);
const uint64_t ASEED = 0x8c678e6b;
const uint64_t BSEED = 0x9c16f733;
const int MIX_ROUNDS = 4;

static size_t hash(int32_t height, uintptr_t rip, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  /* uint64_t key = (rip >> TAG_OFFSET) * (height + 1); */
  /* return (size_t)(key & mask); */

  uint64_t key = (uint32_t)(rip >> TAG_OFFSET) + height;
  uint32_t h = (uint32_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }

  /* fprintf(stderr, "hash = %lu, (1 << lg_capacity) = %d\n", */
  /* 	  h & mask, 1 << lg_capacity); */
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
bool empty_entry_p(const cc_hashtable_entry_t *entry) {
  return (0 == entry->rip);
}


// Create an empty hashtable entry
static void make_empty_entry(cc_hashtable_entry_t *entry) {
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

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_entry(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
cc_hashtable_t* cc_hashtable_create(void) {
  return cc_hashtable_alloc(START_LG_CAPACITY);
}


// Helper function to get the entry in tab corresponding to rip.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
static cc_hashtable_entry_t*
get_cc_hashtable_entry_targeted(int32_t height, uintptr_t rip, cc_hashtable_t *tab) {
  
  assert((uintptr_t)NULL != rip);

  cc_hashtable_entry_t *entry = &(tab->entries[hash(height,
						    rip,
						    tab->lg_capacity)]);

  assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    /* fprintf(stderr, "get_cc_hashtable_entry_targeted(): disp = %d\n", disp); */

    if (empty_entry_p(entry) ||
	(entry->rip == rip &&
	 entry->height == height)) {
      break;
    }
    ++entry;
    // Wrap around to the beginning
    if (&(tab->entries[1 << tab->lg_capacity]) == entry) {
      entry = &(tab->entries[0]);
    }
  }

  // Return false if the entry was not found in the target area.
  if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) {
    return NULL;
  }

  return entry;
}


// Return a hashtable with the contents of tab and more capacity.
static cc_hashtable_t* increase_table_capacity(const cc_hashtable_t *tab) {

  /* fprintf(stderr, "calling increase_table_capacity()\n"); */

  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  cc_hashtable_t *new_tab;

  do {
    new_tab = cc_hashtable_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const cc_hashtable_entry_t *old = &(tab->entries[i]);
      if (empty_entry_p(old)) {
	continue;
      }

      /* cc_hashtable_entry_t *new = &(new_tab->entries[hash(old->height, */
      /* 							  old->rip, */
      /* 							  new_tab->lg_capacity)]); */
      /* assert(new >= new_tab->entries && */
      /* 	     new < new_tab->entries + (1 << new_tab->lg_capacity)); */
      
      /* int disp; */
      /* for (disp = 0; disp < (1 << new_tab->lg_capacity); ++disp) { */
      /* 	if (empty_entry_p(new) || */
      /* 	    (new->rip == old->rip && */
      /* 	     new->height == old->height)) { */
      /* 	  break; */
      /* 	} */
      /* 	++new; */
      /* 	// Wrap around to the start of the table */
      /* 	if (&(new_tab->entries[(1 << new_tab->lg_capacity)]) == new) { */
      /* 	  new = &(new_tab->entries[0]); */
      /* 	} */
      /* } */

      /* assert(1 << new_tab->lg_capacity > disp); */

      cc_hashtable_entry_t *new
	= get_cc_hashtable_entry_targeted(old->height, old->rip, new_tab);

      if (NULL == new) {
	++new_lg_capacity;
	free(new_tab);
	break;
      } else {
	assert(empty_entry_p(new));
      
	*new = *old;
	++elements_added;
      }
    }

  } while (elements_added < tab->table_size);

  new_tab->table_size = tab->table_size;

  new_tab->list_size = tab->list_size;
  new_tab->head = tab->head;
  new_tab->tail = tab->tail;

  return new_tab;
}


/* // Helper function to get the entry in tab corresponding to rip. */
/* // Returns a pointer to the entry if it can find a place to store it, */
/* // NULL otherwise. */
/* static cc_hashtable_entry_t* */
/* get_cc_hashtable_entry_h(int32_t height, uintptr_t rip, cc_hashtable_t *tab) { */
  
/*   assert((uintptr_t)NULL != rip); */

/*   // Find entry via chaining */
/*   cc_hashtable_entry_t *entry = &(tab->entries[hash(height, */
/* 						    rip, */
/* 						    tab->lg_capacity)]); */

/*   assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity)); */

/*   int disp; */
/*   for (disp = 0; disp < (1 << tab->lg_capacity); ++disp) { */
/*     /\* fprintf(stderr, "get_cc_hashtable_entry(): disp = %d\n", disp); *\/ */

/*     if (empty_entry_p(entry) || */
/* 	(entry->rip == rip && */
/* 	 entry->height == height)) { */
/*       break; */
/*     } */
/*     ++entry; */
/*     // Wrap around to the beginning of table */
/*     if (&(tab->entries[1 << tab->lg_capacity]) == entry) { */
/*       entry = &(tab->entries[0]); */
/*     } */
/*   } */

/*   // Return false if the entry was not found in the target area. */
/*   if (1 << tab->lg_capacity <= disp) { */
/*     return NULL; */
/*   } */

/*   return entry; */
/* } */


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static cc_hashtable_entry_t*
get_cc_hashtable_entry(int32_t height, uintptr_t rip, cc_hashtable_t **tab) {
  int old_table_cap = 1 << (*tab)->lg_capacity;

  cc_hashtable_entry_t *entry;
  while (NULL == (entry = get_cc_hashtable_entry_targeted(height, rip, *tab))) {

    cc_hashtable_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free(*tab);
    *tab = new_tab;
  }
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_cc_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
  return entry;
}


static void flush_cc_hashtable_list(cc_hashtable_t **tab) {
  // Flush list into table
  cc_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    cc_hashtable_entry_t *entry = &(lst_entry->entry);

    /* fprintf(stderr, "inserting entry %lx:%d into table\n", entry->rip, entry->height); */
    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    cc_hashtable_entry_t *tab_entry;
    /* int attempts = 0; */
    /* do { */
    /*   tab_entry = get_cc_hashtable_entry_h(entry->height, */
    /* 					   entry->rip, */
    /* 					   *tab); */
    /*   if (NULL != tab_entry) { */
    /* 	break; */
    /*   } */

    /*   ++attempts; */

    /*   cc_hashtable_t *new_tab = cc_hashtable_alloc((*tab)->lg_capacity + 1); */
    /*   new_tab->table_size = (*tab)->table_size; */

    /*   for (size_t i = 0; i < (1 << (*tab)->lg_capacity); ++i) { */
    /* 	const cc_hashtable_entry_t *old_entry = &((*tab)->entries[i]); */
    /* 	if (empty_entry_p(old_entry)) { */
    /* 	  continue; */
    /* 	} */
    /* 	cc_hashtable_entry_t *new_entry */
    /* 	  = get_cc_hashtable_entry_h(old_entry->height, */
    /* 				     old_entry->rip, */
    /* 				     new_tab); */
	
    /* 	assert(NULL != new_entry); */

    /* 	assert(empty_entry_p(new_entry)); */
	
    /* 	*new_entry = *old_entry; */
    /*   } */
      
    /*   free(*tab); */
    /*   *tab = new_tab; */
    /* } while (1); */
    /* if (attempts > 2) { */
    /*   fprintf(stderr, "fluch_cc_hashtable_list: total resize attempts %d\n", attempts); */
    /* } */

    tab_entry = get_cc_hashtable_entry(entry->height, entry->rip, tab);

    assert(NULL != tab_entry);

    if (empty_entry_p(tab_entry)) {
      tab_entry->height = entry->height;
      tab_entry->rip = entry->rip;
      tab_entry->wrk = entry->wrk;
      tab_entry->spn = entry->spn;
      ++(*tab)->table_size;
    } else {
      tab_entry->wrk += entry->wrk;
      tab_entry->spn += entry->spn;
    }

    entries_added++;
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
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
			 int32_t height, uintptr_t rip,
			 uint64_t wrk, uint64_t spn) {

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    cc_hashtable_list_el_t *lst_entry =
      (cc_hashtable_list_el_t*)malloc(sizeof(cc_hashtable_list_el_t));

    lst_entry->entry.height = height;
    lst_entry->entry.rip = rip;
    lst_entry->entry.wrk = wrk;
    lst_entry->entry.spn = spn;
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
      /* fprintf(stderr, "calling flush_cc_hashtable_list()\n"); */
      assert((*tab)->head != NULL);
      flush_cc_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    cc_hashtable_entry_t *entry = get_cc_hashtable_entry(height, rip, tab);

    if (NULL == entry) {
      return false;
    }
  
    if (empty_entry_p(entry)) {
      entry->height = height;
      entry->rip = rip;
      entry->wrk = wrk;
      entry->spn = spn;
      ++(*tab)->table_size;
    } else {
      entry->wrk += wrk;
      entry->spn += spn;
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

  if ((*left)->list_size > 0 &&
      (*left)->list_size + (*left)->table_size
      >= (1 << ((*left)->lg_capacity - LG_FRAC_SIZE_THRESHOLD))) {
    /* fprintf(stderr, "add_cc_hashtables: flush_cc_hashtable_list(%p)\n", left); */
    flush_cc_hashtable_list(left);
  }

  cc_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_entry_p(r_entry)) {

      l_entry = get_cc_hashtable_entry(r_entry->height, r_entry->rip, left);

      assert (NULL != l_entry);

      if (empty_entry_p(l_entry)) {
	l_entry->rip = r_entry->rip;
	l_entry->height = r_entry->height;
	l_entry->wrk = r_entry->wrk;
	l_entry->spn = r_entry->spn;
	++(*left)->table_size;
      } else {
	l_entry->wrk += r_entry->wrk;
	l_entry->spn += r_entry->spn;
      }
    }
  }

  return *left;
}

// Clear all entries in tab.
void clear_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  cc_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }
  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_entry(&(tab->entries[i]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  cc_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  free(tab);
}

#endif
