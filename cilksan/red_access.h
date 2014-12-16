/* -*- Mode: C++ -*- */

#ifndef _RED_ACCESS_H
#define _RED_ACCESS_H

#include <iostream>
#include <inttypes.h>

#include "debug_util.h"
#include "disjointset.h"
#include "viewread_bags.h"

typedef struct RedAccess_t {

  // the function containing the access
  DisjointSet_t<Bag_t *> *func;
  uint64_t rip;

  RedAccess_t(DisjointSet_t<Bag_t *> *_func, uint64_t _rip)
      : func(_func), rip(_rip)
  { }

} RedAccess_t;


void report_race(uint64_t first_inst, uint64_t second_inst, 
                 uint64_t addr) {
  
}
#endif  // _RED_ACCESS_H
