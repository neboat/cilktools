// -*- C++ -*-
#ifndef REDUCER_VECTOR_H_INCLUDED
#define REDUCER_VECTOR_H_INCLUDED
#define NDEBUG

#include <stdlib.h>
#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <vector>
#include <cassert>

// #include <cilksan.h>
#include <reducertool.h>

// extern "C++"
// {

// Forward class definition
template <class _T, class _A> class reducer_basic_vector;

//////////////////////////////////////////////////////////////////////
// The hypervector class implements the underlying monoid for the
// reducer_basic_vector data structure.  It supports the following
// methods:
// 
//   a default constructor,
//
//   a destructor,
//
//   push_back() - append a given element to the end of the
//   hypervector's head,
//   
//   concatenate() - append a given hypervector onto the end of this
//   hypervector.
//
//   get_vec() - convert this hypervector into a std::vector object.
//   Private methods are given to allow a parallel implementation of
//   this method.
//
// The semantics of the hypervector object are as follows.  A
// hypervector consists of a vector and a linked-list of hypervector
// nodes.  The identity hypervector consists of a single empty vector
// object, called the head, and an empty linked-list.  Each thread may
// append elements to the end of its head vector.  When a child thread
// joins its parent thread in the steal tree, the reduce operation
// appends the child hypervector head and list to the end of the
// parent's linked list.  The get_vec() method collapses this linked
// list into a single vector object with each subsequent vector in the
// linked list appended onto the end of the existing head vector.
//
// The hypervector assumes that each thread in a set of threads pushes
// a contiguous set of elements onto the end of a vector.  The
// hypervector provides a monoid that allows for the implementation of
// a reducer to provide this functionality to a set of parallel
// threads.
//////////////////////////////////////////////////////////////////////
template<class _T,
         class _A = std::allocator<_T> >
class hypervector
{
  friend class reducer_basic_vector<_T, _A>;

public:
  typedef std::vector<_T, _A> vector_type;
  typedef std::vector<_T, _A> value_type;
  typedef typename vector_type::size_type size_type;
  
  // Default Constructor
  hypervector();
  
  // Destructor
  ~hypervector();

  // Get the std::vector corresponding to this hypervector (parallel)
  const std::vector<_T, _A>& get_vec();

  // Appends hv to the end of this hypervector
  void concatenate(hypervector<_T, _A> *hv);

  // Pushes element onto the end of this hypervector's head
  void push_back(const _T element);

  const std::vector<_T, _A>& view_get_value() { return get_vec(); }

private:
  //Hypervector Data Structure Definition
  struct hypervector_node {
    hypervector_node* next;
    vector_type* vec;

    hypervector_node() {
      next = NULL;
      vec = new vector_type();
    }
  };

  hypervector_node* head;
  hypervector_node* end;
  size_type list_size;
  size_type total_size;

  // Additional struct, used for linking Cilk++ code in C++
  struct ArgStruct {
    hypervector* self;
  };

  // Additional methods necessary to link Cilk++ code in C++.
  //
  // The __cilk attribute is necessary in both the declaration and the
  // definition of the method.  This is necessary in parallel reducer
  // methods because all Cilk++ reducers must be C++ classes, not
  // Cilk++ classes.  If you create a parallel helper function to
  // implement parallel_get_vec(), be sure to use the __cilk attribute
  // for that method's declaration and definition as well.
  void parallel_get_vec();
  static void parallel_get_vec_wrapper(void *args);
  void parallel_get_vec_helper(hypervector_node *start_node,
					     size_type list_size,
					     size_type start_index);
}; // class hypervector

// Reducer class that implements a basic vector.  This reducer uses
// the hypervector class as the underlying monoid.  All methods of the
// hypervector class are exposed in the reducer_basic_vector except
// for concatenate(), which is used as the reduce() operation for this
// reducer.
template<class _T,
         class _A = std::allocator<_T> >
class reducer_basic_vector
{

public:
  // View of reducer data
  struct Monoid : public cilk::monoid_with_view< hypervector<_T, _A>, true >
  {
    static void reduce (hypervector<_T, _A> *left, hypervector<_T, _A> *right);
  };

private:
  //Hyperobject to serve up views
  cilk::reducer<Monoid> imp_;

public:

  // Default constructor - Construct an empty reducer_basic_vector
  reducer_basic_vector();

  // Destructor
  ~reducer_basic_vector();

  // Return a read-only reference to the current vector
  const std::vector<_T, _A> &get_vec();

  // Return a read-only reference to the current vector
  const std::vector<_T, _A> &get_value() { return get_vec(); }

  // Add an element to the end of the hypervector
  void push_back(const _T element);

}; // class reducer_basic_vector

///////////////////////////////////////////////////////
/// Implementation of inline and template functions ///
///////////////////////////////////////////////////////

//----------------------------//
// template class hypervector //
//----------------------------//
// Default constructor -- creates an empty hypervector
template<class _T, class _A>
hypervector<_T, _A>::hypervector()
{
  cilk_set_reducer(this, __builtin_return_address(0), __FUNCTION__, __LINE__);
  BEGIN_UPDATE_STRAND_NOSCOPE;
  //this->head->vec = new vector_type();
  //this->head->next = NULL;
  this->head = new hypervector_node();
  this->end = this->head;
  assert(this->end->next == NULL);
  this->list_size = 1;
  this->total_size = 0;
  END_UPDATE_STRAND_NOSCOPE;
}

// Destructor
template<class _T, class _A>
hypervector<_T, _A>::~hypervector()
{
  //fprintf(stderr, "deleting\n");
  BEGIN_UPDATE_STRAND_NOSCOPE;
  if (head != NULL) {
    delete head->vec;
    hypervector_node* current = head->next;
    while (current != NULL) {
      if (current->vec != NULL)
	delete current->vec;
      current = current->next;
    }
  }
  END_UPDATE_STRAND_NOSCOPE;
}

// Get the std::vector corresponding to this hypervector (parallel)
template<class _T, class _A>
const std::vector<_T, _A>&
hypervector<_T, _A>::get_vec()
{
  //fprintf(stderr, "get_vec() called, this = %p\n", this);
  // ArgStruct args = { this };
  // ctx.run(&(parallel_get_vec_wrapper), &args);
  //fprintf(stderr, "get_vec() returning\n");
  cilk_read_reducer(this, __builtin_return_address(0), __FUNCTION__, __LINE__);
  
  BEGIN_UPDATE_STRAND_NOSCOPE;
  parallel_get_vec();
  const std::vector<_T, _A> &ret  = *(this->head->vec);
  END_UPDATE_STRAND_NOSCOPE;

  return ret;
}

// Wrapper for parallel method to get the std::vector for this
// hypervector
/*
template<class _T, class _A>
void hypervector<_T, _A>::parallel_get_vec_wrapper(void* args)
{
  //fprintf(stderr, "parallel_get_vec_wrapper called\n");
  ArgStruct* arg = (ArgStruct*) args;
  hypervector* self = arg->self;
  self->parallel_get_vec();
}*/

// Parallel method to get the std::vector for this hypervector
// Angelina: No need to annotate with UPDATE_STRAND because 
// it's only called within this class, and its caller is already annotated.
template<class _T, class _A>
void hypervector<_T, _A>::parallel_get_vec()
{
  cilk_read_reducer(this, __builtin_return_address(0), __FUNCTION__, __LINE__);
  //fprintf(stderr, "parallel_get_vec() called\n");
  if (this->head->next == NULL)
    return;

  if (this->list_size > 1) {
    int head_vec_size = this->head->vec->size();
    this->head->vec->resize(this->total_size);
    parallel_get_vec_helper(this->head->next, this->list_size - 1,
			    head_vec_size);
    //delete head->next;
    this->head->next = NULL;
    this->list_size = 1;
  }
}

// Recursive helper method for getting the the std::vector for this
// hypervector in parallel.
// Angelina: No need to annotate with UPDATE_STRAND because 
// it's only called within this class, and its caller is already annotated.
template<class _T, class _A>
void hypervector<_T, _A>::parallel_get_vec_helper(hypervector_node* start_node,
                                                  size_type list_size,
                                                  size_type start_index)
{
  //fprintf(stderr, "start_node = %p, list_size = %d, start_index = %d\n", start_node, list_size, start_index);

  assert(start_node != NULL);

  if (list_size <= 1) {
    if (list_size < 1)
      return;
    // I want to use a memcpy here, but I'm not sure how to do this
    // given that I am using the std::vector type.
    int vector_size = start_node->vec->size();
    cilk_for (int i = 0; i < vector_size; ++i) {
      assert(start_index + i < this->head->vec->size());
      assert(i < start_node->vec->size());
      (*this->head->vec)[start_index + i] = (*start_node->vec)[i];
    }

    //fprintf(stderr, "Copy complete\n");

    //delete start_node->vec;
    //start_node->next = NULL;

    //fprintf(stderr, "Returning...\n");

    
  } else {
// A good-and-proper divide-and-conquer scheme for get_vec(). This
// routine gives logarithmic base parallelism, but this comes from
// additional work.
//
//     hypervector_node* current_node = start_node;
//     int i = 1;
//     int next_start_index = start_index + current_node->vec->size();
//     while (i < list_size / 2) {
//       ++i;
//       assert(current_node->next != NULL);
//       current_node = current_node->next;
//       next_start_index += current_node->vec->size();
//     }
    
//     cilk_spawn parallel_get_vec_helper(current_node->next, list_size - (list_size / 2),
// 				       next_start_index);
//                parallel_get_vec_helper(start_node, list_size / 2, start_index);
//     cilk_sync;

// A simpler scheme for parallelising get_vec().  Less parallelism,
// but less work too.
    hypervector_node* next_node = start_node->next;
    int next_start_index = start_index + start_node->vec->size();
    cilk_spawn parallel_get_vec_helper(start_node, 1, start_index);
               parallel_get_vec_helper(next_node, list_size - 1,
				       next_start_index);
    cilk_sync;
  }
  
}

// Append a hypervector onto the end of this hypervector's linked list
template<class _T, class _A>
void
hypervector<_T, _A>::concatenate(hypervector<_T, _A> *hv)
{
  //fprintf(stderr, "concatenate called, this = %p, list_size = %d, hv = %p, hv->list_size = %d, hv->end = %p, hv->end->next = %p\n",
  //        this, this->list_size, hv, hv->list_size, hv->end, hv->end->next);
  BEGIN_UPDATE_STRAND_NOSCOPE;
  this->end->next = hv->head;
  this->end = hv->end;
  //assert(hv->end->next == NULL);
  this->list_size += hv->list_size;
  this->total_size += hv->total_size;
  hv->head = NULL;
  END_UPDATE_STRAND_NOSCOPE;
  //fprintf(stderr, "concatenate called, this = %p, list_size = %d, hv = %p, hv->list_size = %d, hv->end = %p, hv->end->next = %p\n",
  //        this, this->list_size, hv, hv->list_size, hv->end, hv->end->next);
}

// Push the given element onto the end of this hypervector's head
// vector.
template<class _T, class _A>
void
hypervector<_T, _A>::push_back(const _T element)
{
  //fprintf(stderr, "push_back called\n");
  BEGIN_UPDATE_STRAND_NOSCOPE;
  this->head->vec->push_back(element);
  ++(this->total_size);
  END_UPDATE_STRAND_NOSCOPE;
  //fprintf(stderr, "push_back done\n");
}

//---------------------------------------//
// template class reducer_vector::Monoid //
//---------------------------------------//

// reduce - appends vector from "right" reducer_basic_vector on the
// end of the "left." When done, the "right" reducer_basic_vector is
// empty.
template<class _T, class _A>
void
reducer_basic_vector<_T, _A>::Monoid::reduce(hypervector<_T, _A> *left,
					     hypervector<_T, _A> *right)
{
  BEGIN_REDUCE_STRAND {
  left->concatenate(right);
  } END_REDUCE_STRAND;
}

// Default constructor - creates an empty hypervector
template<class _T, class _A>
reducer_basic_vector<_T, _A>::reducer_basic_vector() :
  imp_()
{ }

// Destructor
template<class _T, class _A>
reducer_basic_vector<_T, _A>::~reducer_basic_vector()
{ }

// get_vec() - allows read-only access to the corresponding vector,
// and collapses the linked-list into the head's vector.
// Angelina: Don't need to annotate; the corresponding underlying imp_ is already
// annotated.
template<class _T, class _A>
const std::vector<_T, _A>&
reducer_basic_vector<_T, _A>::get_vec()
{
  return imp_.view().get_vec();
}

// push_back() - push the given element onto the end of this vector's
// head vector.
// Angelina: Don't need to annotate; the corresponding underlying imp_ is already
// annotated.
template<class _T, class _A>
void
reducer_basic_vector<_T, _A>::push_back(const _T element)
{
  imp_.view().push_back(element);
}

// } // extern "C++"

#endif // #ifdef HYPERVECTOR_H_INCLUDED
