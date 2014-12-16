/* -*- Mode: C++ -*- */

#ifndef _SPBAG_H
#define _SPBAG_H

#include <assert.h>
#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <inttypes.h>

#include "debug_util.h"
#include "disjointset.h"
#include "cilksan_internal.h"


class SPBagInterface {
public: 
  // Note to self: base class must declare a virtual destructor; it does not 
  // have to be pure and must provide a definition.
  // http://stackoverflow.com/questions/461203/when-to-use-virtual-destructors
  // <stackoverflow>/3336499/virtual-desctructor-on-pure-abstract-base-class
  virtual ~SPBagInterface() { }
  virtual bool is_SBag() = 0;
  virtual bool is_PBag() = 0;
  virtual uint64_t get_func_id() = 0;
  virtual uint64_t get_view_id() = 0;
  virtual uint64_t get_rsp() = 0;
  virtual void set_rsp(uint64_t stack_ptr) = 0;
  // virtual std::string get_call_context() = 0;
};


class SBag_t : public SPBagInterface {
private:
  uint64_t _func_id;
  uint64_t _view_id;
  // std::string _func_name;
  // SBag of the parent function (whether this function is called or spawned) 
  SPBagInterface *_parent;
  uint64_t _stack_ptr;

  SBag_t() {} // disable default constructor

public: 
  SBag_t(uint64_t id, SPBagInterface *parent) :
      _func_id(id), _view_id(UNINIT_VIEW_ID),
      // _func_name(name),
      _parent(parent),
      _stack_ptr(UNINIT_STACK_PTR) { 

    WHEN_CILKSAN_DEBUG( debug_count++; )
  }

#if CILKSAN_DEBUG
  static long debug_count;
  ~SBag_t() {
    debug_count--;
  }
#endif

  bool is_SBag() { return true; }
  bool is_PBag() { return false; }

  uint64_t get_func_id() { return _func_id; }
  uint64_t get_view_id() { /*racedetector_assert(0);*/ return _view_id; }

  uint64_t get_rsp() { 
    cilksan_assert(_stack_ptr != UNINIT_STACK_PTR); 
    return _stack_ptr; 
  }
  void set_rsp(uint64_t stack_ptr) { _stack_ptr = stack_ptr; }

  // Note to self: Apparently the compiler will generate a default inline
  // destructor, and it's better to let the compiler to that than define your 
  // own empty destructor.  This is true even when the parent class has a
  // virtual destructor.
  // http://stackoverflow.com/questions/827196/virtual-default-destructors-in-c
  // ~SBag_t() { fprintf(stderr, "Called SBag destructor.\n"); }

  /*
  std::string get_call_context() {
    std::string res;
    if(_parent) {
      res = _func_name + " called in \n" + _parent->get_call_context();
    } else {
      res = _func_name + "\n";
    }
    return res;
  }*/

};

class PBag_t : public SPBagInterface {
private:
  // the SBag that corresponds to the function instance that holds this PBag
  SPBagInterface *_sib_sbag;
  // each PBag needs its own distinct _view_id
  uint64_t _view_id;
  // eventually will need to contain a vector of 

  PBag_t() {} // disable default constructor

public: 
  PBag_t(SPBagInterface *sib, uint64_t view_id) :
    _sib_sbag(sib), _view_id(view_id) {
    WHEN_CILKSAN_DEBUG( debug_count++; )
  }

#if CILKSAN_DEBUG
  static long debug_count;
  ~PBag_t() {
    debug_count--;
  }
#endif

  bool is_SBag() { return false; }
  bool is_PBag() { return true; }
  uint64_t get_func_id() { return _sib_sbag->get_func_id(); } 
  uint64_t get_view_id() { return _view_id; }
  uint64_t get_rsp() { return _sib_sbag->get_rsp(); }
  void set_rsp(uint64_t stack_ptr) { cilksan_assert(0); /* Should never happen; */ }

  /*
  std::string get_call_context() {
    return _sib_sbag->get_call_context();
  } */
};
#endif // #ifndef _SPBAG_H
