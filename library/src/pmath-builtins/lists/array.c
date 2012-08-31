#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE
pmath_symbol_t _pmath_topmost_symbol(pmath_t obj) { // obj wont be freed
  if(pmath_is_symbol(obj))
    return pmath_ref(obj);
    
  if(pmath_is_expr(obj)) {
    pmath_t head = pmath_expr_get_item(obj, 0);
    pmath_t result = _pmath_topmost_symbol(head);
    pmath_unref(head);
    return result;
  }
  
  return PMATH_NULL;
}

struct _array_data_t {
  pmath_t start;
  pmath_t dims;
  pmath_t index;
  pmath_t function;
  pmath_t head;
  
  size_t dim;
  size_t depth;
  pmath_bool_t start_is_list;
};

static pmath_t array(struct _array_data_t *data) {
  pmath_expr_t list;
  pmath_t obj;
  size_t i, len;
  
  obj = pmath_expr_get_item(data->dims, data->dim);
  len = pmath_integer_get_uiptr(obj);
  pmath_unref(obj);
  
  list = pmath_expr_new(pmath_ref(data->head), len);
  
  if(data->dim >= data->depth) {
    if(data->start_is_list) {
      for(i = 1; i <= len && !pmath_aborting(); ++i) {
        pmath_t ind = pmath_expr_get_item(data->start, data->dim);
        
        ind = PLUS(ind, pmath_integer_new_uiptr(i - 1));
        ind = pmath_evaluate(ind);
        
        data->index = pmath_expr_set_item(
                        data->index, data->dim,
                        ind);
                        
        list = pmath_expr_set_item(list, i,
                                   pmath_expr_set_item(
                                     pmath_ref(data->index), 0,
                                     pmath_ref(data->function)));
      }
    }
    else {
      for(i = 1; i <= len && !pmath_aborting(); ++i) {
        pmath_t ind = PLUS(
                        pmath_ref(data->start),
                        pmath_integer_new_uiptr(i - 1));
                        
        ind = pmath_evaluate(ind);
        
        data->index = pmath_expr_set_item(
                        data->index, data->dim,
                        ind);
                        
        list = pmath_expr_set_item(list, i,
                                   pmath_expr_set_item(
                                     pmath_ref(data->index), 0,
                                     pmath_ref(data->function)));
      }
    }
    
    _pmath_expr_update(list);
    return list;
  }
  
  data->dim++;
  if(data->start_is_list) {
    for(i = 1; i <= len && !pmath_aborting(); ++i) {
      pmath_t ind = pmath_expr_get_item(data->start, data->dim - 1);
      
      ind = PLUS(ind, pmath_integer_new_uiptr(i - 1));
      ind = pmath_evaluate(ind);
      
      data->index = pmath_expr_set_item(
                      data->index, data->dim - 1,
                      ind);
                      
      list = pmath_expr_set_item(list, i, array(data));
    }
  }
  else {
    for(i = 1; i <= len && !pmath_aborting(); ++i) {
      pmath_t ind = PLUS(
                      pmath_ref(data->start),
                      pmath_integer_new_uiptr(i - 1));
                      
      ind = pmath_evaluate(ind);
      
      data->index = pmath_expr_set_item(
                      data->index, data->dim - 1,
                      ind);
                      
      list = pmath_expr_set_item(list, i, array(data));
    }
  }
  data->dim--;
  
  
  _pmath_expr_update(list);
  return list;
}

// precondition: INT32_MIN < start + len * delta < INT32_MAX
static pmath_t array_range_int(int start, int delta, size_t len) {
  pmath_t expr;
  struct _pmath_expr_t *list = _pmath_expr_new_noinit(len);
  size_t i;
  
  if(!list)
    return PMATH_NULL;
    
  list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = 1; i <= len; ++i) {
    list->items[i] = PMATH_FROM_INT32(start);
    
    start += delta;
  }
  
  expr = PMATH_FROM_PTR(list);
  _pmath_expr_update(expr);
  return expr;
}

// precondition: -Infinity < start + len * delta < Infinity; no NaN's.
static pmath_t array_range_double(double start, double delta, size_t len) {
  pmath_t expr;
  struct _pmath_expr_t *list = _pmath_expr_new_noinit(len);
  size_t i;
  
  if(!list)
    return PMATH_NULL;
    
  list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = 1; i <= len; ++i) {
    list->items[i] = PMATH_FROM_DOUBLE(start);
    
    start += delta;
  }
  
  expr = PMATH_FROM_PTR(list);
  _pmath_expr_update(expr);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_array(pmath_expr_t expr) {
  /* Array(f, n)
     Array(f, {n1, n2, ...})
     Array(f, {n1, n2, ...}, {s1, s2, ...})
     Array(f, dims, origs, h)
  
     Array(s..e..d)
     Array(s..e)
     Array(e)
  
     messages:
       General::range
   */
  struct _array_data_t data;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 1) {
    pmath_t range = pmath_expr_get_item(expr, 1);
    pmath_t delta;
    size_t len;
    
    if(!extract_delta_range(range, &data.start, &delta, &len)) {
      pmath_unref(data.start);
      pmath_unref(delta);
      pmath_message(PMATH_NULL, "range", 1, range);
      return expr;
    }
    
    pmath_unref(range);
    pmath_unref(expr);
    
    if(len > 0) {
      if(pmath_is_double(data.start)) {
        if(pmath_is_int32(delta))
          delta = PMATH_FROM_DOUBLE((double)PMATH_AS_INT32(delta));
      }
      
      if(pmath_is_double(delta)){
        if(pmath_is_double(data.start)) {
          double s = PMATH_AS_DOUBLE(data.start);
          double d = PMATH_AS_DOUBLE(delta);
          double e = s + d * len;
          
          if(isfinite(e))
            return array_range_double(s, d, len);
        }
        
        if(pmath_is_int32(data.start)) {
          int s = PMATH_AS_INT32(data.start);
          double d = PMATH_AS_DOUBLE(delta);
          double e = s + d * len;
          
          if(isfinite(e)) {
            expr = array_range_double((double)s, d, len);
            expr = pmath_expr_set_item(expr, 1, data.start);
            return expr;
          }
        }
      }
      
      if(pmath_is_int32(data.start) && pmath_is_int32(delta)) {
        int s = PMATH_AS_INT32(data.start);
        int d = PMATH_AS_INT32(delta);
        
        // check that at least INT32_MIN < s + len * d < INT32_MAX
        
        if(d > 0) {
          int max = INT32_MAX;
          if(s > 0)
            max -= s;
            
          if(len < (size_t)(max / d))
            return array_range_int(s, d, len);
        }
        else if(d > INT32_MIN){
          int min = INT32_MIN;
          assert(d < 0);
          
          if(s < 0)
            min -= s;
          
          if(len < (size_t)(min / d))
            return array_range_int(s, d, len);
        }
      }
    }
    
    expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
    if(!pmath_is_null(expr) && len > 0) {
      expr = pmath_expr_set_item(expr, 1, pmath_ref(data.start));
      
      for(data.dim = 2; data.dim <= len && !pmath_aborting(); data.dim++) {
        data.start = pmath_evaluate(PLUS(data.start, pmath_ref(delta)));
        
        expr = pmath_expr_set_item(expr, data.dim, pmath_ref(data.start));
      }
    }
    
    pmath_unref(data.start);
    pmath_unref(delta);
    
    _pmath_expr_update(expr);
    return expr;
  }
  
  if(exprlen < 1 || exprlen > 4) {
    pmath_message_argxxx(exprlen, 1, 4);
    return expr;
  }
  
  data.dims = pmath_expr_get_item(expr, 2);
  if(!pmath_is_expr_of(data.dims, PMATH_SYMBOL_LIST))
    data.dims = pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_LIST), 1,
                  data.dims);
                  
  data.depth = pmath_expr_length(data.dims);
  for(data.dim = data.depth; data.dim > 0; data.dim--) {
    pmath_t d = pmath_expr_get_item(data.dims, data.dim);
    
    if(!pmath_is_int32(d) || PMATH_AS_INT32(d) < 0) {
      pmath_unref(d);
      pmath_unref(data.dims);
      pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(d);
  }
  
  data.start_is_list = FALSE;
  if(exprlen >= 3) {
    data.start = pmath_expr_get_item(expr, 3);
    if(pmath_is_expr_of(data.start, PMATH_SYMBOL_LIST)) {
      if(pmath_expr_length(data.start) != data.depth) {
        pmath_message(PMATH_NULL, "plen", 2, data.dims, data.start);
        return expr;
      }
      
      data.start_is_list = TRUE;
    }
  }
  else
    data.start = PMATH_FROM_INT32(1);
    
  data.function = pmath_expr_get_item(expr, 1);
  if(exprlen == 4)
    data.head = pmath_expr_get_item(expr, 4);
  else
    data.head = pmath_ref(PMATH_SYMBOL_LIST);
    
  data.dim = 1;
  data.index = pmath_expr_new(PMATH_NULL, data.depth);
  pmath_unref(expr);
  expr = array(&data);
  
  pmath_unref(data.function);
  pmath_unref(data.head);
  pmath_unref(data.dims);
  pmath_unref(data.start);
  pmath_unref(data.index);
  return expr;
}

#define MAX_DIM 10

// frees c
static pmath_t const_list(pmath_t c, size_t length) {
  pmath_expr_t expr;
  struct _pmath_expr_t *list = _pmath_expr_new_noinit(length);
  size_t i;
  
  if(!list)
    return PMATH_NULL;
  
  if(pmath_is_pointer(c)) {
    (void)pmath_atomic_fetch_add(&(PMATH_AS_PTR(c)->refcount), (intptr_t)length - 1);
  }
  
  list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = length;i > 0;--i)
    list->items[i] = c;
  
  expr = PMATH_FROM_PTR(list);
  _pmath_expr_update(expr);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_constantarray(pmath_expr_t expr) {
  /* ConstantArray(c, n)
     ConstantArray(c, {n1, n2, ...})
  
     messages:
       General::ilsmn
   */
  pmath_t c, n;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  c = pmath_expr_get_item(expr, 1);
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0){
    pmath_unref(expr);
    return const_list(c, (size_t)PMATH_AS_INT32(n));
  }
  
  if(pmath_is_expr_of(n, PMATH_SYMBOL_LIST)) {
    size_t depth = pmath_expr_length(n);
    size_t i;
    
    for(i = depth;i > 0;--i) {
      pmath_t ni = pmath_expr_get_item(n, i);
      
      if(pmath_is_int32(ni) && PMATH_AS_INT32(ni) >= 0){
        c = const_list(c, (size_t)PMATH_AS_INT32(n));
        continue;
      }
      
      pmath_unref(ni);
      pmath_unref(n);
      pmath_unref(c);
      pmath_message(
        PMATH_NULL, "ilsmn", 2,
        PMATH_FROM_INT32(2),
        pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(n);
    pmath_unref(expr);
    return c;
  }
  
  pmath_unref(c);
  pmath_unref(n);
  pmath_message(
    PMATH_NULL, "ilsmn", 2,
    PMATH_FROM_INT32(2),
    pmath_ref(expr));
  return expr;
}
