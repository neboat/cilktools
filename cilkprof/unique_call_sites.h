#ifndef INCLUDED_UNIQUE_CALL_SITES_H
#define INCLUDED_UNIQUE_CALL_SITES_H

#include <stdbool.h>

// Stack of unique call sites in stack
typedef struct unique_call_site_t {
  bool is_recursive;
  int32_t depth;
  uintptr_t call_site;
  struct unique_call_site_t* prev;
} unique_call_site_t;

// Checks CALL_SITES for ENTER_CALL_SITE.  If found, returns false.
// Otherwise, adds NEW_CALL_SITE to CALL_SITES and return true.
bool enter_call_site(unique_call_site_t **call_sites,
                     uintptr_t call_site, int32_t depth)
{
  /* fprintf(stderr, "enter_call_site(%lx)\n", call_site); */
  // Search for this call site
  unique_call_site_t *entry = *call_sites;
  while (entry) {
    if (call_site == entry->call_site) {
      break;
    }
    entry = entry->prev;
  }

  // We found the call site, so it's not unique
  if (entry) {
    entry->is_recursive = true;
    return false;
  }
  // Add new unique call site
  unique_call_site_t *new_unique_call_site
      = (unique_call_site_t *)malloc(sizeof(unique_call_site_t));
  new_unique_call_site->call_site = call_site;
  new_unique_call_site->depth = depth;
  new_unique_call_site->is_recursive = false;
  new_unique_call_site->prev = *call_sites;
  *call_sites = new_unique_call_site;
  return true;
}

// Finds LEAVE_CALL_SITE in CALL_SITES.  If found with matching DEPTH,
// then removes LEAVE_CALL_SITE from CALL_SITES.  Returns true if this
// function is recursive, false otherwise.
bool exit_call_site(unique_call_site_t **call_sites,
                    uintptr_t call_site, int32_t depth)
{
  // Search for this call site
  unique_call_site_t *entry = *call_sites;
  unique_call_site_t *last_entry = NULL;
  while (entry) {
    if (call_site == entry->call_site) {
      break;
    }
    last_entry = entry;
    entry = entry->prev;
  }

  assert(entry);
  bool is_recursive = entry->is_recursive;

  if (depth == entry->depth) {
    // Remove this call site from call_sites
    if (last_entry) {
      last_entry->prev = entry->prev;
    } else {
      *call_sites = entry->prev;
    }
    free(entry);
  } else {
    assert(entry->is_recursive);
  }

  return is_recursive;
}

#endif
