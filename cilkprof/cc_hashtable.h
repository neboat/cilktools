#ifndef INCLUDED_CC_HASHTABLE_H
#define INCLUDED_CC_HASHTABLE_H

#include <stdbool.h>
#include <inttypes.h>

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Function depth 
  int32_t depth;

  // Return address that identifies call site
  uintptr_t rip;

  // Work associated with rip
  uint64_t wrk;

  // Span associated with rip
  uint64_t spn;

  // Update counts associated with this rip.  This count is
  // independent of depth.
  uint32_t count;

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
 * Exposed hashtable methods
 */
bool empty_cc_entry_p(const cc_hashtable_entry_t *entry);
cc_hashtable_t* cc_hashtable_create(void);
void clear_cc_hashtable(cc_hashtable_t *tab);
void flush_cc_hashtable(cc_hashtable_t **tab);
cc_hashtable_entry_t*
get_cc_hashtable_entry_const(uintptr_t rip, cc_hashtable_t *tab);
bool add_to_cc_hashtable(cc_hashtable_t **tab,
			 int32_t depth, uintptr_t rip,
			 uint64_t wrk, uint64_t spn);
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left,
				  cc_hashtable_t **right);
void free_cc_hashtable(cc_hashtable_t *tab);
bool cc_hashtable_is_empty(const cc_hashtable_t *tab);

#endif
