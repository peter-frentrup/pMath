#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Sequence;

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
  pmath_bool_t evaluate_immediately;
  pmath_bool_t mark_as_updated;
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
    pmath_bool_t has_sequence = FALSE;
    
    if(data->start_is_list) {
      for(i = 1; i <= len && !pmath_aborting(); ++i) {
        pmath_t ind = pmath_expr_get_item(data->start, data->dim);
        
        ind = PLUS(ind, pmath_integer_new_uiptr(i - 1));
        ind = pmath_evaluate(ind);
        
        data->index = pmath_expr_set_item(
                        data->index, data->dim,
                        ind);
                        
        ind = pmath_expr_set_item(
                pmath_ref(data->index), 0,
                pmath_ref(data->function));
                
        if(data->evaluate_immediately) {
          ind = pmath_evaluate(ind);
          
          if(!has_sequence && pmath_is_expr_of(ind, pmath_System_Sequence))
            has_sequence = TRUE;
        }
        
        list = pmath_expr_set_item(list, i, ind);
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
                        
        ind = pmath_expr_set_item(
                pmath_ref(data->index), 0,
                pmath_ref(data->function));
                
        if(data->evaluate_immediately) {
          ind = pmath_evaluate(ind);
          
          if(!has_sequence && pmath_is_expr_of(ind, pmath_System_Sequence))
            has_sequence = TRUE;
        }
        
        list = pmath_expr_set_item(list, i, ind);
      }
    }
    
    if(has_sequence)
      list = pmath_evaluate(list);
    else if(data->mark_as_updated)
      _pmath_expr_update(list);
      
    return list;
  }
  
  if(pmath_aborting())
    return list;
    
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
  
  if(data->mark_as_updated)
    _pmath_expr_update(list);
    
  return list;
}

// precondition: INT32_MIN < start <= start + len * delta < INT32_MAX
static pmath_t array_range_int(int start, int delta, size_t len) {
  pmath_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &len, NULL, 0);
  int32_t *data = pmath_packed_array_begin_write(&arr, NULL, 0);
  size_t i;
  
  if(!data) 
    return PMATH_NULL;
  
  for(i = 0; i < len; ++i) {
    data[i] = start;
    start+= delta;
  }
  return arr;
}

// precondition: -Infinity < start <= start + len * delta < Infinity; no NaN's.
static pmath_t array_range_double(double start, double delta, size_t len) {
  pmath_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_DOUBLE, 1, &len, NULL, 0);
  double *data = pmath_packed_array_begin_write(&arr, NULL, 0);
  size_t i;
  
  if(!data) 
    return PMATH_NULL;
  
  for(i = 0; i < len; ++i) {
    data[i] = start;
    start+= delta;
  }
  return arr;
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
      
      if(pmath_is_double(delta)) {
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
        else if(d > INT32_MIN) {
          int min = -INT32_MAX; // = INT32_MIN + 1
          assert(d < 0);
          
          if(s < 0)
            min -= s;
            
          if(len < (size_t)(min / d))
            return array_range_int(s, d, len);
        }
      }
    }
    
    expr = pmath_expr_new(pmath_ref(pmath_System_List), len);
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
  if(!pmath_is_expr_of(data.dims, pmath_System_List))
    data.dims = pmath_expr_new_extended(
                  pmath_ref(pmath_System_List), 1,
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
    if(pmath_is_expr_of(data.start, pmath_System_List)) {
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
    data.head = pmath_ref(pmath_System_List);
    
  data.dim = 1;
  data.index = pmath_expr_new(PMATH_NULL, data.depth);
  pmath_unref(expr);
  
  if(pmath_same(data.head, pmath_System_List)) {
    data.evaluate_immediately = TRUE;
    data.mark_as_updated      = TRUE;
  }
  else {
    data.evaluate_immediately = FALSE;
    data.mark_as_updated      = FALSE;
  }
  
  expr = array(&data);
  
  pmath_unref(data.function);
  pmath_unref(data.head);
  pmath_unref(data.dims);
  pmath_unref(data.start);
  pmath_unref(data.index);
  return expr;
}
