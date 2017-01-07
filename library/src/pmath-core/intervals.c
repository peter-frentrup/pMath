#include <pmath-core/intervals-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/objects-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>


//{ caching unused intervals ...

#define CACHE_SIZE 256
#define CACHE_MASK (CACHE_SIZE-1)

static pmath_atomic_t interval_cache[CACHE_SIZE];
static pmath_atomic_t interval_cache_pos = PMATH_ATOMIC_STATIC_INIT;

#ifdef PMATH_DEBUG_LOG
static pmath_atomic_t interval_cache_hits   = PMATH_ATOMIC_STATIC_INIT;
static pmath_atomic_t interval_cache_misses = PMATH_ATOMIC_STATIC_INIT;
#endif

static uintptr_t interval_cache_inc(intptr_t delta) {
  return (uintptr_t)pmath_atomic_fetch_add(&interval_cache_pos, delta);
}

static struct _pmath_interval_t *interval_cache_swap(
  uintptr_t                 i,
  struct _pmath_interval_t *interval
) {
  i = i & CACHE_MASK;
  
  return (void *)pmath_atomic_fetch_set(&interval_cache[i], (intptr_t)interval);
}

static void interval_cache_clear(void) {
  uintptr_t i;
  
  for(i = 0; i < CACHE_SIZE; ++i) {
    struct _pmath_interval_t *interval = interval_cache_swap(i, NULL);
    
    if(interval) {
      assert(interval->inherited.refcount._data == 0);
      
      mpfi_clear(interval->value);
      pmath_mem_free(interval);
    }
  }
}

PMATH_PRIVATE pmath_interval_t _pmath_create_interval(mpfr_prec_t precision) {
  struct _pmath_interval_t *interval;
  uintptr_t i;
  
  /*if(precision == 0)
    precision = mpfr_get_default_prec();
  else */if(precision < MPFR_PREC_MIN)
    precision = MPFR_PREC_MIN;
  else if(precision > PMATH_MP_PREC_MAX) // MPFR_PREC_MAX is too big! (not enough memory)
    precision =       PMATH_MP_PREC_MAX; // overflow error message?
    
  i = interval_cache_inc(-1);
  interval = interval_cache_swap(i - 1, NULL);
  if(interval) {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&interval_cache_hits, 1);
#endif
    
    assert(interval->inherited.refcount._data == 0);
    pmath_atomic_write_release(&interval->inherited.refcount, 1);
    
    mpfi_set_prec(interval->value, precision);
    return PMATH_FROM_PTR(interval);
  }
  else {
#ifdef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&interval_cache_misses, 1);
#endif
  }
  
  interval = (void *)PMATH_AS_PTR(_pmath_create_stub(
                                    PMATH_TYPE_SHIFT_INTERVAL,
                                    sizeof(struct _pmath_interval_t)));
                                    
  if(!interval)
    return PMATH_NULL;
    
  mpfi_init2(interval->value, precision);
  
  return PMATH_FROM_PTR(interval);
}

PMATH_PRIVATE pmath_interval_t _pmath_create_interval_for_result(pmath_interval_t val) {
  assert(pmath_is_interval(val));
  if(pmath_refcount(val) == 1)
    return pmath_ref(val);
  
  return _pmath_create_interval(mpfi_get_prec(PMATH_AS_MP_INTERVAL(val)));
}

PMATH_PRIVATE pmath_interval_t _pmath_create_interval_for_result_with_prec(pmath_interval_t val, mp_prec_t min_precision) {
  if(pmath_refcount(val) == 1 && mpfi_get_prec(PMATH_AS_MP_INTERVAL(val)) >= min_precision)
    return pmath_ref(val);
  
  return _pmath_create_interval(min_precision);
}

//} ============================================================================

PMATH_PRIVATE
pmath_bool_t _pmath_interval_set_point(mpfi_ptr result, pmath_t value) {
  if(pmath_is_mpfloat(value)) {
    mpfi_set_fr(
      result,
      PMATH_AS_MP_VALUE(value));
    return TRUE;
  }
  
  if(pmath_is_double(value)) {
    mpfi_set_d(
      result,
      PMATH_AS_DOUBLE(value));
    return TRUE;
  }
  
  if(pmath_is_int32(value)) {
    mpfi_set_si(
      result,
      PMATH_AS_INT32(value));
    return TRUE;
  }
  
  if(pmath_is_mpint(value)) {
    mpfi_set_z(
      result,
      PMATH_AS_MPZ(value));
    return TRUE;
  }
  
  if(pmath_is_rational(value)) {
    mpq_t quot;
    
    if(pmath_is_int32(PMATH_QUOT_NUM(value))) {
      mpz_init_set_si(mpq_numref(quot), PMATH_AS_INT32(PMATH_QUOT_NUM(value)));
    }
    else {
      assert(pmath_is_mpint(PMATH_QUOT_NUM(value)));
      
      mpz_init_set(mpq_numref(quot), PMATH_AS_MPZ(PMATH_QUOT_NUM(value)));
    }
    
    if(pmath_is_int32(PMATH_QUOT_DEN(value))) {
      mpz_init_set_si(mpq_denref(quot), PMATH_AS_INT32(PMATH_QUOT_DEN(value)));
    }
    else {
      assert(pmath_is_mpint(PMATH_QUOT_DEN(value)));
      
      mpz_init_set(mpq_denref(quot), PMATH_AS_MPZ(PMATH_QUOT_DEN(value)));
    }
    
    mpfi_set_q(result, quot);
      
    mpq_clear(quot);
    
    return TRUE;
  }
  
  if(_pmath_is_infinite(value)) {
    int cls = _pmath_number_class(value);
    
    if(cls & PMATH_CLASS_POSINF) {
      mpfi_set_d(
        result,
        HUGE_VAL);
        
      return TRUE;
    }
    
    if(cls & PMATH_CLASS_NEGINF) {
      mpfi_set_d(
        result,
        -HUGE_VAL);
        
      return TRUE;
    }
    
    return FALSE;
  }
  
  return FALSE;
}

PMATH_API
pmath_t pmath_interval_from_expr(pmath_t obj) {

  /* Internal`RealInterval(left, right)
     Internal`RealInterval(left, right, precision)
   */
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_INTERNAL_REALINTERVAL)) {
    pmath_t left  = pmath_expr_get_item(obj, 1);
    pmath_t right = pmath_expr_get_item(obj, 2);
    pmath_integer_t result;
    
    double dprec;
    mpfr_prec_t prec;
    
    size_t exprlen = pmath_expr_length(obj);
    
    if(exprlen == 3) {
      pmath_t prec_obj = pmath_expr_get_item(obj, 3);
      
      if(!_pmath_to_precision(prec_obj, &dprec))
        dprec = HUGE_VAL;
        
      pmath_unref(prec_obj);
    }
    else if(exprlen == 2)
      dprec = pmath_precision(pmath_ref(obj));
    else
      dprec = HUGE_VAL;
      
    if(dprec > 0 && dprec <= PMATH_MP_PREC_MAX) {
      prec = (mpfr_prec_t)ceil(dprec);
    }
    else if(dprec == -HUGE_VAL) {
      prec = DBL_MANT_DIG;
    }
    else {
      pmath_unref(left);
      pmath_unref(right);
      return obj;
    }
    
    result = _pmath_create_interval(prec);
    if(pmath_is_null(result)) {
      pmath_unref(left);
      pmath_unref(right);
      return obj;
    }
    
    if(pmath_is_mpfloat(left) && pmath_is_mpfloat(right)) {
      mpfi_interv_fr(
        PMATH_AS_MP_INTERVAL(result),
        PMATH_AS_MP_VALUE(left),
        PMATH_AS_MP_VALUE(right));
        
      pmath_unref(left);
      pmath_unref(right);
      pmath_unref(obj);
      return result;
    }
    
    if(_pmath_interval_set_point(PMATH_AS_MP_INTERVAL(result), left)) {
      pmath_interval_t tmp = _pmath_create_interval(prec);
      
      if(!pmath_is_null(tmp) && _pmath_interval_set_point(PMATH_AS_MP_INTERVAL(tmp), right)) {
        mpfi_put(
          PMATH_AS_MP_INTERVAL(result),
          PMATH_AS_MP_INTERVAL(tmp));
          
        pmath_unref(tmp);
        pmath_unref(left);
        pmath_unref(right);
        pmath_unref(obj);
        return result;
      }
      
      pmath_unref(tmp);
    }
    
    pmath_unref(left);
    pmath_unref(right);
    pmath_unref(result);
  }
  
  return obj;
}

PMATH_PRIVATE
pmath_t _pmath_interval_exceptions(pmath_interval_t x) {
  if(mpfi_nan_p(PMATH_AS_MP_INTERVAL(x))) {
    pmath_unref(x);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  if(mpfi_is_empty(PMATH_AS_MP_INTERVAL(x))) {
    pmath_unref(x);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  return x;
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_interval_get_expr(pmath_interval_t interval) {
  pmath_t left, right;
  
  if(pmath_is_null(interval))
    return PMATH_NULL;
    
  assert(pmath_is_interval(interval));
  
  left = pmath_interval_get_left(interval);
  right = pmath_interval_get_right(interval);
  
  if(pmath_is_mpfloat(left) || pmath_is_mpfloat(right)) {
    return pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_INTERNAL_REALINTERVAL), 2,
             left,
             right);
  }
  
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_INTERNAL_REALINTERVAL), 3,
           left,
           right,
           _pmath_from_precision(
             (double)mpfi_get_prec(PMATH_AS_MP_INTERVAL(interval))));
}

// =============================================================================

static void destroy_interval(pmath_t interval) {
  uintptr_t i = interval_cache_inc(+1);
  struct _pmath_interval_t *interval_ptr;
  
  assert(pmath_refcount(interval) == 0);
  
  interval_ptr = (void *)PMATH_AS_PTR(interval);
  interval_ptr = interval_cache_swap(i, interval_ptr);
  if(interval_ptr) {
    assert(interval_ptr->inherited.refcount._data == 0);
    
    mpfi_clear(interval_ptr->value);
    pmath_mem_free(interval_ptr);
  }
}

static unsigned int hash_interval(pmath_t interval) {
  unsigned int h = 0;
  
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].left._mpfr_prec, sizeof(mpfr_prec_t), h);
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].left._mpfr_sign, sizeof(mpfr_sign_t), h);
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].left._mpfr_exp,  sizeof(mp_exp_t), h);
  
  h = incremental_hash(
        PMATH_AS_MP_INTERVAL(interval)[0].left._mpfr_d,
        sizeof(mp_limb_t) * (size_t)ceil(PMATH_AS_MP_INTERVAL(interval)[0].left._mpfr_prec / (double)mp_bits_per_limb),
        h);
        
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].right._mpfr_prec, sizeof(mpfr_prec_t), h);
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].right._mpfr_sign, sizeof(mpfr_sign_t), h);
  h = incremental_hash(&PMATH_AS_MP_INTERVAL(interval)[0].right._mpfr_exp,  sizeof(mp_exp_t), h);
  
  h = incremental_hash(
        PMATH_AS_MP_INTERVAL(interval)[0].right._mpfr_d,
        sizeof(mp_limb_t) * (size_t)ceil(PMATH_AS_MP_INTERVAL(interval)[0].right._mpfr_prec / (double)mp_bits_per_limb),
        h);
        
  return h;
}

PMATH_PRIVATE
int compare_intervals(
  pmath_t intervalA,
  pmath_t intervalB
) {
  if(mpfi_nan_p(PMATH_AS_MP_INTERVAL(intervalA)) || mpfi_nan_p(PMATH_AS_MP_INTERVAL(intervalB))) {
    return 0;
  }
  
  /* Returns -1 if all elements in A are strictly less than all elements in B,
       -"-   +1        -"-         B                   -"-                  A,
       -"-    0 if A and B overlap
   */
  return mpfi_cmp(PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_MP_INTERVAL(intervalB));
}

PMATH_PRIVATE
pmath_bool_t equal_intervals(
  pmath_t intervalA,
  pmath_t intervalB
) {
  if(mpfi_nan_p(PMATH_AS_MP_INTERVAL(intervalA)) || mpfi_nan_p(PMATH_AS_MP_INTERVAL(intervalB))) {
    return FALSE;
  }
  
  return mpfr_equal_p(&PMATH_AS_MP_INTERVAL(intervalA)[0].left,  &PMATH_AS_MP_INTERVAL(intervalB)[0].left) &&
         mpfr_equal_p(&PMATH_AS_MP_INTERVAL(intervalA)[0].right, &PMATH_AS_MP_INTERVAL(intervalB)[0].right);
}

static void write_interval(struct pmath_write_ex_t *info, pmath_t interval) {
  pmath_t as_expr = pmath_interval_get_expr(interval);
  
  pmath_write_ex(info, as_expr);
  
  pmath_unref(as_expr);
}

// =============================================================================

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_get_value(
  pmath_interval_t interval, // wont be freed
  int (*get_fn)(mpfr_ptr, mpfi_srcptr)
) {
  pmath_mpfloat_t result;
  
  if(pmath_is_null(interval))
    return PMATH_NULL;
    
  assert(pmath_is_interval(interval));
  
  if(mpfi_nan_p(PMATH_AS_MP_INTERVAL(interval)) ||
      mpfi_is_empty(PMATH_AS_MP_INTERVAL(interval)))
  {
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  result = _pmath_create_mp_float(mpfi_get_prec(PMATH_AS_MP_INTERVAL(interval)));
  if(pmath_is_null(result))
    return result;
    
  get_fn(PMATH_AS_MP_VALUE(result), PMATH_AS_MP_INTERVAL(interval));
  if(mpfr_inf_p(PMATH_AS_MP_VALUE(result))) {
    int sgn = mpfr_sgn(PMATH_AS_MP_VALUE(result));
    
    pmath_unref(result);
    if(sgn < 0)
      return pmath_ref(_pmath_object_neg_infinity);
      
    return pmath_ref(_pmath_object_pos_infinity);
  }
  
  return result;
}

PMATH_API pmath_t pmath_interval_get_left(pmath_interval_t interval) {
  return _pmath_interval_get_value(interval, mpfi_get_left);
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_interval_get_right(pmath_interval_t interval) {
  return _pmath_interval_get_value(interval, mpfi_get_right);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_call(
  pmath_interval_t   arg, // will be freed
  int              (*func)(mpfi_ptr, mpfi_srcptr)
) {
  pmath_interval_t result;
  
  if(pmath_is_null(arg))
    return arg;
  
  assert(pmath_is_interval(arg));
  result = _pmath_create_interval_for_result(arg);
  if(!pmath_is_null(result))
    func(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(arg));
  
  pmath_unref(arg);
  return result;
}

// =============================================================================

PMATH_PRIVATE void _pmath_intervals_memory_panic(void) {
  interval_cache_clear();
}

PMATH_PRIVATE pmath_bool_t _pmath_intervals_init(void) {
  pmath_debug_print("[mpfi %s]\n", mpfi_get_version());
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_INTERVAL,
    compare_intervals,
    hash_interval,
    destroy_interval,
    equal_intervals,
    write_interval);
    
  interval_cache_clear();
  return TRUE;
}

PMATH_PRIVATE void _pmath_intervals_done(void) {

  interval_cache_clear();
  
#ifdef PMATH_DEBUG_LOG
  {
    intptr_t hits   = pmath_atomic_read_aquire(&interval_cache_hits);
    intptr_t misses = pmath_atomic_read_aquire(&interval_cache_misses);
    
    pmath_debug_print("interval cache hit rate:         %f (%d of %d)\n",
                      hits / (double)(hits + misses),
                      (int) hits,
                      (int)(hits + misses));
  }
#endif
}
