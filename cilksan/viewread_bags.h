/* -*- Mode: C++ -*- */

#ifndef _VIEWREAD_BAGS_H
#define _VIEWREAD_BAGS_H

#include <assert.h>
#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <inttypes.h>

#include "debug_util.h"
#include "disjointset.h"
#include "cilksan_internal.h"


enum BagType_t { SS, SP, P };

typedef struct Bag_t Bag_t;

struct Bag_t {
  BagType_t type;
  union {
    // Data when _type == SS
    struct {
      uint64_t func_id;
      uint64_t num_local_spawns;
      uint64_t num_ancestor_spawns;
      Bag_t* parent;
    };
    // Data when _type == SP or P
    struct {
      Bag_t* sib;
    };
  };

  Bag_t(void) { }
  ~Bag_t(void) { }

  inline bool is_SSBag(void) { return SS == type; }
  inline bool is_SPBag(void) { return SP == type; }
  inline bool is_PBag(void) { return P == type; }

  inline uint64_t get_func_id(void) {
    if (SS == type) {
      return func_id;
    } else {
      return sib->func_id;
    }
  }
};

Bag_t* make_SSBag(uint64_t _func_id, unit64_t _num_ancestor_spawns,
                  Bag_t *_parent) {
  Bag_t *new_bag = (Bag_t *)malloc(sizeof(Bag_t));
  new_bag->type = SS;
  new_bag->func_id = _func_id;
  new_bag->num_local_spawns = 0;
  new_bag->num_ancestor_spawns = _num_ancestor_spawns;
  new_bag->parent = _parent;
  return new_bag;
}

Bag_t* make_SPBag(Bag_t *_sib) {
  Bag_t *new_bag = (Bag_t *)malloc(sizeof(Bag_t));
  new_bag->type = SP;
  new_bag->sib = _sib;
  return new_bag;
}

Bag_t* make_PBag(Bag_t *_sib) {
  Bag_t *new_bag = (Bag_t *)malloc(sizeof(Bag_t));
  new_bag->type = P;
  new_bag->sib = _sib;
  return new_bag;
}

#endif  // _VIEWREAD_BAGS_H
