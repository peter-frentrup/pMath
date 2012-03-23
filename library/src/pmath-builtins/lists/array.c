#include <pmath-core/numbers.h>

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
  
  return list;
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
    
    if(!extract_delta_range(range, &data.start, &delta, &data.depth)) {
      pmath_unref(data.start);
      pmath_unref(delta);
      pmath_message(PMATH_NULL, "range", 1, range);
      return expr;
    }
    
    pmath_unref(range);
    pmath_unref(expr);
    expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), data.depth);
    if(!pmath_is_null(expr) && data.depth > 0) {
      expr = pmath_expr_set_item(expr, 1, pmath_ref(data.start));
      
      for(data.dim = 2; data.dim <= data.depth && !pmath_aborting(); data.dim++) {
        data.start = pmath_evaluate(
                       pmath_expr_new_extended(
                         pmath_ref(PMATH_SYMBOL_PLUS), 2,
                         data.start,
                         pmath_ref(delta)));
                         
        expr = pmath_expr_set_item(expr, data.dim, pmath_ref(data.start));
      }
    }
    
    pmath_unref(data.start);
    pmath_unref(delta);
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

typedef struct {
  size_t   lengths[MAX_DIM];
  pmath_t  c;
  size_t   dim;
  size_t   dims;
} special_array_data_t;

static pmath_t special_array(special_array_data_t *data) {
  pmath_expr_t list;
  size_t i;
  
  if(data->dim == data->dims)
    return pmath_ref(data->c);
    
  data->dim++;
  
  list = pmath_expr_new(
           pmath_ref(PMATH_SYMBOL_LIST),
           data->lengths[data->dim - 1]);
           
  for(i = 1; i <= data->lengths[data->dim - 1] && !pmath_aborting(); ++i) {
    list = pmath_expr_set_item(list, i, special_array(data));
  }
  
  data->dim--;
  return list;
}

PMATH_PRIVATE pmath_t builtin_constantarray(pmath_expr_t expr) {
  /* ConstantArray(c, n)
     ConstantArray(c, {n1, n2, ...})
  
     messages:
       General::ilsmn
   */
  special_array_data_t data;
  pmath_t n;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_expr(n)) {
    pmath_t h = pmath_expr_get_item(n, 0);
    pmath_unref(h);
    
    if(pmath_same(h, PMATH_SYMBOL_LIST)) {
      data.dims = pmath_expr_length(n);
      if(data.dims > MAX_DIM) {
        pmath_unref(n);
        
        return expr;
      }
      
      for(data.dim = data.dims; data.dim > 0; data.dim--) {
        pmath_t l = pmath_expr_get_item(n, data.dim);
        
        if(!pmath_is_int32(l) || PMATH_AS_INT32(l) < 0) {
          pmath_unref(l);
          pmath_unref(n);
          pmath_message(
            PMATH_NULL, "ilsmn", 2,
            PMATH_FROM_INT32(2),
            pmath_ref(expr));
          return expr;
        }
        
        data.lengths[data.dim - 1] = pmath_integer_get_uiptr(l);
        
        pmath_unref(l);
      }
      
      data.c = pmath_expr_get_item(expr, 1);
      data.dim = 0;
      
      pmath_unref(n);
      pmath_unref(expr);
      
      n = special_array(&data);
      
      pmath_unref(data.c);
      return n;
    }
  }
  
  if(!pmath_is_int32(n) || PMATH_AS_INT32(n) < 0) {
    pmath_unref(n);
    pmath_message(
      PMATH_NULL, "ilsmn", 2,
      PMATH_FROM_INT32(2),
      pmath_ref(expr));
    return expr;
  }
  
  data.lengths[0] = pmath_integer_get_uiptr(n);
  data.c = pmath_expr_get_item(expr, 1);
  data.dim = 0;
  data.dims = 1;
  
  pmath_unref(n);
  pmath_unref(expr);
  
  n = special_array(&data);
  
  pmath_unref(data.c);
  return n;
}
