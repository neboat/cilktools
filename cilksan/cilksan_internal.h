#ifndef __CILKSAN_INTERNAL_H__
#define __CILKSAN_INTERNAL_H__

#include <cstdio>
#include <vector>
#include <map>

#define UNINIT_STACK_PTR ((uint64_t)0LL)
#define UNINIT_VIEW_ID ((uint64_t)0LL)

#include "cilksan.h"

#define BT_OFFSET 1
#define BT_DEPTH 2

// The context in which the access is made; user = user code, 
// update = update methods for a reducer object; reduce = reduce method for 
// a reducer object 
enum AccContextType_t { USER = 1, UPDATE = 2, REDUCE = 3 };
// W stands for write, R stands for read
enum RaceType_t { RW_RACE = 1, WW_RACE = 2, WR_RACE = 3 };


typedef struct RaceInfo_t {
  uint64_t first_inst;   // instruction addr of the first access
  uint64_t second_inst;  // instruction addr of the second access
  uint64_t addr;         // addr of memory location that got raced on
  enum RaceType_t type; // type of race

  RaceInfo_t(uint64_t _first, uint64_t _second,
             uint64_t _addr, enum RaceType_t _type) :
    first_inst(_first), second_inst(_second), addr(_addr), type(_type)
    { }

  bool is_equivalent_race(const struct RaceInfo_t& other) const {
    /*
    if( (type == other.type && 
         first_inst == other.first_inst && second_inst == other.second_inst) ||
        (first_inst == other.second_inst && second_inst == other.first_inst &&
         ((type == RW_RACE && other.type == WR_RACE) ||
          (type == WR_RACE && other.type == RW_RACE))) ) {
      return true;
    } */
    // Angelina: It turns out that, Cilkscreen does not care about the race
    // types.  As long as the access instructions are the same, it's considered
    // as a duplicate.
    if( (first_inst == other.first_inst && second_inst == other.second_inst) ||
        (first_inst == other.second_inst && second_inst == other.first_inst) ) {
      return true;
    }
    return false;
  }
} RaceInfo_t;

// public functions
void cilksan_init(); 
void cilksan_deinit(); 
void cilksan_do_enter_begin(); 
void cilksan_do_enter_helper_begin(); 
void cilksan_do_enter_end(struct __cilkrts_worker *w, uint64_t stack_ptr);
void cilksan_do_detach_begin();
void cilksan_do_detach_end();
void cilksan_do_sync_begin(); 
void cilksan_do_sync_end(); 
void cilksan_do_return(); 
void cilksan_do_leave_begin();
void cilksan_do_leave_end();
void cilksan_do_leave_stolen_callback();

void cilksan_do_read(uint64_t inst_addr, uint64_t addr, size_t len); 
void cilksan_do_write(uint64_t inst_addr, uint64_t addr, size_t len); 
void cilksan_clear_shadow_memory(size_t start, size_t end);
// void cilksan_do_function_entry(uint64_t an_address);
// void cilksan_do_function_exit();
#endif // __CILKSAN_INTERNAL_H__
