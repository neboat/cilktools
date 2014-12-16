/* -*- Mode: C++ -*- */

#ifndef _DISJOINTSET_H
#define _DISJOINTSET_H

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

#include "debug_util.h"


template <typename DISJOINTSET_DATA_T>
class DisjointSet_t {
private:
  // the node that initialized this set; const field that does not change  
  DISJOINTSET_DATA_T const _node; 
  // the oldest node representing the set that the _node belongs to 
  DISJOINTSET_DATA_T _set_node; 
  DisjointSet_t *_set_parent;
  uint64_t _rank; // roughly as the height of this node

  /*
   * The only reason we need this function is to ensure that  
   * the _set_node returned for representing this set is the oldest 
   * node in the set.  
   */
  void swap_set_node_with(DisjointSet_t *that) {
    DISJOINTSET_DATA_T tmp;
    tmp = this->_set_node;
    this->_set_node = that->_set_node;
    that->_set_node = tmp;
  }

  /*
   * Links this disjoint set to that disjoint set.
   * Don't need to be public.
   *
   * @param that that disjoint set.
   */
  void link(DisjointSet_t *that) {

    cilksan_assert(that != NULL);
    // link the node with smaller height into the node with larger height
    if (this->_rank > that->_rank) {
      that->_set_parent = this;
    } else {
      this->_set_parent = that;
      if(this->_rank == that->_rank)
        ++that->_rank;
      // because we are linking this into the forest rooted that that, let's
      // swap the nodes in this object and that object to keep the metadata
      // hold in the node consistent.
      this->swap_set_node_with(that);
    }
  }

  /*
   * Finds the set containing this disjoint set element.
   *
   * Note: Performs path compression along the way.
   *       The _set_parent field will be updated after the call.
   */
  DisjointSet_t* find_set() {

    cilksan_assert(this->_set_parent);
    if (this->_set_parent != this) {
      this->_set_parent = this->_set_parent->find_set();
    }
    return this->_set_parent;
  }

public:
  DisjointSet_t(DISJOINTSET_DATA_T node) :
      _node(node),
      _set_node(node),
      _set_parent(this),
      _rank(0) { 

    WHEN_CILKSAN_DEBUG( debug_count++; )
  }

#if CILKSAN_DEBUG
  static long debug_count;
  static uint64_t nodes_created;
  ~DisjointSet_t() {
    debug_count--;
  }
#endif

  DISJOINTSET_DATA_T get_node() {
    return _node;
  }

  DISJOINTSET_DATA_T get_set_node() {
    return find_set()->_set_node;
  }

  /*
   * Unions this disjoint set and that disjoint set.
   *
   * NOTE: implicitly, in order to maintain the oldest _set_node, one
   * should always combine younger set into this set (defined by creation
   * time).  Since we union by rank, we may end up linking this set to 
   * the younger set.  To make sure that we always return the oldest _node 
   * to represent the set, we use an additional _set_node field to keep 
   * track of the oldest node and use that to represent the set.
   *
   * @param that that (younger) disjoint set.
   */
  // Called "combine," because "union" is a reserved keyword in C
  void combine(DisjointSet_t *that) {

    cilksan_assert(that);
    cilksan_assert(this->find_set() != that->find_set());
    this->find_set()->link(that->find_set());
    cilksan_assert(this->find_set() == that->find_set());
  }

};

#endif // #ifndef _DISJOINTSET_H
