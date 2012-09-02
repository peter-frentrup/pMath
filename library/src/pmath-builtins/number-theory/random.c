#include <pmath-core/expressions-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control-private.h>


struct random_int_info_t {
  mpz_ptr min;
  mpz_ptr range; // = max - min + 1 > 0
};

static pmath_t make_random_int(struct random_int_info_t *info) {
  pmath_mpint_t result = _pmath_create_mp_int(0);
  
  if(pmath_is_null(result))
    return result;
    
  pmath_atomic_lock(&_pmath_rand_spinlock);
  
  mpz_urandomm(
    PMATH_AS_MPZ(result),
    _pmath_randstate,
    info->range);
    
  pmath_atomic_unlock(&_pmath_rand_spinlock);
  
  mpz_add(
    PMATH_AS_MPZ(result),
    PMATH_AS_MPZ(result),
    info->min);
    
  return _pmath_mp_int_normalize(result);
}



struct random_mpfloat_info_t {
  mpfr_ptr       min;       // may be NULL
  mpfr_ptr       range;     // may be NULL
  double         bit_prec;
  mpfr_prec_t    round_bit_prec;
};

static pmath_t make_random_mpfloat(struct random_mpfloat_info_t *info) {
  pmath_mpfloat_t result = _pmath_create_mp_float(info->round_bit_prec);
  
  if(pmath_is_null(result))
    return result;
    
  pmath_atomic_lock(&_pmath_rand_spinlock);
  
  mpfr_urandomb(PMATH_AS_MP_VALUE(result), _pmath_randstate);
  
  pmath_atomic_unlock(&_pmath_rand_spinlock);
  
  if(info->range) {
    mpfr_mul(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_MP_VALUE(result),
      info->range,
      MPFR_RNDN);
  }
  
  if(info->min) {
    mpfr_add(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_MP_VALUE(result),
      info->min,
      MPFR_RNDN);
  }
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(result))) {
    pmath_unref(result);
    return PMATH_FROM_INT32(0);
  }
  
  mpfr_set_d( PMATH_AS_MP_ERROR(result), -info->bit_prec, MPFR_RNDN);
  mpfr_ui_pow(PMATH_AS_MP_ERROR(result), 2, PMATH_AS_MP_ERROR(result), MPFR_RNDU);
  mpfr_mul(   PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), PMATH_AS_MP_VALUE(result), MPFR_RNDU);
  mpfr_abs(   PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDN);
  
  return result;
}



struct random_double_info_t {
  double min;
  double range; // max - min
};

struct random_double_list_info_t {
  struct random_double_info_t more;
  size_t                      length;
};

static pmath_t make_random_double_list(struct random_double_list_info_t *info) {
  struct _pmath_expr_t *list;
  size_t i;
  pmath_expr_t expr;
  double d;
  MPFR_DECL_INIT(temp, DBL_MANT_DIG);
  
  list = _pmath_expr_new_noinit(info->length);
  if(!list)
    return PMATH_NULL;
    
  list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  
  pmath_atomic_lock(&_pmath_rand_spinlock);
  for(i = 1; i <= info->length; ++i) {
    mpfr_urandomb(temp, _pmath_randstate);
    
    d = mpfr_get_d(temp, MPFR_RNDN);
    d = info->more.min + d * info->more.range;
    
    list->items[i] = PMATH_FROM_DOUBLE(d);
  }
  pmath_atomic_unlock(&_pmath_rand_spinlock);
  
  expr = PMATH_FROM_PTR(list);
  _pmath_expr_update(expr); // the list is already fully evaluated
  return expr;
}

static pmath_t make_random_double(struct random_double_info_t *info) {
  double d;
  MPFR_DECL_INIT(temp, DBL_MANT_DIG);
  
  pmath_atomic_lock(&_pmath_rand_spinlock);
  {
    mpfr_urandomb(temp, _pmath_randstate);
    
    d = mpfr_get_d(temp, MPFR_RNDN);
    d = info->min + d * info->range;
  }
  pmath_atomic_unlock(&_pmath_rand_spinlock);
  
  return PMATH_FROM_DOUBLE(d);
}



static pmath_expr_t make_array(
  size_t        dimensions,
  const size_t *lengths,
  void         *context,
  pmath_t     (*generator)(void *) // needs to return an evaluated result
) {
  if(dimensions > 0) {
    struct _pmath_expr_t *list;
    size_t i;
    pmath_expr_t expr;
    
    list = _pmath_expr_new_noinit(*lengths);
    if(!list)
      return PMATH_NULL;
      
    list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
    for(i = 1; i <= *lengths; ++i)
      list->items[i] = make_array(dimensions - 1, lengths + 1, context, generator);
      
    expr = PMATH_FROM_PTR(list);
    _pmath_expr_update(expr); // the list is already fully evaluated
    return expr;
  }
  
  return (*generator)(context);
}



// range will be freed
static void get_range_min_max(pmath_t range, pmath_t *min, pmath_t *max) {
  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)) {
    *min = pmath_expr_get_item(range, 1);
    *max = pmath_expr_get_item(range, 2);
    pmath_unref(range);
    return;
  }
  
  *min = PMATH_FROM_INT32(0);
  *max = range;
}

static pmath_bool_t convert_lengths(
  pmath_t   dims, // will be freed
  size_t   *dimensions,
  size_t  **lengths
) {
  if(pmath_is_int32(dims) && PMATH_AS_INT32(dims) >= 0) {
    *lengths = pmath_mem_alloc(sizeof(size_t));
    
    if(*lengths) {
      *dimensions = 1;
      *lengths[0] = (size_t)PMATH_AS_INT32(dims);
      return TRUE;
    }
    
    *dimensions = 0;
    return TRUE;
  }
  
  if(pmath_is_expr_of(dims, PMATH_SYMBOL_LIST)) {
    size_t len = pmath_expr_length(dims);
    size_t i;
    
    if(len > 0) {
      *lengths = pmath_mem_alloc(sizeof(size_t) * len);
      if(*lengths) {
        *dimensions = len;
        
        for(i = 1; i <= len; ++i) {
          pmath_t item = pmath_expr_get_item(dims, i);
          
          if(pmath_is_int32(item) && PMATH_AS_INT32(item) >= 0) {
            *lengths[i] = (size_t)PMATH_AS_INT32(item);
            continue;
          }
          
          pmath_mem_free(*lengths);
          *dimensions = 0;
          *lengths = NULL;
          pmath_unref(item);
          pmath_unref(dims);
          return FALSE;
        }
      }
    }
    
    *dimensions = 0;
    pmath_unref(dims);
    return TRUE;
  }
  
  *dimensions = 0;
  *lengths = NULL;
  pmath_unref(dims);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_randominteger(pmath_expr_t expr) {
  /* RandomInteger(a..b, n)
     RandomInteger(a..b, {n1, n2, ...})
     RandomInteger(a..b)
     RandomInteger(b)     = RandomInteger(0..b)
     RandomInteger()      = RandomInteger(0..1)
  
     Messages:
       General::range
   */
  pmath_mpint_t min, range;
  struct random_int_info_t info;
  size_t exprlen;
  size_t dimensions;
  size_t *lengths;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen > 2) {
    pmath_message_argxxx(exprlen, 0, 2);
    return expr;
  }
  
  if(exprlen == 0) {
    pmath_unref(expr);
    
    min   = _pmath_create_mp_int(0);
    range = _pmath_create_mp_int(2);
    if(pmath_is_null(min) || pmath_is_null(range)) {
      pmath_unref(min);
      pmath_unref(range);
      return PMATH_NULL;
    }
    
    info.min   = PMATH_AS_MPZ(min);
    info.range = PMATH_AS_MPZ(range);
    
    expr = make_random_int(&info);
    
    pmath_unref(min);
    pmath_unref(range);
    return expr;
  }
  
  if(exprlen >= 1) {
    pmath_t max;
    
    get_range_min_max(pmath_expr_get_item(expr, 1), &min, &max);
    
    if(pmath_is_int32(min)) {
      min = _pmath_create_mp_int(PMATH_AS_INT32(min));
      
      if(pmath_is_null(min)) {
        pmath_unref(max);
        return expr;
      }
    }
    
    if(pmath_is_int32(max)) {
      max = _pmath_create_mp_int(PMATH_AS_INT32(max));
      
      if(pmath_is_null(max)) {
        pmath_unref(min);
        return expr;
      }
    }
    
    if(!pmath_is_mpint(min) || !pmath_is_integer(max)) {
      pmath_unref(min);
      pmath_unref(max);
      
      pmath_message(
        PMATH_NULL, "range", 1,
        PMATH_FROM_INT32(2),
        pmath_expr_get_item(expr, 1));
        
      return expr;
    }
    
    range = _pmath_create_mp_int(0);
    if(pmath_is_null(range)) {
      pmath_unref(min);
      pmath_unref(max);
      return expr;
    }
    
    if(mpz_cmp(PMATH_AS_MPZ(min), PMATH_AS_MPZ(max)) > 0) {
      pmath_t tmp = min;
      min = max;
      max = tmp;
    }
    
    mpz_sub(   PMATH_AS_MPZ(range), PMATH_AS_MPZ(max), PMATH_AS_MPZ(min));
    mpz_add_ui(PMATH_AS_MPZ(range), PMATH_AS_MPZ(range), 1);
    
    pmath_unref(max);
    
    info.min   = PMATH_AS_MPZ(min);
    info.range = PMATH_AS_MPZ(range);
    
    if(exprlen == 1) {
      pmath_unref(expr);
      
      expr = make_random_int(&info);
      
      pmath_unref(min);
      pmath_unref(range);
      return expr;
    }
  }
  
  if(convert_lengths(pmath_expr_get_item(expr, 2), &dimensions, &lengths)) {
    pmath_unref(expr);
    
    expr = make_array(
             dimensions,
             lengths,
             &info,
             (pmath_t( *)(void *))make_random_int);
             
    pmath_mem_free(lengths);
  }
  else {
    pmath_message(
      PMATH_NULL, "ilsmn", 2,
      PMATH_FROM_INT32(2),
      pmath_ref(expr));
  }
  
  pmath_unref(min);
  pmath_unref(range);
  return expr;
}

static pmath_t force_approx(pmath_t x, double bit_prec) {
  x = pmath_approximate(x, bit_prec, HUGE_VAL, NULL);
  
  // integer 0 is not changed by pmath_approximate() with bit_prec > -HUGE_VAL
  if(pmath_is_int32(x)) { // N(0, 10) === 0
    if(bit_prec == -HUGE_VAL) {
      x = PMATH_FROM_DOUBLE((double)PMATH_AS_INT32(x));
    }
    else {
      int val = PMATH_AS_INT32(x);
      mpfr_prec_t prec = (mpfr_prec_t)ceil(bit_prec);
      
      x = _pmath_create_mp_float(prec);
      if(!pmath_is_null(x)) {
        mpfr_set_si(PMATH_AS_MP_VALUE(x), val, MPFR_RNDN);
        mpfr_set_si(PMATH_AS_MP_ERROR(x), val, MPFR_RNDN);
        mpfr_abs(   PMATH_AS_MP_ERROR(x), PMATH_AS_MP_VALUE(x), MPFR_RNDU);
        mpfr_div_2si(
          PMATH_AS_MP_ERROR(x),
          PMATH_AS_MP_ERROR(x),
          prec,
          MPFR_RNDU);
      }
    }
  }
  
  return x;
}

PMATH_PRIVATE pmath_t builtin_randomreal(pmath_expr_t expr) {
  /* RandomReal(a..b, n)
     RandomReal(a..b, {n1, n2, ...})
     RandomReal(a..b)
     RandomReal(b)     = RandomReal(0..b)
     RandomReal()      = RandomReal(0..1)
  
     Options:
       WorkingPrecision -> MachinePrecision
  
     Messages:
       General::invprec
       General::precw
       General::range
   */
  struct random_mpfloat_info_t mp_info;
  pmath_t min, max, options, opt;
  size_t *lengths        = NULL;
  size_t dimensions      = 0;
  size_t exprlen         = pmath_expr_length(expr);
  size_t last_non_option = exprlen;
  
  if(exprlen >= 2) {
    if(!convert_lengths(pmath_expr_get_item(expr, 2), &dimensions, &lengths)) {
      last_non_option = 1;
    }
  }
  
  if(exprlen >= 1) {
    pmath_t range = pmath_expr_get_item(expr, 1);
    
    if(_pmath_is_rule(range) || _pmath_is_list_of_rules(range)) {
      last_non_option = 0;
      
      pmath_unref(range);
      min = PMATH_FROM_INT32(0);
      max = PMATH_FROM_INT32(1);
    }
    else {
      get_range_min_max(range, &min, &max);
    }
  }
  else {
    min = PMATH_FROM_INT32(0);
    max = PMATH_FROM_INT32(1);
  }
  
  options = pmath_options_extract(expr, last_non_option);
  if(pmath_is_null(options)) {
    pmath_unref(min);
    pmath_unref(max);
    pmath_mem_free(lengths);
    return expr;
  }
  
  opt = pmath_option_value(
          PMATH_SYMBOL_RANDOMREAL,
          PMATH_SYMBOL_WORKINGPRECISION,
          options);
  pmath_unref(options);
  
  if( !_pmath_to_precision(opt, &mp_info.bit_prec) ||
      mp_info.bit_prec > PMATH_MP_PREC_MAX)
  {
    pmath_message(PMATH_NULL, "invprec", 1, opt);
    pmath_unref(min);
    pmath_unref(max);
    pmath_mem_free(lengths);
    return expr;
  }
  
  pmath_unref(opt);
  
  
  min = force_approx(min, mp_info.bit_prec);
  max = force_approx(max, mp_info.bit_prec);
  
  if(!pmath_is_float(min) || !pmath_is_float(max)) {
    pmath_t range = pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_RANGE), 2,
                      min, max);
                      
    pmath_message(
      PMATH_NULL, "range", 1,
      range);
      
    pmath_mem_free(lengths);
    return expr;
  }
  
  if(pmath_is_double(min) && pmath_is_double(max) && mp_info.bit_prec == -HUGE_VAL) {
    struct random_double_list_info_t d_info;
    
    d_info.more.min   = PMATH_AS_DOUBLE(min);
    d_info.more.range = PMATH_AS_DOUBLE(max) - d_info.more.min;
    
    if(isfinite(d_info.more.range)) {
      if(dimensions > 0) {
        d_info.length = lengths[dimensions - 1];
        
        pmath_unref(expr);
        expr = make_array(
                 dimensions - 1,
                 lengths,
                 &d_info,
                 (pmath_t( *)(void *))make_random_double_list);
                 
        pmath_mem_free(lengths);
        return expr;
      }
      
      pmath_unref(expr);
      expr = make_random_double(&d_info.more);
      return expr;
    }
  }
  
  {
    double min_prec = pmath_precision(pmath_ref(min));
    double max_prec = pmath_precision(pmath_ref(max));
    
    if( (0 < min_prec && min_prec < mp_info.bit_prec) ||
        (0 < max_prec && max_prec < mp_info.bit_prec))
    {
      pmath_t mima = pmath_expr_get_item(expr, 1);
      pmath_t wprec;
      
      if(mp_info.bit_prec == -HUGE_VAL)
        wprec = pmath_ref(PMATH_SYMBOL_WORKINGPRECISION);
      else
        wprec = PMATH_FROM_DOUBLE(LOG10_2 * mp_info.bit_prec);
        
      pmath_message(
        PMATH_NULL, "precw", 2,
        mima,
        wprec);
    }
  }
  
  if(pmath_is_double(min))
    min = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(min));
    
  if(!pmath_is_null(min)) {
    pmath_mpfloat_t range;
    
    assert(pmath_is_mpfloat(min));
    
    if(mp_info.bit_prec == -HUGE_VAL)
      mp_info.round_bit_prec = DBL_MANT_DIG;
    else if(mp_info.bit_prec < MPFR_PREC_MIN)
      mp_info.round_bit_prec = MPFR_PREC_MIN;
    else
      mp_info.round_bit_prec = (mpfr_prec_t)ceil(mp_info.bit_prec);
      
    range = _pmath_create_mp_float(mp_info.round_bit_prec);
    
    if(!pmath_is_null(range)) {
      if(pmath_is_double(max))
        mpfr_d_sub(PMATH_AS_MP_VALUE(range), PMATH_AS_DOUBLE(max), PMATH_AS_MP_VALUE(min), MPFR_RNDN);
      else
        mpfr_sub(PMATH_AS_MP_VALUE(range), PMATH_AS_MP_VALUE(max), PMATH_AS_MP_VALUE(min), MPFR_RNDN);
        
      mp_info.min   = PMATH_AS_MP_VALUE(min);
      mp_info.range = PMATH_AS_MP_VALUE(range);
      
      pmath_unref(expr);
      expr = make_array(
               dimensions,
               lengths,
               &mp_info,
               (pmath_t( *)(void *))make_random_mpfloat);
               
      pmath_unref(range);
      pmath_unref(min);
      pmath_unref(max);
      pmath_mem_free(lengths);
      return expr;
    }
  }
  
  pmath_unref(min);
  pmath_unref(max);
  pmath_mem_free(lengths);
  return expr;
}

