/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2013, Ruby Science Foundation
// NMatrix is Copyright (c) 2013, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == list.c
//
// List-of-lists n-dimensional matrix storage. Uses singly-linked
// lists.

/*
 * Standard Includes
 */

#include <ruby.h>
#include <algorithm> // std::min
#include <iostream>
#include <vector>

/*
 * Project Includes
 */

#include "types.h"

#include "data/data.h"

#include "common.h"
#include "list.h"

#include "math/math.h"
#include "util/sl_list.h"

/*
 * Macros
 */

/*
 * Global Variables
 */

namespace nm { namespace list_storage {

/*
 * Forward Declarations
 */

class RecurseData {
public:
  // Note that providing init_obj argument does not override init.
  RecurseData(const LIST_STORAGE* s, VALUE init_obj__ = Qnil) : ref(s), actual(s), shape_(s->shape), offsets(s->dim, 0), init_(s->default_val), init_obj_(init_obj__) {
    while (actual->src != actual) {
      for (size_t i = 0; i < s->dim; ++i) // update offsets as we recurse
        offsets[i] += actual->offset[i];
      actual = reinterpret_cast<LIST_STORAGE*>(actual->src);
    }
    actual_shape_ = actual->shape;

    if (init_obj_ == Qnil) {
      init_obj_ = s->dtype == nm::RUBYOBJ ? *reinterpret_cast<VALUE*>(s->default_val) : rubyobj_from_cval(s->default_val, s->dtype).rval;
    }
  }

  dtype_t dtype() const { return ref->dtype; }


  size_t dim() const { return ref->dim; }

  size_t ref_shape(size_t rec) const {
    return shape_[ref->dim - rec - 1];
  }

  size_t* copy_alloc_shape() const {
    size_t* new_shape = ALLOC_N(size_t, ref->dim);
    memcpy(new_shape, shape_, sizeof(size_t)*ref->dim);
    return new_shape;
  }

  size_t actual_shape(size_t rec) const {
    return actual_shape_[ref->dim - rec - 1];
  }

  size_t offset(size_t rec) const {
    return offsets[ref->dim - rec - 1];
  }

  void* init() const {
    return init_;
  }

  VALUE init_obj() const { return init_obj_; }

  LIST* top_level_list() const {
    return reinterpret_cast<LIST*>(actual->rows);
  }

  const LIST_STORAGE* ref;
  const LIST_STORAGE* actual;

  size_t* shape_; // of ref
  size_t* actual_shape_;
protected:
  std::vector<size_t> offsets; // relative to actual
  void* init_;
  VALUE init_obj_;

};


template <typename LDType, typename RDType>
static LIST_STORAGE* cast_copy(const LIST_STORAGE* rhs, dtype_t new_dtype);

template <typename LDType, typename RDType>
static bool eqeq_r(RecurseData& left, RecurseData& right, const LIST* l, const LIST* r, size_t rec);

template <typename SDType, typename TDType>
static bool eqeq_empty_r(RecurseData& s, const LIST* l, size_t rec, const TDType* t_init);


/*
 * Recursive helper for map_merged_stored_r which handles the case where one list is empty and the other is not.
 */
static void map_empty_stored_r(RecurseData& result, RecurseData& s, LIST* x, const LIST* l, size_t rec, bool rev, const VALUE& t_init) {
  NODE *curr  = l->first,
       *xcurr = NULL;

  // For reference matrices, make sure we start in the correct place.
  size_t offset   = result.offset(rec);
  size_t x_shape  = result.ref_shape(rec);

  while (curr && curr->key < offset) {  curr = curr->next;  }
  if (curr && curr->key - offset >= x_shape) curr = NULL;

  if (rec) {
    while (curr) {
      LIST* val = nm::list::create();
      map_empty_stored_r(result, s, val, reinterpret_cast<const LIST*>(curr->val), rec-1, rev, t_init);

      if (!val->first) nm::list::del(val, 0);
      else nm::list::insert_helper(x, xcurr, curr->key - offset, val);

      curr = curr->next;
      if (curr && curr->key - offset >= x_shape) curr = NULL;
    }
  } else {
    while (curr) {
      VALUE val, s_val = rubyobj_from_cval(curr->val, s.dtype()).rval;
      if (rev) val = rb_yield_values(2, t_init, s_val);
      else     val = rb_yield_values(2, s_val, t_init);

      if (rb_funcall(val, rb_intern("!="), 1, result.init_obj()) == Qtrue)
        xcurr = nm::list::insert_helper(x, xcurr, curr->key - offset, val);

      curr = curr->next;
      if (curr && curr->key - offset >= x_shape) curr = NULL;
    }
  }

}


/*
 * Recursive helper function for nm_list_map_merged_stored
 */
static void map_merged_stored_r(RecurseData& result, RecurseData& left, RecurseData& right, LIST* x, const LIST* l, const LIST* r, size_t rec) {
  NODE *lcurr = l->first,
       *rcurr = r->first,
       *xcurr = x->first;

  // For reference matrices, make sure we start in the correct place.
  while (lcurr && lcurr->key < left.offset(rec))  {  lcurr = lcurr->next;  }
  while (rcurr && rcurr->key < right.offset(rec)) {  rcurr = rcurr->next;  }

  if (rcurr && rcurr->key - right.offset(rec) >= result.ref_shape(rec)) rcurr = NULL;
  if (lcurr && lcurr->key - left.offset(rec) >= result.ref_shape(rec))  lcurr = NULL;

  if (rec) {
    while (lcurr || rcurr) {
      size_t key;
      LIST*  val = nm::list::create();

      if (!rcurr || (lcurr && (lcurr->key - left.offset(rec) < rcurr->key - right.offset(rec)))) {
        map_empty_stored_r(result, left, val, reinterpret_cast<const LIST*>(lcurr->val), rec-1, false, right.init_obj());
        key   = lcurr->key - left.offset(rec);
        lcurr = lcurr->next;
      } else if (!lcurr || (rcurr && (rcurr->key - right.offset(rec) < lcurr->key - left.offset(rec)))) {
        map_empty_stored_r(result, right, val, reinterpret_cast<const LIST*>(rcurr->val), rec-1, true, left.init_obj());
        key   = rcurr->key - right.offset(rec);
        rcurr = rcurr->next;
      } else { // == and both present
        map_merged_stored_r(result, left, right, val, reinterpret_cast<const LIST*>(lcurr->val), reinterpret_cast<const LIST*>(rcurr->val), rec-1);
        key   = lcurr->key - left.offset(rec);
        lcurr = lcurr->next;
        rcurr = rcurr->next;
      }

      if (!val->first) nm::list::del(val, 0); // empty list -- don't insert
      else xcurr = nm::list::insert_helper(x, xcurr, key, val);

      if (rcurr && rcurr->key - right.offset(rec) >= result.ref_shape(rec)) rcurr = NULL;
      if (lcurr && lcurr->key - left.offset(rec) >= result.ref_shape(rec)) lcurr = NULL;
    }
  } else {
    while (lcurr || rcurr) {
      size_t key;
      VALUE  val;

      if (!rcurr || (lcurr && (lcurr->key - left.offset(rec) < rcurr->key - right.offset(rec)))) {
        val   = rb_yield_values(2, rubyobj_from_cval(lcurr->val, left.dtype()).rval, right.init_obj());
        key   = lcurr->key - left.offset(rec);
        lcurr = lcurr->next;
      } else if (!lcurr || (rcurr && (rcurr->key - right.offset(rec) < lcurr->key - left.offset(rec)))) {
        val   = rb_yield_values(2, left.init_obj(), rubyobj_from_cval(rcurr->val, right.dtype()).rval);
        key   = rcurr->key - right.offset(rec);
        rcurr = rcurr->next;
      } else { // == and both present
        val   = rb_yield_values(2, rubyobj_from_cval(lcurr->val, left.dtype()).rval, rubyobj_from_cval(rcurr->val, right.dtype()).rval);
        key   = lcurr->key - left.offset(rec);
        lcurr = lcurr->next;
        rcurr = rcurr->next;
      }
      if (rb_funcall(val, rb_intern("!="), 1, result.init_obj()) == Qtrue)
        xcurr = nm::list::insert_helper(x, xcurr, key, val);

      if (rcurr && rcurr->key - right.offset(rec) >= result.ref_shape(rec)) rcurr = NULL;
      if (lcurr && lcurr->key - left.offset(rec) >= result.ref_shape(rec)) lcurr = NULL;
    }
  }
}


} // end of namespace list_storage

extern "C" {

/*
 * Functions
 */


////////////////
// Lifecycle //
///////////////

/*
 * Creates a list-of-lists(-of-lists-of-lists-etc) storage framework for a
 * matrix.
 *
 * Note: The pointers you pass in for shape and init_val become property of our
 * new storage. You don't need to free them, and you shouldn't re-use them.
 */
LIST_STORAGE* nm_list_storage_create(dtype_t dtype, size_t* shape, size_t dim, void* init_val) {
  LIST_STORAGE* s = ALLOC( LIST_STORAGE );

  s->dim   = dim;
  s->shape = shape;
  s->dtype = dtype;

  s->offset = ALLOC_N(size_t, s->dim);
  memset(s->offset, 0, s->dim * sizeof(size_t));

  s->rows  = list::create();
  s->default_val = init_val;
  s->count = 1;
  s->src = s;

  return s;
}

/*
 * Documentation goes here.
 */
void nm_list_storage_delete(STORAGE* s) {
  if (s) {
    LIST_STORAGE* storage = (LIST_STORAGE*)s;
    if (storage->count-- == 1) {
      list::del( storage->rows, storage->dim - 1 );

      xfree(storage->shape);
      xfree(storage->offset);
      xfree(storage->default_val);
      xfree(s);
    }
  }
}

/*
 * Documentation goes here.
 */
void nm_list_storage_delete_ref(STORAGE* s) {
  if (s) {
    LIST_STORAGE* storage = (LIST_STORAGE*)s;

    nm_list_storage_delete( reinterpret_cast<STORAGE*>(storage->src ) );
    xfree(storage->shape);
    xfree(storage->offset);
    xfree(s);
  }
}

/*
 * Documentation goes here.
 */
void nm_list_storage_mark(void* storage_base) {
  LIST_STORAGE* storage = (LIST_STORAGE*)storage_base;

  if (storage && storage->dtype == RUBYOBJ) {
    rb_gc_mark(*((VALUE*)(storage->default_val)));
    list::mark(storage->rows, storage->dim - 1);
  }
}

///////////////
// Accessors //
///////////////

/*
 * Documentation goes here.
 */
static NODE* list_storage_get_single_node(LIST_STORAGE* s, SLICE* slice)
{
  size_t r;
  LIST*  l = s->rows;
  NODE*  n;

  for (r = 0; r < s->dim; r++) {
    n = list::find(l, s->offset[r] + slice->coords[r]);
    if (n)  l = reinterpret_cast<LIST*>(n->val);
    else return NULL;
  }

  return n;
}


/*
 * Recursive helper function for each_with_indices, based on nm_list_storage_count_elements_r.
 * Handles empty/non-existent sublists.
 */
static void each_empty_with_indices_r(nm::list_storage::RecurseData& s, size_t rec, VALUE& stack) {
  VALUE empty  = s.dtype() == nm::RUBYOBJ ? *reinterpret_cast<VALUE*>(s.init()) : s.init_obj();

  if (rec) {
    for (long index = 0; index < s.ref_shape(rec); ++index) {
      // Don't do an unshift/shift here -- we'll let that be handled in the lowest-level iteration (recursions == 0)
      rb_ary_push(stack, LONG2NUM(index));
      each_empty_with_indices_r(s, rec-1, stack);
      rb_ary_pop(stack);
    }
  } else {
    rb_ary_unshift(stack, empty);
    for (long index = 0; index < s.ref_shape(rec); ++index) {
      rb_ary_push(stack, LONG2NUM(index));
      rb_yield_splat(stack);
      rb_ary_pop(stack);
    }
    rb_ary_shift(stack);
  }
}

/*
 * Recursive helper function for each_with_indices, based on nm_list_storage_count_elements_r.
 */
static void each_with_indices_r(nm::list_storage::RecurseData& s, const LIST* l, size_t rec, VALUE& stack) {
  NODE*  curr  = l->first;

  size_t offset = s.offset(rec);
  size_t shape  = s.ref_shape(rec);

  while (curr && curr->key < offset) curr = curr->next;
  if (curr && curr->key - offset >= shape) curr = NULL;


  if (rec) {
    for (long index = 0; index < shape; ++index) { // index in reference
      rb_ary_push(stack, LONG2NUM(index));
      if (!curr || index < curr->key - offset) {
        each_empty_with_indices_r(s, rec-1, stack);
      } else { // index == curr->key - offset
        each_with_indices_r(s, reinterpret_cast<const LIST*>(curr->val), rec-1, stack);
        curr = curr->next;
      }
      rb_ary_pop(stack);
    }
  } else {
    for (long index = 0; index < shape; ++index) {

      rb_ary_push(stack, LONG2NUM(index));

      if (!curr || index < curr->key - offset) {
        rb_ary_unshift(stack, s.dtype() == nm::RUBYOBJ ? *reinterpret_cast<VALUE*>(s.init()) : s.init_obj());

      } else { // index == curr->key - offset
        rb_ary_unshift(stack, s.dtype() == nm::RUBYOBJ ? *reinterpret_cast<VALUE*>(curr->val) : rubyobj_from_cval(curr->val, s.dtype()).rval);

        curr = curr->next;
      }
      rb_yield_splat(stack);

      rb_ary_shift(stack);
      rb_ary_pop(stack);
    }
  }

}


/*
 * Recursive helper function for each_stored_with_indices, based on nm_list_storage_count_elements_r.
 */
static void each_stored_with_indices_r(nm::list_storage::RecurseData& s, const LIST* l, size_t rec, VALUE& stack) {
  NODE* curr = l->first;

  size_t offset = s.offset(rec);
  size_t shape  = s.ref_shape(rec);

  while (curr && curr->key < offset) { curr = curr->next; }
  if (curr && curr->key - offset >= shape) curr = NULL;

  if (rec) {
    while (curr) {

      rb_ary_push(stack, LONG2NUM(static_cast<long>(curr->key - offset)));
      each_stored_with_indices_r(s, reinterpret_cast<const LIST*>(curr->val), rec-1, stack);
      rb_ary_pop(stack);

      curr = curr->next;
      if (curr && curr->key - offset >= shape) curr = NULL;
    }
  } else {
    while (curr) {
      rb_ary_push(stack, LONG2NUM(static_cast<long>(curr->key - offset))); // add index to end

      // add value to beginning
      rb_ary_unshift(stack, s.dtype() == nm::RUBYOBJ ? *reinterpret_cast<VALUE*>(curr->val) : rubyobj_from_cval(curr->val, s.dtype()).rval);
      // yield to the whole stack (value, i, j, k, ...)
      rb_yield_splat(stack);

      // remove the value
      rb_ary_shift(stack);

      // remove the index from the end
      rb_ary_pop(stack);

      curr = curr->next;
      if (curr && curr->key - offset >= shape) curr = NULL;
    }
  }
}



/*
 * Each/each-stored iterator, brings along the indices.
 */
VALUE nm_list_each_with_indices(VALUE nmatrix, bool stored) {

  // If we don't have a block, return an enumerator.
  RETURN_SIZED_ENUMERATOR(nmatrix, 0, 0, 0);

  nm::list_storage::RecurseData sdata(NM_STORAGE_LIST(nmatrix));

  VALUE stack = rb_ary_new();

  if (stored) each_stored_with_indices_r(sdata, sdata.top_level_list(), sdata.dim() - 1, stack);
  else        each_with_indices_r(sdata, sdata.top_level_list(), sdata.dim() - 1, stack);

  return nmatrix;
}


/*
 * map merged stored iterator. Always returns a matrix containing RubyObjects which probably needs to be casted.
 */
VALUE nm_list_map_merged_stored(VALUE left, VALUE right, VALUE init) {

  bool scalar = false;

  LIST_STORAGE *s   = NM_STORAGE_LIST(left),
               *t;

  // For each matrix, if it's a reference, we want to deal directly with the original (with appropriate offsetting)
  nm::list_storage::RecurseData sdata(s);

  void* scalar_init = NULL;

  // right might be a scalar, in which case this is a scalar operation.
  if (TYPE(right) != T_DATA || (RDATA(right)->dfree != (RUBY_DATA_FUNC)nm_delete && RDATA(right)->dfree != (RUBY_DATA_FUNC)nm_delete_ref)) {
    nm::dtype_t r_dtype = nm_dtype_min(right);

    scalar_init         = rubyobj_to_cval(right, r_dtype); // make a copy of right

    t                   = reinterpret_cast<LIST_STORAGE*>(nm_list_storage_create(r_dtype, sdata.copy_alloc_shape(), s->dim, scalar_init));
    scalar              = true;
  } else {
    t                   = NM_STORAGE_LIST(right); // element-wise, not scalar.
  }

  //if (!rb_block_given_p()) {
  //  rb_raise(rb_eNotImpError, "RETURN_SIZED_ENUMERATOR probably won't work for a map_merged since no merged object is created");
  //}
  // If we don't have a block, return an enumerator.
  RETURN_SIZED_ENUMERATOR(left, 0, 0, 0); // FIXME: Test this. Probably won't work. Enable above code instead.

  // Figure out default value if none provided by the user
  nm::list_storage::RecurseData tdata(t);
  if (init == Qnil) init = rb_yield_values(2, sdata.init_obj(), tdata.init_obj());

	// Allocate a new shape array for the resulting matrix.
  void* init_val = ALLOC(VALUE);
  memcpy(init_val, &init, sizeof(VALUE));

  NMATRIX* result = nm_create(nm::LIST_STORE, nm_list_storage_create(nm::RUBYOBJ, sdata.copy_alloc_shape(), s->dim, init_val));
  LIST_STORAGE* r = reinterpret_cast<LIST_STORAGE*>(result->storage);
  nm::list_storage::RecurseData rdata(r, init);

  map_merged_stored_r(rdata, sdata, tdata, rdata.top_level_list(), sdata.top_level_list(), tdata.top_level_list(), sdata.dim() - 1);

  // If we are working with a scalar operation
  if (scalar) nm_list_storage_delete(t);

  return Data_Wrap_Struct(CLASS_OF(left), nm_list_storage_mark, nm_delete, result);
}


/*
 * Copy a slice of a list matrix into a regular list matrix.
 */
static LIST* slice_copy(const LIST_STORAGE* src, LIST* src_rows, size_t* coords, size_t* lengths, size_t n) {

  void *val = NULL;
  int key;
  
  LIST* dst_rows = list::create();
  NODE* src_node = src_rows->first;

  while (src_node) {
    key = src_node->key - (src->offset[n] + coords[n]);
    
    if (key >= 0 && (size_t)key < lengths[n]) {
      if (src->dim - n > 1) {
        val = slice_copy( src,
                          reinterpret_cast<LIST*>(src_node->val),
                          coords,
                          lengths,
                          n + 1    );

        if (val) {  list::insert_copy(dst_rows, false, key, val, sizeof(LIST)); }
      }

      else list::insert_copy(dst_rows, false, key, src_node->val, DTYPE_SIZES[src->dtype]);
    }

    src_node = src_node->next;
  }

  return dst_rows;
}

/*
 * Documentation goes here.
 */
void* nm_list_storage_get(STORAGE* storage, SLICE* slice) {
  LIST_STORAGE* s = (LIST_STORAGE*)storage;
  LIST_STORAGE* ns = NULL;
  NODE* n;

  if (slice->single) {
    n = list_storage_get_single_node(s, slice);
    return (n ? n->val : s->default_val);
  } else {
    void *init_val = ALLOC_N(char, DTYPE_SIZES[s->dtype]);
    memcpy(init_val, s->default_val, DTYPE_SIZES[s->dtype]);

    size_t *shape = ALLOC_N(size_t, s->dim);
    memcpy(shape, slice->lengths, sizeof(size_t) * s->dim);

    ns = nm_list_storage_create(s->dtype, shape, s->dim, init_val);

    ns->rows = slice_copy(s, s->rows, slice->coords, slice->lengths, 0);
    return ns;
  }
}

/*
 * Get the contents of some set of coordinates. Note: Does not make a copy!
 * Don't free!
 */
void* nm_list_storage_ref(STORAGE* storage, SLICE* slice) {
  LIST_STORAGE* s = (LIST_STORAGE*)storage;
  LIST_STORAGE* ns = NULL;
  NODE* n;

  //TODO: It needs a refactoring.
  if (slice->single) {
    n = list_storage_get_single_node(s, slice); 
    return (n ? n->val : s->default_val);
  } 
  else {
    ns              = ALLOC( LIST_STORAGE );
    
    ns->dim         = s->dim;
    ns->dtype       = s->dtype;
    ns->offset      = ALLOC_N(size_t, ns->dim);
    ns->shape       = ALLOC_N(size_t, ns->dim);

    for (size_t i = 0; i < ns->dim; ++i) {
      ns->offset[i] = slice->coords[i] + s->offset[i];
      ns->shape[i]  = slice->lengths[i];
    }

    ns->rows        = s->rows;
    ns->default_val = s->default_val;
    
    s->src->count++;
    ns->src         = s->src;
    
    return ns;
  }
}


/*
 * Recursive function, sets multiple values in a matrix from a single source value.
 */
static void slice_set_single(LIST_STORAGE* dest, LIST* l, void* val, size_t* coords, size_t* lengths, size_t n) {

  // drill down into the structure
  NODE* node = NULL;
  if (dest->dim - n > 1) {
    for (size_t i = 0; i < lengths[n]; ++i) {

      size_t key = i + dest->offset[n] + coords[n];

      if (!node) {
        node = list::insert(l, false, key, list::create()); // try to insert list
      } else if (!node->next || (node->next && node->next->key > key)) {
        node = list::insert_after(node, key, list::create());
      } else {
        node = node->next; // correct rank already exists.
      }

      // cast it to a list and recurse
      slice_set_single(dest, reinterpret_cast<LIST*>(node->val), val, coords, lengths, n + 1);
    }
  } else {
    for (size_t i = 0; i < lengths[n]; ++i) {

      size_t key = i + dest->offset[n] + coords[n];

      if (!node)  {
        node = list::insert_copy(l, true, key, val, DTYPE_SIZES[dest->dtype]);
      } else {
        node = list::replace_insert_after(node, key, val, true, DTYPE_SIZES[dest->dtype]);
      }
    }
  }

}


/*
 * Set a value or values in a list matrix.
 */
void nm_list_storage_set(VALUE left, SLICE* slice, VALUE right) {
  LIST_STORAGE* s = NM_STORAGE_LIST(left);

  if (TYPE(right) == T_DATA) {
    if (RDATA(right)->dfree == (RUBY_DATA_FUNC)nm_delete || RDATA(right)->dfree == (RUBY_DATA_FUNC)nm_delete_ref) {
      rb_raise(rb_eNotImpError, "this type of slicing not yet supported");
    } else {
      rb_raise(rb_eTypeError, "unrecognized type for slice assignment");
    }
  } else {
    void* val = rubyobj_to_cval(right, s->dtype);

    bool remove = !std::memcmp(val, s->default_val, s->dtype);

    if (remove) {
      xfree(val);
      list::remove_recursive(s->rows, slice->coords, s->offset, slice->lengths, 0, s->dim);
    } else {
      slice_set_single(s, s->rows, val, slice->coords, slice->lengths, 0);
      xfree(val);
    }
  }
}


/*
 * Insert an entry directly in a row (not using copy! don't free after).
 *
 * Returns a pointer to the insertion location.
 *
 * TODO: Allow this function to accept an entire row and not just one value -- for slicing
 */
NODE* nm_list_storage_insert(STORAGE* storage, SLICE* slice, void* val) {
  LIST_STORAGE* s = (LIST_STORAGE*)storage;
  // Pretend dims = 2
  // Then coords is going to be size 2
  // So we need to find out if some key already exists
  size_t r;
  NODE*  n;
  LIST*  l = s->rows;

  // drill down into the structure
  for (r = s->dim; r > 1; --r) {
    n = list::insert(l, false, s->offset[s->dim - r] + slice->coords[s->dim - r], list::create());
    l = reinterpret_cast<LIST*>(n->val);
  }

  return list::insert(l, true, s->offset[s->dim - r] + slice->coords[s->dim - r], val);
}

/*
 * Remove an item or slice from list storage.
 */
void nm_list_storage_remove(STORAGE* storage, SLICE* slice) {
  LIST_STORAGE* s = (LIST_STORAGE*)storage;

  // This returns a boolean, which will indicate whether s->rows is empty.
  // We can safely ignore it, since we never want to delete s->rows until
  // it's time to destroy the LIST_STORAGE object.
  list::remove_recursive(s->rows, slice->coords, s->offset, slice->lengths, 0, s->dim);
}

///////////
// Tests //
///////////

/*
 * Comparison of contents for list storage.
 */
bool nm_list_storage_eqeq(const STORAGE* left, const STORAGE* right) {
	NAMED_LR_DTYPE_TEMPLATE_TABLE(ttable, nm::list_storage::eqeq_r, bool, nm::list_storage::RecurseData& left, nm::list_storage::RecurseData& right, const LIST* l, const LIST* r, size_t rec)

  nm::list_storage::RecurseData ldata(reinterpret_cast<const LIST_STORAGE*>(left)),
                                rdata(reinterpret_cast<const LIST_STORAGE*>(right));

	return ttable[left->dtype][right->dtype](ldata, rdata, ldata.top_level_list(), rdata.top_level_list(), ldata.dim()-1);
}

//////////
// Math //
//////////


/*
 * List storage matrix multiplication.
 */
STORAGE* nm_list_storage_matrix_multiply(const STORAGE_PAIR& casted_storage, size_t* resulting_shape, bool vector) {
  free(resulting_shape);
  rb_raise(rb_eNotImpError, "multiplication not implemented for list-of-list matrices");
  return NULL;
  //DTYPE_TEMPLATE_TABLE(dense_storage::matrix_multiply, NMATRIX*, STORAGE_PAIR, size_t*, bool);

  //return ttable[reinterpret_cast<DENSE_STORAGE*>(casted_storage.left)->dtype](casted_storage, resulting_shape, vector);
}


/*
 * List storage to Hash conversion. Uses Hashes with default values, so you can continue to pretend
 * it's a sparse matrix.
 */
VALUE nm_list_storage_to_hash(const LIST_STORAGE* s, const dtype_t dtype) {

  // Get the default value for the list storage.
  VALUE default_value = rubyobj_from_cval(s->default_val, dtype).rval;

  // Recursively copy each dimension of the matrix into a nested hash.
  return nm_list_copy_to_hash(s->rows, dtype, s->dim - 1, default_value);
}

/////////////
// Utility //
/////////////

/*
 * Recursively count the non-zero elements in a list storage object.
 */
size_t nm_list_storage_count_elements_r(const LIST* l, size_t recursions) {
  size_t count = 0;
  NODE* curr = l->first;
  
  if (recursions) {
    while (curr) {
      count += nm_list_storage_count_elements_r(reinterpret_cast<const LIST*>(curr->val), recursions - 1);
      curr   = curr->next;
    }
    
  } else {
    while (curr) {
      ++count;
      curr = curr->next;
    }
  }
  
  return count;
}

/*
 * Count non-diagonal non-zero elements.
 */
size_t nm_list_storage_count_nd_elements(const LIST_STORAGE* s) {
  NODE *i_curr, *j_curr;
  size_t count = 0;
  
  if (s->dim != 2) {
  	rb_raise(rb_eNotImpError, "non-diagonal element counting only defined for dim = 2");
  }

  for (i_curr = s->rows->first; i_curr; i_curr = i_curr->next) {
    int i = i_curr->key - s->offset[0];
    if (i < 0 || i >= (int)s->shape[0]) continue;

    for (j_curr = ((LIST*)(i_curr->val))->first; j_curr; j_curr = j_curr->next) {
      int j = j_curr->key - s->offset[1];
      if (j < 0 || j >= (int)s->shape[1]) continue;

      if (i != j)  	++count;
    }
  }
  
  return count;
}

/////////////////////////
// Copying and Casting //
/////////////////////////
//
/*
 * List storage copy constructor C access.
 */

LIST_STORAGE* nm_list_storage_copy(const LIST_STORAGE* rhs)
{
  size_t *shape = ALLOC_N(size_t, rhs->dim);
  memcpy(shape, rhs->shape, sizeof(size_t) * rhs->dim);
  
  void *init_val = ALLOC_N(char, DTYPE_SIZES[rhs->dtype]);
  memcpy(init_val, rhs->default_val, DTYPE_SIZES[rhs->dtype]);

  LIST_STORAGE* lhs = nm_list_storage_create(rhs->dtype, shape, rhs->dim, init_val);
  
  lhs->rows = slice_copy(rhs, rhs->rows, lhs->offset, lhs->shape, 0);

  return lhs;
}

/*
 * List storage copy constructor C access with casting.
 */
STORAGE* nm_list_storage_cast_copy(const STORAGE* rhs, dtype_t new_dtype, void* dummy) {
  NAMED_LR_DTYPE_TEMPLATE_TABLE(ttable, nm::list_storage::cast_copy, LIST_STORAGE*, const LIST_STORAGE* rhs, dtype_t new_dtype);

  return (STORAGE*)ttable[new_dtype][rhs->dtype]((LIST_STORAGE*)rhs, new_dtype);
}


/*
 * List storage copy constructor for transposing.
 */
STORAGE* nm_list_storage_copy_transposed(const STORAGE* rhs_base) {
  rb_raise(rb_eNotImpError, "list storage transpose not yet implemented");
  return NULL;
}


} // end of extern "C" block


/////////////////////////
// Templated Functions //
/////////////////////////



namespace list_storage {


/*
 * List storage copy constructor for changing dtypes.
 */
template <typename LDType, typename RDType>
static LIST_STORAGE* cast_copy(const LIST_STORAGE* rhs, dtype_t new_dtype) {

  // allocate and copy shape
  size_t* shape = ALLOC_N(size_t, rhs->dim);
  memcpy(shape, rhs->shape, rhs->dim * sizeof(size_t));

  // copy default value
  LDType* default_val = ALLOC_N(LDType, 1);
  *default_val = *reinterpret_cast<RDType*>(rhs->default_val);

  LIST_STORAGE* lhs = nm_list_storage_create(new_dtype, shape, rhs->dim, default_val);
  //lhs->rows         = list::create();

  // TODO: Needs optimization. When matrix is reference it is copped twice.
  if (rhs->src == rhs) 
    list::cast_copy_contents<LDType, RDType>(lhs->rows, rhs->rows, rhs->dim - 1);
  else {
    LIST_STORAGE *tmp = nm_list_storage_copy(rhs);
    list::cast_copy_contents<LDType, RDType>(lhs->rows, tmp->rows, rhs->dim - 1);
    nm_list_storage_delete(tmp);
  }

  return lhs;
}


/*
 * Recursive helper function for eqeq. Note that we use SDType and TDType instead of L and R because this function
 * is a re-labeling. That is, it can be called in order L,R or order R,L; and we don't want to get confused. So we
 * use S and T to denote first and second passed in.
 */
template <typename SDType, typename TDType>
static bool eqeq_empty_r(RecurseData& s, const LIST* l, size_t rec, const TDType* t_init) {
  NODE* curr  = l->first;

  // For reference matrices, make sure we start in the correct place.
  while (curr && curr->key < s.offset(rec)) {  curr = curr->next;  }
  if (curr && curr->key - s.offset(rec) >= s.ref_shape(rec)) curr = NULL;

  if (rec) {
    while (curr) {
      if (!eqeq_empty_r<SDType,TDType>(s, reinterpret_cast<const LIST*>(curr->val), rec-1, t_init)) return false;
      curr = curr->next;

      if (curr && curr->key - s.offset(rec) >= s.ref_shape(rec)) curr = NULL;
    }
  } else {
    while (curr) {
      if (*reinterpret_cast<SDType*>(curr->val) != *t_init) return false;
      curr = curr->next;

      if (curr && curr->key - s.offset(rec) >= s.ref_shape(rec)) curr = NULL;
    }
  }
  return true;
}



/*
 * Do these two list matrices of the same dtype have exactly the same contents (accounting for default_vals)?
 *
 * This function is recursive.
 */
template <typename LDType, typename RDType>
static bool eqeq_r(RecurseData& left, RecurseData& right, const LIST* l, const LIST* r, size_t rec) {
  NODE *lcurr = l->first,
       *rcurr = r->first;

  // For reference matrices, make sure we start in the correct place.
  while (lcurr && lcurr->key < left.offset(rec)) {  lcurr = lcurr->next;  }
  while (rcurr && rcurr->key < right.offset(rec)) {  rcurr = rcurr->next;  }
  if (rcurr && rcurr->key - right.offset(rec) >= left.ref_shape(rec)) rcurr = NULL;
  if (lcurr && lcurr->key - left.offset(rec) >= left.ref_shape(rec)) lcurr = NULL;

  bool compared = false;

  if (rec) {

    while (lcurr || rcurr) {

      if (!rcurr || (lcurr && (lcurr->key - left.offset(rec) < rcurr->key - right.offset(rec)))) {
        if (!eqeq_empty_r<LDType,RDType>(left, reinterpret_cast<const LIST*>(lcurr->val), rec-1, reinterpret_cast<const RDType*>(right.init()))) return false;
        lcurr   = lcurr->next;
      } else if (!lcurr || (rcurr && (rcurr->key - right.offset(rec) < lcurr->key - left.offset(rec)))) {
        if (!eqeq_empty_r<RDType,LDType>(right, reinterpret_cast<const LIST*>(rcurr->val), rec-1, reinterpret_cast<const LDType*>(left.init()))) return false;
        rcurr   = rcurr->next;
      } else { // keys are == and both present
        if (!eqeq_r<LDType,RDType>(left, right, reinterpret_cast<const LIST*>(lcurr->val), reinterpret_cast<const LIST*>(rcurr->val), rec-1)) return false;
        lcurr   = lcurr->next;
        rcurr   = rcurr->next;
      }
      if (rcurr && rcurr->key - right.offset(rec) >= right.ref_shape(rec)) rcurr = NULL;
      if (lcurr && lcurr->key - left.offset(rec)  >= left.ref_shape(rec)) lcurr = NULL;
      compared = true;
    }
  } else {
    while (lcurr || rcurr) {

      if (rcurr && rcurr->key - right.offset(rec) >= left.ref_shape(rec)) rcurr = NULL;
      if (lcurr && lcurr->key - left.offset(rec) >= left.ref_shape(rec)) lcurr = NULL;

      if (!rcurr || (lcurr && (lcurr->key - left.offset(rec) < rcurr->key - right.offset(rec)))) {
        if (*reinterpret_cast<LDType*>(lcurr->val) != *reinterpret_cast<const RDType*>(right.init())) return false;
        lcurr         = lcurr->next;
      } else if (!lcurr || (rcurr && (rcurr->key - right.offset(rec) < lcurr->key - left.offset(rec)))) {
        if (*reinterpret_cast<RDType*>(rcurr->val) != *reinterpret_cast<const LDType*>(left.init())) return false;
        rcurr         = rcurr->next;
      } else { // keys == and both left and right nodes present
        if (*reinterpret_cast<LDType*>(lcurr->val) != *reinterpret_cast<RDType*>(rcurr->val)) return false;
        lcurr         = lcurr->next;
        rcurr         = rcurr->next;
      }
      if (rcurr && rcurr->key - right.offset(rec) >= right.ref_shape(rec)) rcurr = NULL;
      if (lcurr && lcurr->key - left.offset(rec)  >= left.ref_shape(rec)) lcurr = NULL;
      compared = true;
    }
  }

  // Final condition: both containers are empty, and have different default values.
  if (!compared && !lcurr && !rcurr) return *reinterpret_cast<const LDType*>(left.init()) == *reinterpret_cast<const RDType*>(right.init());
  return true;
}


}} // end of namespace nm::list_storage

extern "C" {
  /*
   * call-seq:
   *     __list_to_hash__ -> Hash
   *
   * Create a Ruby Hash from a list NMatrix.
   *
   * This is an internal C function which handles list stype only.
   */
  VALUE nm_to_hash(VALUE self) {
    return nm_list_storage_to_hash(NM_STORAGE_LIST(self), NM_DTYPE(self));
  }

    /*
     * call-seq:
     *     __list_default_value__ -> ...
     *
     * Get the default_value property from a list matrix.
     */
    VALUE nm_list_default_value(VALUE self) {
      return (NM_DTYPE(self) == nm::RUBYOBJ) ? *reinterpret_cast<VALUE*>(NM_DEFAULT_VAL(self)) : rubyobj_from_cval(NM_DEFAULT_VAL(self), NM_DTYPE(self)).rval;
    }
} // end of extern "C" block
