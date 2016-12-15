#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/logic-private.h>


static double minf(double a, double b) {
  if(a <= b)
    return a;
  if(b <= a)
    return b;
  if(isnan(a))
    return a;
  return b;
}

static double maxf(double a, double b) {
  if(a <= b)
    return b;
  if(b <= a)
    return a;
  if(isnan(a))
    return a;
  return b;
}

static int32_t mini(int32_t a, int32_t b) {
  if(a < b)
    return a;
  return b;
}

static int32_t maxi(int32_t a, int32_t b) {
  if(a < b)
    return b;
  return a;
}

static void packed_double_array_minmax_with(pmath_packed_array_t array, double *inout_min, double *inout_max) { // array will be freed
  size_t noncont_dims = pmath_packed_array_get_non_continuous_dimensions(array);
  size_t length;
  size_t step_size;
  const double *data;
  size_t i;
  
  if(noncont_dims > 0) {
    size_t i;
    
    for(i = pmath_expr_length(array); i > 0; --i)
      packed_double_array_minmax_with(pmath_expr_get_item(array, i), inout_min, inout_max);
      
    return;
  }
  
  length = *pmath_packed_array_get_sizes(array);
  step_size = *pmath_packed_array_get_steps(array);
  data = pmath_packed_array_read(array, NULL, 0);
  for(i = 0; i < length * step_size; ++i) {
    *inout_min = minf(*inout_min, data[i]);
    *inout_max = maxf(*inout_max, data[i]);
  }
  pmath_unref(array);
}

static void packed_int32_array_minmax_with(pmath_packed_array_t array, int32_t *inout_min, int32_t *inout_max) { // array will be freed
  size_t noncont_dims = pmath_packed_array_get_non_continuous_dimensions(array);
  size_t length;
  size_t step_size;
  const int32_t *data;
  size_t i;
  
  if(noncont_dims > 0) {
    size_t i;
    
    for(i = pmath_expr_length(array); i > 0; --i)
      packed_int32_array_minmax_with(pmath_expr_get_item(array, i), inout_min, inout_max);
      
    return;
  }
  
  length = *pmath_packed_array_get_sizes(array);
  step_size = *pmath_packed_array_get_steps(array);
  data = pmath_packed_array_read(array, NULL, 0);
  for(i = 0; i < length * step_size; ++i) {
    *inout_min = mini(*inout_min, data[i]);
    *inout_max = maxi(*inout_max, data[i]);
  }
  pmath_unref(array);
}

static void packed_array_minmax(pmath_packed_array_t array, pmath_t *optout_min, pmath_t *optout_max) { // array will be freed
  switch(pmath_packed_array_get_element_type(array)) {
    case PMATH_PACKED_DOUBLE: {
        double min = HUGE_VAL;
        double max = -HUGE_VAL;
        
        packed_double_array_minmax_with(array, &min, &max);
        
        if(optout_min) *optout_min = _pmath_packed_element_unbox(&min, PMATH_PACKED_DOUBLE);
        if(optout_max) *optout_max = _pmath_packed_element_unbox(&max, PMATH_PACKED_DOUBLE);
      }
      return;
      
    case PMATH_PACKED_INT32: {
        int32_t min = INT32_MAX;
        int32_t max = INT32_MIN;
        
        packed_int32_array_minmax_with(array, &min, &max);
        
        if(optout_min) *optout_min = _pmath_packed_element_unbox(&min, PMATH_PACKED_INT32);
        if(optout_max) *optout_max = _pmath_packed_element_unbox(&max, PMATH_PACKED_INT32);
      }
      return;
  }
  
  assert(0 && "unknown pmath_packed_array_get_element_type()");
  
  pmath_unref(array);
  if(optout_min) *optout_min = PMATH_UNDEFINED;
  if(optout_max) *optout_max = PMATH_UNDEFINED;
  return;
}

static void interval_minmax(pmath_interval_t interval, pmath_t *optout_min, pmath_t *optout_max) { // array will be freed
  mpfr_srcptr left  = &(PMATH_AS_MP_INTERVAL(interval)->left);
  mpfr_srcptr right = &(PMATH_AS_MP_INTERVAL(interval)->right);
  pmath_mpfloat_t min = _pmath_create_mp_float(mpfr_get_prec(left));
  pmath_mpfloat_t max = _pmath_create_mp_float(mpfr_get_prec(right));
  if(pmath_is_null(min) || pmath_is_null(max)) {
    pmath_unref(min);
    pmath_unref(max);
    pmath_unref(interval);
    if(optout_min) *optout_min = PMATH_UNDEFINED;
    if(optout_max) *optout_max = PMATH_UNDEFINED;
    return;
  }
  mpfr_set(PMATH_AS_MP_VALUE(min), left,  MPFR_RNDD);
  mpfr_set(PMATH_AS_MP_VALUE(max), right, MPFR_RNDU);
  
  if(optout_min) *optout_min = pmath_ref(min);
  if(optout_max) *optout_max = pmath_ref(max);
  
  pmath_unref(min);
  pmath_unref(max);
  pmath_unref(interval);
}

static void minmax(pmath_t item, pmath_t *optout_min, pmath_t *optout_max);

// return first index after start, where comparison failed.
static size_t list_minmax_step(pmath_expr_t list, size_t start, pmath_t *optinout_min, pmath_t *optinout_max, pmath_t *fail_min, pmath_t *fail_max) {
  size_t len = pmath_expr_length(list);
  
  *fail_min = PMATH_UNDEFINED;
  *fail_max = PMATH_UNDEFINED;
  
  while(++start <= len) {
    pmath_bool_t is_unknown = FALSE;
    pmath_t item = pmath_expr_get_item(list, start);
    if(pmath_same(item, PMATH_UNDEFINED))
      continue;
    
    minmax(item, optinout_min ? fail_min : NULL, optinout_max ? fail_max : NULL);
    
    if(optinout_min) {
      int cmp = _pmath_numeric_order(*fail_min, *optinout_min, PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL);
      if(cmp == TRUE) {
        pmath_unref(*optinout_min);
        *optinout_min = *fail_min;
        *fail_min = PMATH_UNDEFINED;
      }
      else if(cmp != FALSE) {
        is_unknown = TRUE;
      }
    }
    
    if(optinout_max) {
      int cmp = _pmath_numeric_order(*fail_max, *optinout_max, PMATH_DIRECTION_GREATER | PMATH_DIRECTION_EQUAL);
      if(cmp == TRUE) {
        pmath_unref(*optinout_max);
        *optinout_max = *fail_max;
        *fail_max = PMATH_UNDEFINED;
      }
      else if(cmp != FALSE) {
        is_unknown = TRUE;
      }
    }
    
    if(is_unknown)
      return start;
    
    pmath_unref(*fail_min); *fail_min = PMATH_UNDEFINED;
    pmath_unref(*fail_max); *fail_max = PMATH_UNDEFINED;
  }
  
  return len + 1;
}

/* directions is PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL for min
   and PMATH_DIRECTION_GREATER | PMATH_DIRECTION_EQUAL for max
 */
static pmath_expr_t list_min_or_max_rest(pmath_expr_t list, int directions) {
  size_t start;
  size_t len = pmath_expr_length(list);
  
  for(start = 2; start < len; ++start) {
    pmath_t item = pmath_expr_extract_item(list, start);
    if(pmath_same(item, PMATH_UNDEFINED))
      continue;
    
    if(start < len) {
      size_t next;
      for(next = start + 1; next <= len; ++next) {
        pmath_t next_item = pmath_expr_get_item(list, next);
        int cmp;
        
        if(pmath_same(next_item, PMATH_UNDEFINED))
          continue;
        
        cmp = _pmath_numeric_order(item, next_item, directions);
        if(cmp == TRUE) {
          list = pmath_expr_set_item(list, next, PMATH_UNDEFINED);
          pmath_unref(next_item);
        }
        else if(cmp == FALSE) {
          list = pmath_expr_set_item(list, next, PMATH_UNDEFINED);
          item = next_item;
        }
        else
          pmath_unref(next_item);
      }
    }
    list = pmath_expr_set_item(list, start, item);
  }
  
  return pmath_expr_remove_all(list, PMATH_UNDEFINED);
}

static void list_minmax(pmath_expr_t list, pmath_t *optout_min, pmath_t *optout_max) {
  size_t len = pmath_expr_length(list);
  size_t start, next;
  pmath_t item;
  pmath_t min_list, max_list, next_min, next_max;
  
  if(len == 0) {
    pmath_unref(list);
    if(optout_min) *optout_min = pmath_ref(_pmath_object_pos_infinity);
    if(optout_max) *optout_max = pmath_ref(_pmath_object_neg_infinity);
    return;
  }
  
  start = 1;
  item = pmath_expr_get_item(list, start);
  minmax(item, optout_min, optout_max);
  
  next = list_minmax_step(list, start, optout_min, optout_max, &next_min, &next_max);
  if(next > len) {
    pmath_unref(list);
    pmath_unref(next_min);
    pmath_unref(next_max);
    return;
  }
  
  for(++start; start < next; ++start) 
    list = pmath_expr_set_item(list, start, PMATH_UNDEFINED);
  
  if(optout_min) min_list = pmath_expr_set_item(pmath_ref(list), 0, pmath_ref(PMATH_SYMBOL_MIN)); 
  if(optout_max) max_list = pmath_expr_set_item(pmath_ref(list), 0, pmath_ref(PMATH_SYMBOL_MAX));
  
  pmath_unref(list); list = PMATH_UNDEFINED;
  
  for(;next <= len;++next) {
    if(optout_min) min_list = pmath_expr_set_item(min_list, next, next_min);
    if(optout_max) max_list = pmath_expr_set_item(max_list, next, next_max);
    item = pmath_expr_get_item(optout_min ? min_list : max_list, next + 1);
    minmax(item, optout_min ? &next_min : NULL, optout_max ? &next_max : NULL);
  }
  
  if(optout_min) {
    min_list = pmath_expr_set_item(min_list, 1, *optout_min);
    *optout_min = list_min_or_max_rest(min_list, PMATH_DIRECTION_LESS | PMATH_DIRECTION_EQUAL);
  }
  if(optout_max) {
    max_list = pmath_expr_set_item(max_list, 1, *optout_max);
    *optout_max = list_min_or_max_rest(max_list, PMATH_DIRECTION_GREATER | PMATH_DIRECTION_EQUAL);
  }
}

static void minmax(pmath_t item, pmath_t *optout_min, pmath_t *optout_max) { // item will be freed
  if(pmath_is_packed_array(item)) {
    packed_array_minmax(item, optout_min, optout_max);
    return;
  }
  if(pmath_is_interval(item)) {
    interval_minmax(item, optout_min, optout_max);
    return;
  }
  if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
    list_minmax(item, optout_min, optout_max);
    return;
  }
  if(optout_min) *optout_min = pmath_ref(item);
  if(optout_max) *optout_max = pmath_ref(item);
  pmath_unref(item);
}

PMATH_PRIVATE pmath_t builtin_max(pmath_expr_t expr) {
  list_minmax(expr, NULL, &expr);
  
  if(!pmath_is_expr(expr))
    return expr;
    
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_MAX)) 
    expr = pmath_expr_flatten(expr, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
    
  return expr;
}

PMATH_PRIVATE pmath_t builtin_min(pmath_expr_t expr) {
  list_minmax(expr, &expr, NULL);
  
  if(!pmath_is_expr(expr))
    return expr;
    
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_MAX)) 
    expr = pmath_expr_flatten(expr, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
    
  return expr;
}

PMATH_PRIVATE pmath_t builtin_minmax(pmath_expr_t expr) {
/* MinMax(list)
   MinMax(list, err)
   MinMax(list, {minerr, maxerr})
   
   TODO: MinMax(list, Scaled(err))
 */
  pmath_t list, min, max;
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
  }
  
  list = pmath_expr_get_item(expr, 1);
  minmax(list, &min, &max);
  
  if(exprlen == 2) {
    pmath_t err = pmath_expr_get_item(expr, 2);
    pmath_t min_err, max_err;
    if(pmath_is_expr_of_len(err, PMATH_SYMBOL_LIST, 2)) {
      min_err = pmath_expr_get_item(err, 1);
      max_err = pmath_expr_get_item(err, 2);
      pmath_unref(err);
    }
    else {
      min_err = pmath_ref(err);
      max_err = err;
    }
    
    min = MINUS(min, min_err);
    max = PLUS(min, max_err);
  }
  
  pmath_unref(expr);
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_LIST), 2,
    min, 
    max);
}
