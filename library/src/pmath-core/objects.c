#include <pmath-core/objects-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>

#include <stdio.h>
#include <string.h>


#define PMATH_VALID_TYPE_SHIFT(ts)  (((unsigned int)ts) < PMATH_TYPE_SHIFT_COUNT)

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif

typedef struct {
  pmath_proc_t                 destroy;
  pmath_hash_func_t            hash;
  pmath_equal_func_t           equals;
  pmath_compare_func_t         compare;
  _pmath_object_write_func_t   write;
} _pmath_type_imp_t;

PMATH_PRIVATE _pmath_type_imp_t pmath_type_imps[PMATH_TYPE_SHIFT_COUNT];
#ifdef PMATH_DEBUG_MEMORY
static pmath_atomic_t object_alloc_stats[PMATH_TYPE_SHIFT_COUNT];
static char *type_names[PMATH_TYPE_SHIFT_COUNT] = {
  "float (multi prec)",
  "integer (multi prec)",
  "quotient",
  "string",
  "symbol",
  "expression (general)",
  "expression (part)",
  "symbol rule",
  "custom",
  "memory blob",
  "packed array"
};
#endif

#if PMATH_BITSIZE < 64
static volatile _pmath_timer_t global_timer;
static pmath_atomic_t global_timer_spin = PMATH_ATOMIC_STATIC_INIT;
#else
static pmath_atomic_t global_timer = PMATH_ATOMIC_STATIC_INIT;
#endif

PMATH_PRIVATE
_pmath_timer_t _pmath_timer_get(void) {
#if PMATH_BITSIZE < 64
  _pmath_timer_t result;
  
  pmath_atomic_lock(&global_timer_spin);
  
  result = global_timer;
  
  pmath_atomic_unlock(&global_timer_spin);
  
  return result;
#else
  return pmath_atomic_read_aquire(&global_timer);
#endif
}

PMATH_PRIVATE
_pmath_timer_t _pmath_timer_get_next(void) {
#if PMATH_BITSIZE < 64
  _pmath_timer_t result;
  
  pmath_atomic_lock(&global_timer_spin);
  
  result = ++global_timer;
  
  pmath_atomic_unlock(&global_timer_spin);
  
  return result;
#else
  return pmath_atomic_fetch_add(&global_timer, +1);
#endif
}


/*============================================================================*/

PMATH_API void _pmath_destroy_object(pmath_t obj) {
  assert(pmath_is_pointer(obj));
  assert(!pmath_is_null(obj));
  
#ifdef PMATH_DEBUG_LOG
  if(! PMATH_VALID_TYPE_SHIFT(PMATH_AS_PTR(obj)->type_shift)) {
    fprintf(stderr, "[invalid type shift: %p, %d]\n",
            PMATH_AS_PTR(obj), PMATH_AS_PTR(obj)->type_shift);
  }
#endif
  
  assert(PMATH_VALID_TYPE_SHIFT(PMATH_AS_PTR(obj)->type_shift));
  assert(pmath_refcount(obj) == 0 || PMATH_AS_PTR(obj)->type_shift == PMATH_TYPE_SHIFT_SYMBOL);
  
  if(pmath_type_imps[PMATH_AS_PTR(obj)->type_shift].destroy)
    pmath_type_imps[PMATH_AS_PTR(obj)->type_shift].destroy(obj);
}

PMATH_API unsigned int pmath_hash(pmath_t obj) {
  if(pmath_is_pointer(obj) && PMATH_AS_PTR(obj) != NULL) {
    pmath_hash_func_t hash;
    
#  ifdef PMATH_DEBUG_LOG
    if(! PMATH_VALID_TYPE_SHIFT(PMATH_AS_PTR(obj)->type_shift)) {
      fprintf(stderr, "[invalid type shift: %p, %d]\n",
              PMATH_AS_PTR(obj), PMATH_AS_PTR(obj)->type_shift);
    }
#  endif

    assert(PMATH_VALID_TYPE_SHIFT(PMATH_AS_PTR(obj)->type_shift));
    
    hash = pmath_type_imps[PMATH_AS_PTR(obj)->type_shift].hash;
    assert(hash != NULL);
    return hash(obj);
  }
  
  if(pmath_is_double(obj)) {
    /* Ensure that +0.0 and -0.0 give the same hash value.
       
       Another option would be to make (0.0 === -0.0) return False, but then
       we would have to make (0.0`20 === -0.0`20) give False, too. 
       Identical() depends on pmath_equals() and we would have to add some
       pmath_mp_identical() function.
       
       TODO: add Internal`SignBit(), Internal`CopySign(), Internal`NextToward()
     */
    
    double d = PMATH_AS_DOUBLE(obj);
    if(d == 0)
      d = +0.0;
      
    return incremental_hash(&d, sizeof(double), 0); 
  }
  
  return incremental_hash(&obj, sizeof(pmath_t), 0);
}

#ifdef pmath_equals
#  undef pmath_equals
#endif

PMATH_API pmath_bool_t pmath_equals(
  pmath_t objA,
  pmath_t objB
) {
  pmath_equal_func_t   eqA,  eqB;
  pmath_compare_func_t cmpA, cmpB;
  
  if(pmath_same(objA, objB))
    return TRUE;
    
  if(pmath_is_pointer(objA) && PMATH_AS_PTR(objA) != NULL) {
    eqA  = pmath_type_imps[PMATH_AS_PTR(objA)->type_shift].equals;
    cmpA = pmath_type_imps[PMATH_AS_PTR(objA)->type_shift].compare;
  }
  else if(pmath_is_double(objA)) {
    eqA  = _pmath_numbers_equal;
    cmpA = _pmath_numbers_compare;
  }
  else if(pmath_is_ministr(objA)) {
    eqA  = _pmath_strings_equal;
    cmpA = _pmath_strings_compare;
  }
  else
    return FALSE;
    
  if(pmath_is_pointer(objB) && PMATH_AS_PTR(objB) != NULL) {
    eqB  = pmath_type_imps[PMATH_AS_PTR(objB)->type_shift].equals;
    cmpB = pmath_type_imps[PMATH_AS_PTR(objB)->type_shift].compare;
  }
  else if(pmath_is_double(objB)) {
    eqB  = _pmath_numbers_equal;
    cmpB = _pmath_numbers_compare;
  }
  else if(pmath_is_ministr(objB)) {
    eqB  = _pmath_strings_equal;
    cmpB = _pmath_strings_compare;
  }
  else
    return FALSE;
    
  if(eqA && eqA == eqB)
    return eqA(objA, objB);
    
  assert(cmpA != NULL);
  assert(cmpB != NULL);
  
  if(cmpA == cmpB)
    return 0 == cmpA(objA, objB);
    
  return FALSE;
}

PMATH_API int pmath_compare(pmath_t objA, pmath_t objB) {
  pmath_compare_func_t cmpA = NULL;
  pmath_compare_func_t cmpB = NULL;
  
  if(pmath_same(objA, objB))
    return 0;
    
  if(pmath_is_double(objA) || pmath_is_int32(objA)) {
    cmpA = _pmath_numbers_compare;
  }
  else if(pmath_is_ministr(objA)) {
    cmpA = _pmath_strings_compare;
  }
  else if(pmath_is_pointer(objA) && PMATH_AS_PTR(objA) != NULL) {
    cmpA = pmath_type_imps[PMATH_AS_PTR(objA)->type_shift].compare;
  }
  
  if(pmath_is_double(objB) || pmath_is_int32(objB)) {
    cmpB = _pmath_numbers_compare;
  }
  else if(pmath_is_ministr(objB)) {
    cmpB = _pmath_strings_compare;
  }
  else if(pmath_is_pointer(objB) && PMATH_AS_PTR(objB) != NULL) {
    cmpB = pmath_type_imps[PMATH_AS_PTR(objB)->type_shift].compare;
  }
  
  if(cmpA && cmpA == cmpB) {
    return cmpA(objA, objB);
  }
  
  if(pmath_is_double(objA))
    return -1;
    
  if(pmath_is_double(objB))
    return 1;
    
  if(pmath_is_pointer(objA) && PMATH_AS_PTR(objA) != NULL) {
    if(pmath_is_pointer(objB) && PMATH_AS_PTR(objB) != NULL) {
      return PMATH_AS_PTR(objA)->type_shift - PMATH_AS_PTR(objB)->type_shift;
    }
    
    return 1;
  }
  
  if(pmath_is_pointer(objB) && PMATH_AS_PTR(objB) != NULL)
    return -1;
    
  if(PMATH_AS_TAG(objA) < PMATH_AS_TAG(objB))
    return -1;
    
  if(PMATH_AS_TAG(objA) > PMATH_AS_TAG(objB))
    return 1;
    
  if(PMATH_AS_INT32(objA) < PMATH_AS_INT32(objB))
    return -1;
    
  if(PMATH_AS_INT32(objA) > PMATH_AS_INT32(objB))
    return 1;
    
  return 0;
}

PMATH_API void pmath_write(
  pmath_t                 obj,
  pmath_write_options_t   options,
  void (*write)(void *, const uint16_t *, int),
  void                   *user
) {
  struct pmath_write_ex_t info;
  memset(&info, 0, sizeof(info));
  info.size    = sizeof(info);
  info.options = options;
  info.user    = user;
  info.write   = write;
  
  pmath_write_ex(&info, obj);
}

PMATH_API
void pmath_write_ex(struct pmath_write_ex_t *info, pmath_t obj) {
  assert(info != NULL);
  
  if(info->size > sizeof(struct pmath_write_ex_t))
    return;
    
  if(info->size < sizeof(struct pmath_write_ex_t)) {
    struct pmath_write_ex_t info2;
    memset(&info2, 0, sizeof(info2));
    memcpy(&info2, info, info->size);
    info2.size = sizeof(info2);
    
    pmath_write_ex(&info2, obj);
    return;
  }
  
  if(info->pre_write)
    info->pre_write(info->user, obj, info->options);
    
  if(pmath_is_pointer(obj)) {
    if(PMATH_AS_PTR(obj) == NULL) {
      _pmath_write_cstr("/\\/", info->write, info->user);
      
      if(info->post_write)
        info->post_write(info->user, obj, info->options);
        
      return;
    }
    
#ifdef PMATH_DEBUG_MEMORY
    if(pmath_refcount(obj) == 0) {
      _pmath_write_cstr("[NOREF: ", info->write, info->user);
    }
    else if(pmath_refcount(obj) < 0) {
      _pmath_write_cstr("\a[INVREF: ", info->write, info->user);
    }
#endif
    
    assert(PMATH_VALID_TYPE_SHIFT(PMATH_AS_PTR(obj)->type_shift));
    
    if(!pmath_type_imps[PMATH_AS_PTR(obj)->type_shift].write) {
      char s[100];
      snprintf(s, sizeof(s), "(/\\/ /* 16^^%"PRIxPTR" */)", (uintptr_t)PMATH_AS_PTR(obj));
      _pmath_write_cstr(s, info->write, info->user);
    }
    else
      pmath_type_imps[PMATH_AS_PTR(obj)->type_shift].write(info, obj);
      
#ifdef PMATH_DEBUG_MEMORY
    if(pmath_refcount(obj) <= 0) {
      _pmath_write_cstr("]", info->write, info->user);
    }
#endif
    
    if(info->post_write)
      info->post_write(info->user, obj, info->options);
      
    return;
  }
  
  if(pmath_is_double(obj)) {
    _pmath_write_machine_float(info, obj);
    
    if(info->post_write)
      info->post_write(info->user, obj, info->options);
      
    return;
  }
  
  if(pmath_is_int32(obj)) {
    _pmath_write_machine_int(info, obj);
    
    if(info->post_write)
      info->post_write(info->user, obj, info->options);
      
    return;
  }
  
  if(pmath_is_ministr(obj)) {
    _pmath_string_write(info, obj);
    
    if(info->post_write)
      info->post_write(info->user, obj, info->options);
      
    return;
  }
  
  {
    char s[40];
    
    snprintf(s, sizeof(s), "(/\\/ /* 16^^%x, 16^^%x */)",
             (int)PMATH_AS_TAG(obj),
             (int)PMATH_AS_INT32(obj));
             
    _pmath_write_cstr(s, info->write, info->user);
  }
  
  if(info->post_write)
    info->post_write(info->user, obj, info->options);
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE int pmath_maxrecursion = 128;
//static int maxiteration = 4096;

PMATH_API
pmath_bool_t pmath_is_evaluated(pmath_t obj) {
  if(pmath_is_expr(obj))
    return _pmath_expr_is_updated(obj);
    
  if(pmath_is_symbol(obj)) {
    pmath_t value = pmath_symbol_get_value(obj);
    pmath_bool_t result = !pmath_is_evaluatable(value);
    pmath_unref(value);
    return result;
  }
  
  return TRUE;
}

PMATH_API
pmath_t pmath_get_debug_info(pmath_t obj) {
  if(pmath_is_expr(obj))
    return _pmath_expr_get_debug_info(obj);
  
  return PMATH_NULL;
}

PMATH_API
pmath_t pmath_try_set_debug_info(pmath_t obj, pmath_t debug_info) {
  if(pmath_is_expr(obj))
    return _pmath_expr_set_debug_info(obj, debug_info);
  
  pmath_unref(debug_info);
  return obj;
}

/*============================================================================*/

PMATH_PRIVATE pmath_t _pmath_create_stub(unsigned int type_shift, size_t size) {
  struct _pmath_t *obj;
  
  assert(size >= sizeof(struct _pmath_t));
  assert(PMATH_VALID_TYPE_SHIFT(type_shift));
#ifdef PMATH_DEBUG_MEMORY
  (void)pmath_atomic_fetch_add(&object_alloc_stats[type_shift], 1);
#endif
  
  obj = pmath_mem_alloc(size);
  if(!obj)
    return PMATH_NULL;
    
  obj->type_shift = type_shift;
  obj->flags8     = 0;
  obj->flags16    = 0;
  pmath_atomic_write_release(&obj->refcount, 1);
  return PMATH_FROM_PTR(obj);
}

PMATH_PRIVATE void _pmath_init_special_type(
  unsigned int                type_shift,
  pmath_compare_func_t        comparer,
  pmath_hash_func_t           hashfunc,
  /* optional ... */
  pmath_proc_t                destructor,
  pmath_equal_func_t          equality_comparer,
  _pmath_object_write_func_t  writer
) {
  assert(PMATH_VALID_TYPE_SHIFT(type_shift));
  assert(comparer != NULL);
  assert(hashfunc != NULL);
  pmath_type_imps[type_shift].destroy     = destructor;
  pmath_type_imps[type_shift].compare     = comparer;
  pmath_type_imps[type_shift].hash        = hashfunc;
  pmath_type_imps[type_shift].equals      = equality_comparer;
  pmath_type_imps[type_shift].write       = writer;
}

PMATH_PRIVATE pmath_bool_t _pmath_objects_init(void) {
#if PMATH_BITSIZE < 64
  global_timer = 1;
#else
  global_timer._data = 1;
#endif
  
  memset(pmath_type_imps, 0, sizeof(pmath_type_imps));
#ifdef PMATH_DEBUG_MEMORY
  memset(object_alloc_stats, 0, sizeof(object_alloc_stats));
#endif
  return TRUE;
}

PMATH_PRIVATE void _pmath_objects_done(void) {
#ifdef PMATH_DEBUG_MEMORY
  size_t total = 0;
  int i;
  pmath_debug_print("\ntype                 allocations\n");
  for(i = 0; i < PMATH_TYPE_SHIFT_COUNT; i++) {
    size_t stat = (size_t)pmath_atomic_read_aquire(&object_alloc_stats[i]);
    
    pmath_debug_print("%-20s %6"PRIdPTR"\n", type_names[i], stat);
    total += stat;
  }
  pmath_debug_print("total object allocations: %"PRIdPTR"\n", total);
#endif
  
#if PMATH_BITSIZE < 64
  pmath_debug_print("global_timer = %"PRId64"\n", global_timer);
#else
  pmath_debug_print("global_timer = %"PRIdPTR"\n", global_timer._data);
#endif
}
