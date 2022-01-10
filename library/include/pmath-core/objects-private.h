#ifndef __PMATH_CORE__OBJECTS_PRIVATE_H__
#define __PMATH_CORE__OBJECTS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-inline.h>

enum { // private pmath_type_t ...
  PMATH_TYPE_SHIFT_MULTIRULE       = PMATH_TYPE_SHIFT_RESERVED_1,
  PMATH_TYPE_SHIFT_DISPATCH_TABLE  = PMATH_TYPE_SHIFT_RESERVED_2,
  
  PMATH_TYPE_MULTIRULE             = 1 << PMATH_TYPE_SHIFT_MULTIRULE,
  PMATH_TYPE_DISPATCH_TABLE        = 1 << PMATH_TYPE_SHIFT_DISPATCH_TABLE
};

#define pmath_is_multirule(obj)       (pmath_is_pointer_of(obj, PMATH_TYPE_MULTIRULE))
#define pmath_is_dispatch_table(obj)  (pmath_is_pointer_of(obj, PMATH_TYPE_DISPATCH_TABLE))


#define PMATH_MAGIC_METADATA_LIST     PMATH_FROM_TAG(PMATH_TAG_MAGIC, 3)


PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_NONNULL(1)
intptr_t _pmath_refcount_ptr(struct _pmath_t *obj) {
  return pmath_atomic_read_aquire(&obj->refcount);
}

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_ref_ptr(struct _pmath_t *obj) {
  (void)pmath_atomic_fetch_add(&(obj->refcount), 1);
}

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_unref_ptr(struct _pmath_t *obj) {
  pmath_atomic_barrier();
  if(1 == pmath_atomic_fetch_add(&(obj->refcount), -1)) { // was 1 -> is 0
    _pmath_destroy_object(PMATH_FROM_PTR(obj));
  }
  pmath_atomic_barrier();
}



extern PMATH_PRIVATE int pmath_maxrecursion;

#if PMATH_BITSIZE < 64
typedef int64_t _pmath_timer_t;
#else
typedef intptr_t _pmath_timer_t;
#endif

struct _pmath_timed_t {
  struct _pmath_t  inherited;
  _pmath_timer_t   last_change;
};

struct _pmath_gc_t { // expr and symbol inherit from this
  struct _pmath_timed_t  inherited;
  uintptr_t              gc_refcount;
#if PMATH_BITSIZE == 32
  pmath_atomic_uint32_t  padding_flags32;
#endif
};

#define _pmath_is_gcobj(obj)          (pmath_is_pointer_of((obj), PMATH_TYPE_EXPRESSION|PMATH_TYPE_SYMBOL))

#if PMATH_BITSIZE == 32
#  define PMATH_GC_FLAGS32(PTR)  ((PTR)->padding_flags32)
#else
#  define PMATH_GC_FLAGS32(PTR)  ((PTR)->inherited.inherited.padding_flags32)
#endif

#ifdef PMATH_DEBUG_LOG
#  define PMATH_OBJECT_MARK_DELETION_TRAP(_obj_ptr)   ((void)pmath_atomic_or_uint8(&(_obj_ptr)->flags8, PMATH_OBJECT_FLAGS8_TRAP_DELETED))
#else
#  define PMATH_OBJECT_MARK_DELETION_TRAP(_obj_ptr)   ((void)0)
#endif

PMATH_PRIVATE
_pmath_timer_t _pmath_timer_get(void);

PMATH_PRIVATE
_pmath_timer_t _pmath_timer_get_next(void);

typedef void (*_pmath_object_write_func_t)(struct pmath_write_ex_t *, pmath_t);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_write_impl(struct pmath_write_ex_t *info, pmath_t obj);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_create_stub(unsigned int type_shift, size_t size);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(2, 3)
void _pmath_init_special_type(
  unsigned int                type_shift,
  pmath_compare_func_t        comparer,
  pmath_hash_func_t           hashfunc,
  /* optional ... */
  pmath_proc_t                destructor,
  pmath_equal_func_t          equality_comparer,
  _pmath_object_write_func_t  writer);

//PMATH_PRIVATE
//pmath_bool_t _pmath_object_find_user_rule(
//  pmath_t symbol, // wont be freed; Must be a pmath_symbol_t!
//  pmath_t *obj);  // [in/out]

PMATH_PRIVATE pmath_bool_t _pmath_objects_init(void);
PMATH_PRIVATE void         _pmath_objects_done(void);

#endif /* __PMATH_CORE__OBJECTS_PRIVATE_H__ */
