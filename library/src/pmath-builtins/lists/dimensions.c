#include <pmath-core/numbers-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_bool_t _pmath_is_vector(pmath_t v) {
  pmath_t item;
  size_t i;
  
  if(pmath_is_packed_array(v))
    return pmath_packed_array_get_dimensions(v) == 1;
    
  if(!pmath_is_expr_of(v, PMATH_SYMBOL_LIST))
    return FALSE;
    
  i = pmath_expr_length(v);
  for(; i > 0; --i) {
    item = pmath_expr_get_item(v, i);
    
    if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)) {
      pmath_unref(item);
      return FALSE;
    }
    
    pmath_unref(item);
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_matrix(
  pmath_t       m,
  size_t       *rows,
  size_t       *cols,
  pmath_bool_t  check_non_list_entries
) {
  pmath_expr_t row;
  size_t i;
  
  if(pmath_is_packed_array(m)) {
    const size_t *sizes;
    
    if(check_non_list_entries) {
      if(pmath_packed_array_get_dimensions(m) != 2)
        return FALSE;
    }
    else {
      if(pmath_packed_array_get_dimensions(m) < 2)
        return FALSE;
    }
    
    sizes = pmath_packed_array_get_sizes(m);
    *rows = sizes[0];
    *cols = sizes[1];
    
    return TRUE;
  }
  
  *cols = *rows = 0;
  if(!pmath_is_expr_of(m, PMATH_SYMBOL_LIST))
    return FALSE;
    
  *rows = pmath_expr_length(m);
  if(*rows == 0)
    return TRUE;
    
  row = pmath_expr_get_item(m, 1);
  if(!pmath_is_expr_of(row, PMATH_SYMBOL_LIST)) {
    pmath_unref(row);
    return FALSE;
  }
  
  *cols = pmath_expr_length(row);
  pmath_unref(row);
  
  for(i = *rows; i > 1; --i) {
    row = pmath_expr_get_item(m, i);
    
    if( !pmath_is_expr_of_len(row, PMATH_SYMBOL_LIST, *cols) ||
        (check_non_list_entries &&
         !_pmath_is_vector(row)))
    {
      pmath_unref(row);
      return FALSE;
    }
    
    pmath_unref(row);
  }
  
  return TRUE;
}

typedef struct {
  pmath_t   head;
  size_t    maxdim;
  size_t   *dim_arr;
} dims_data_t;

static void check_rest_dims(
  dims_data_t     *data,
  pmath_t          obj,  // wont be freed
  size_t           level
) {
  pmath_t tmp;
  size_t i;
  
  if(level >= data->maxdim)
    return;
    
  if(!pmath_is_expr(obj) || pmath_expr_length(obj) != data->dim_arr[level]) {
    data->maxdim = level;
    return;
  }
  
  tmp = pmath_expr_get_item(obj, 0);
  if(!pmath_equals(tmp, data->head)) {
    pmath_unref(tmp);
    data->maxdim = level;
    return;
  }
  
  pmath_unref(tmp);
  for(i = pmath_expr_length(obj); i > 0; --i) {
    tmp = pmath_expr_get_item(obj, i);
    
    check_rest_dims(data, tmp, level + 1);
    
    pmath_unref(tmp);
  }
}

static pmath_bool_t sizes_fit_int32(const size_t *sizes, size_t length) {
  size_t i;
  
  for(i = 0; i < length; ++i) {
    if(sizes[i] > INT32_MAX)
      return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE
pmath_expr_t _pmath_sizes_to_expr(const size_t *sizes, size_t length) {
  size_t i;
  
  if(sizes_fit_int32(sizes, length)) {
    pmath_blob_t blob = pmath_blob_new(length * sizeof(int32_t), FALSE);
    
    int32_t *data = pmath_blob_try_write(blob);
    if(data) {
      for(i = 0; i < length; ++i)
        data[i] = (int32_t)sizes[i];
    }
    
    return pmath_packed_array_new(
             blob,
             PMATH_PACKED_INT32,
             1,
             &length,
             NULL,
             0);
  }
  else {
    pmath_expr_t list;
    pmath_t item;
    
    list = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), length);
    for(i = 0; i < length;) {
      item = pmath_integer_new_uiptr(sizes[i++]);
      list = pmath_expr_set_item(list, i, item);
    }
    
    return list;
  }
}

PMATH_PRIVATE pmath_expr_t _pmath_dimensions(
  pmath_t obj, // wont be freed
  size_t maxdepth
) {
  dims_data_t data;
  size_t dims, i;
  pmath_t tmp, item;
  
  if(!pmath_is_expr(obj) || maxdepth == 0)
    return pmath_ref(_pmath_object_emptylist);
    
  if(pmath_is_packed_array(obj)) {
    data.maxdim = pmath_packed_array_get_dimensions(obj);
    
    if(data.maxdim > maxdepth)
      data.maxdim = maxdepth;
    
    return _pmath_sizes_to_expr(pmath_packed_array_get_sizes(obj), data.maxdim);
  }
  
  data.head = pmath_expr_get_item(obj, 0);
  
  dims = 1;
  tmp = pmath_expr_get_item(obj, 1);
  while(dims < maxdepth && pmath_is_expr(tmp)) {
    item = pmath_expr_get_item(tmp, 0);
    
    if(!pmath_equals(item, data.head)) {
      pmath_unref(item);
      break;
    }
    
    ++dims;
    if(dims < maxdepth) {
      pmath_unref(item);
      item = pmath_expr_get_item(tmp, 1);
      pmath_unref(tmp);
      tmp = item;
    }
  }
  pmath_unref(tmp);
  
  data.dim_arr = (size_t *)pmath_mem_alloc(dims * sizeof(size_t));
  if(!data.dim_arr)
    return PMATH_NULL;
    
  data.maxdim = dims;
  tmp = pmath_ref(obj);
  for(i = 0;;) {
    data.dim_arr[i++] = pmath_expr_length(tmp);
    
    if(i >= dims)
      break;
      
    item = pmath_expr_get_item(tmp, 1);
    pmath_unref(tmp);
    tmp = item;
    
  }
  pmath_unref(tmp);
  
  check_rest_dims(&data, obj, 0);
  
  tmp = _pmath_sizes_to_expr(data.dim_arr, data.maxdim);
  
  pmath_mem_free(data.dim_arr);
  pmath_unref(data.head);
  
  return tmp;
}

PMATH_PRIVATE pmath_t builtin_dimensions(pmath_expr_t expr) {
  /* Dimensions(obj, n)
  
     Dimensions(obj)  ==  Dimensions(obj, Infinity)
   */
  pmath_t obj;
  size_t maxdepth = SIZE_MAX;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2) {
    obj = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_integer(obj) && pmath_number_sign(obj) >= 0) {
      if(pmath_is_int32(obj))
        maxdepth = (size_t)PMATH_AS_INT32(obj);
    }
    else if(!pmath_equals(obj, _pmath_object_pos_infinity)) {
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
