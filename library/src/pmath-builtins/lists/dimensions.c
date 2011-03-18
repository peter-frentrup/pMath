#include <pmath-core/numbers-private.h>

#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_bool_t _pmath_is_matrix(
  pmath_t m,
  size_t *rows,
  size_t *cols
){
  pmath_t head;
  pmath_expr_t row;
  size_t i;
  
  *cols = *rows = 0;
  if(!pmath_is_expr(m))
    return FALSE;
  
  head = pmath_expr_get_item(m, 0);
  pmath_unref(head);
  
  if(!pmath_same(head, PMATH_SYMBOL_LIST))
    return FALSE;
  
  *rows = pmath_expr_length(m);
  if(*rows == 0)
    return TRUE;
  
  row = pmath_expr_get_item(m, 1);
  if(!pmath_is_expr(row)){
    pmath_unref(row);
    return FALSE;
  }
  
  *cols = pmath_expr_length(row);
  head = pmath_expr_get_item(row, 0);
  pmath_unref(head);
  pmath_unref(row);
  
  if(!pmath_same(head, PMATH_SYMBOL_LIST))
    return FALSE;
  
  for(i = *rows;i > 1;--i){
    row = pmath_expr_get_item(m, 1);
    if(!pmath_is_expr(row) || pmath_expr_length(row) != *cols){
      pmath_unref(row);
      return FALSE;
    }
    head = pmath_expr_get_item(row, 0);
    pmath_unref(head);
    pmath_unref(row);
    if(!pmath_same(head, PMATH_SYMBOL_LIST))
      return FALSE;
  }
  return TRUE;
}

typedef struct{
  pmath_t   head;
  size_t    maxdim;
  size_t   *dim_arr;
}dims_data_t;

static void check_rest_dims(
  dims_data_t     *data,
  pmath_t          obj,  // wont be freed
  size_t           level
){
  pmath_t tmp;
  size_t i;
  
  if(level >= data->maxdim)
    return;
    
  if(!pmath_is_expr(obj) || pmath_expr_length(obj) != data->dim_arr[level]){
    data->maxdim = level;
    return;
  }
  
  tmp = pmath_expr_get_item(obj, 0);
  if(!pmath_equals(tmp, data->head)){
    pmath_unref(tmp);
    data->maxdim = level;
    return;
  }
  
  pmath_unref(tmp);
  for(i = pmath_expr_length(obj);i > 0;--i){
    tmp = pmath_expr_get_item(obj, i);
    
    check_rest_dims(data, tmp, level + 1);
    
    pmath_unref(tmp);
  }
}

PMATH_PRIVATE pmath_expr_t _pmath_dimensions(
  pmath_t obj, // wont be freed
  size_t maxdepth
){
  dims_data_t data;
  size_t dims, i;
  pmath_t tmp, item;
  
  if(!pmath_is_expr(obj) || maxdepth == 0)
    return pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
  
  data.head = pmath_expr_get_item(obj, 0);
  
  dims = 1;
  tmp = pmath_expr_get_item(obj, 1);
  while(dims < maxdepth && pmath_is_expr(tmp)){
    item = pmath_expr_get_item(tmp, 0);
    
    if(!pmath_equals(item, data.head)){
      pmath_unref(item);
      break;
    }
    
    ++dims;
    if(dims < maxdepth){
      pmath_unref(item);
      item = pmath_expr_get_item(tmp, 1);
      pmath_unref(tmp);
      tmp = item;
    }
  }
  pmath_unref(tmp);
  
  data.dim_arr = (size_t*)pmath_mem_alloc(dims * sizeof(size_t));
  if(!data.dim_arr)
    return PMATH_NULL;
  
  data.maxdim = dims;
  tmp = pmath_ref(obj);
  for(i = 0;;){
    data.dim_arr[i++] = pmath_expr_length(tmp);
    
    if(i >= dims)
      break;
    
    item = pmath_expr_get_item(tmp, 1);
    pmath_unref(tmp);
    tmp = item;
    
  }
  pmath_unref(tmp);
  
  check_rest_dims(&data, obj, 0);
  
  tmp = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), data.maxdim);
  for(i = 0;i < data.maxdim;){
    item = pmath_integer_new_uiptr(data.dim_arr[i++]);
    tmp = pmath_expr_set_item(tmp, i, item);
  }
  
  pmath_mem_free(data.dim_arr);
  pmath_unref(data.head);
  
  return tmp;
}

PMATH_PRIVATE pmath_t builtin_dimensions(pmath_expr_t expr){
/* Dimensions(obj, n)
   
   Dimensions(obj)  ==  Dimensions(obj, Infinity)
 */
  pmath_t obj;
  size_t maxdepth = SIZE_MAX;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2){
    obj = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_integer(obj) && pmath_number_sign(obj) >= 0){
      if(pmath_is_int32(obj))
        maxdepth = (size_t)PMATH_AS_INT32(obj);
    }
    else if(!pmath_equals(obj, _pmath_object_infinity)){
      pmath_unref(obj);
      
      pmath_message(PMATH_NULL, "innf", 2,
        PMATH_FROM_INT32(2),
        pmath_ref(expr));
      
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  expr = _pmath_dimensions(obj, maxdepth);
  pmath_unref(obj);
  return expr;
}
