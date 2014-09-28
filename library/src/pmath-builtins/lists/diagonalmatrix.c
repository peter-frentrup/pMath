#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


static pmath_bool_t has_inexact_machine_number(pmath_expr_t vector) {
  size_t i;
  
  if(pmath_is_packed_array(vector)) {
    pmath_packed_type_t elem_type;
    
    if(pmath_packed_array_get_dimensions(vector) != 1)
      return FALSE;
    
    elem_type = pmath_packed_array_get_element_type(vector);
    
    switch(elem_type) {
      case PMATH_PACKED_INT32:
      return FALSE;
      
      case PMATH_PACKED_DOUBLE:
      return TRUE;
    }
  }
  
  assert(pmath_is_expr_of(vector, PMATH_SYMBOL_LIST));
  
  for(i = pmath_expr_length(vector);i > 0;--i) {
    pmath_t item = pmath_expr_get_item(vector, i);
    
    if(pmath_is_double(item))
      return TRUE;
    
    pmath_unref(item);
  }
  
  return FALSE;
}

static pmath_packed_array_t slow_diagonal_matrix(
  pmath_expr_t list,
  intptr_t     diag
) {
  size_t  len, i, j;
  pmath_t mat, zero;
  
  // zero need not be freed, because it is no pointer...
  if(has_inexact_machine_number(list))
    zero = PMATH_FROM_DOUBLE(0.0);
  else
    zero = PMATH_FROM_INT32(0);
  
  len = pmath_expr_length(list);
  if(diag < 0)
    len += (size_t) - diag;
  else
    len += (size_t)diag;
    
  
  mat = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
  for(i = len; i > 0; --i) {
    pmath_t row = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
    
    if(diag >= 0) {
      size_t k = i + (size_t)diag;
      
      for(j = len; j > k; --j)
        row = pmath_expr_set_item(row, j, zero);
        
      row = pmath_expr_set_item(row, k, pmath_expr_get_item(list, i));
      
      for(j = 1; j < k; ++j)
        row = pmath_expr_set_item(row, j, zero);
    }
    else if(i > (size_t) - diag) {
      size_t k = i - ((size_t) - diag);
      
      for(j = len; j > k; --j)
        row = pmath_expr_set_item(row, j, zero);
        
      row = pmath_expr_set_item(row, k, pmath_expr_get_item(list, k));
      
      for(j = 1; j < k; ++j)
        row = pmath_expr_set_item(row, j, zero);
    }
    else {
      for(j = len; j > 0; --j)
        row = pmath_expr_set_item(row, j, zero);
    }
    
    mat = pmath_expr_set_item(mat, i, row);
  }
  
  pmath_unref(list);
  
  return mat;
}

static pmath_packed_array_t packed_diagonal_matrix(
  pmath_packed_array_t list,
  intptr_t             diag
) {
  size_t mat_sizes[2];
  size_t list_length = pmath_packed_array_get_sizes(list)[0];
  pmath_packed_type_t elem_type = pmath_packed_array_get_element_type(list);
  pmath_packed_array_t mat;
  size_t i;
  size_t elem_size = pmath_packed_element_size(elem_type);
  const uint8_t *list_data;
  uint8_t       *mat_data;
  
  mat_sizes[0] = list_length;
  if(diag < 0)
    mat_sizes[0] += (size_t) - diag;
  else
    mat_sizes[0] += (size_t)diag;
  
  mat_sizes[1] = mat_sizes[0];
  
  mat = pmath_packed_array_new(
    PMATH_NULL,
    elem_type,
    2,
    mat_sizes,
    NULL,
    0);
  
  list_data = pmath_packed_array_read(list, NULL, 0);
  mat_data = pmath_packed_array_begin_write(&mat, NULL, 0);
  
  for(i = 0;i < list_length;++i) {
    size_t row, col;
    const uint8_t *list_item_data;
    uint8_t       *mat_item_data;
    
    if(diag > 0) {
      row = i;
      col = diag + i;
    }
    else {
      row = i - diag;
      col = i;
    }
    
    list_item_data = list_data + i * elem_size;
    mat_item_data = mat_data + (row * mat_sizes[0] + col) * elem_size;
    
    memcpy(mat_item_data, list_item_data, elem_size);
  }
  
  pmath_unref(list);
  return mat;
}


PMATH_PRIVATE pmath_t builtin_diagonalmatrix(pmath_expr_t expr) {
  /* DiagonalMatrix(list, diag)
     DiagonalMatrix(list)         = DiagonalMatrix(list, 0)
  
     diag is the diagonal above the leading diagonal.
   */
  
  intptr_t     diag;
  pmath_expr_t list;
  size_t len;
  
  len = pmath_expr_length(expr);
  if(len < 1 || len > 2) {
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST)) {
    pmath_unref(list);
    return expr;
  }
  
  if(len == 2) {
    pmath_t diag_obj = pmath_expr_get_item(expr, 2);
    
    if(!pmath_is_int32(diag_obj)) {
      pmath_message(PMATH_NULL, "intm", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
      pmath_unref(list);
      pmath_unref(diag_obj);
      return expr;
    }
    
    diag = PMATH_AS_INT32(diag_obj);
    pmath_unref(diag_obj);
  }
  else
    diag = 0;
    
  pmath_unref(expr);
  
  if(has_inexact_machine_number(list)) 
    list = _pmath_expr_pack_array(list, PMATH_PACKED_DOUBLE);
  else
    list = _pmath_expr_pack_array(list, PMATH_PACKED_INT32);
  
  if(pmath_is_packed_array(list) && pmath_packed_array_get_dimensions(list) == 1)
    return packed_diagonal_matrix(list, diag);
  
  return slow_diagonal_matrix(list, diag);
}
