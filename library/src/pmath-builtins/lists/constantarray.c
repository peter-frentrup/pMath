#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


#define MAX_DIM 10

// frees c
static pmath_t const_list(pmath_t c, size_t length) {
  pmath_expr_t expr;
  struct _pmath_expr_t *list;
  size_t i;
  
  list = _pmath_expr_new_noinit(length);
  if(!list)
    return PMATH_NULL;
    
  if(pmath_is_pointer(c) && PMATH_AS_PTR(c)) {
    (void)pmath_atomic_fetch_add(&(PMATH_AS_PTR(c)->refcount), (intptr_t)length - 1);
  }
  
  list->items[0] = pmath_ref(PMATH_SYMBOL_LIST);
  for(i = length; i > 0; --i)
    list->items[i] = c;
    
  expr = PMATH_FROM_PTR(list);
  if(pmath_is_evaluated(c))
    _pmath_expr_update(expr);
  return expr;
}

// frees expr, c, n
static pmath_t slow_const_array(pmath_expr_t expr, pmath_t c, pmath_expr_t n) {
  size_t depth = pmath_expr_length(n);
  size_t i;
  
  for(i = depth; i > 0 && !pmath_aborting(); --i) {
    pmath_t ni = pmath_expr_get_item(n, i);
    
    if(pmath_is_int32(ni) && PMATH_AS_INT32(ni) >= 0) {
      c = const_list(c, (size_t)PMATH_AS_INT32(ni));
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

static pmath_bool_t read_sizes(pmath_expr_t expr, pmath_expr_t n, size_t *sizes) {
  size_t nlen = pmath_expr_length(n);
  size_t i;

  for(i = 0; i < nlen; ++i) {
    pmath_t ni = pmath_expr_get_item(n, i + 1);
    
    if(pmath_is_int32(ni) && PMATH_AS_INT32(ni) >= 0) {
      sizes[i] = (size_t)PMATH_AS_INT32(ni);
      continue;
    }
    
    pmath_unref(ni);
    
    pmath_message(
      PMATH_NULL, "ilsmn", 2,
      PMATH_FROM_INT32(2),
      pmath_ref(expr));
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_constantarray(pmath_expr_t expr) {
  /* ConstantArray(c, n)
     ConstantArray(c, {n1, n2, ...})
  
     messages:
       General::ilsmn
   */
  pmath_t c, n;
  pmath_bool_t ignore_array_elem;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  n = pmath_expr_get_item(expr, 2);
  if(!pmath_is_expr_of(n, PMATH_SYMBOL_LIST)) {
    n = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 1,
          n);
  }
  
  c = pmath_expr_get_item(expr, 1);
  
  ignore_array_elem = FALSE;
  if(!pmath_is_expr(c)) {
    ignore_array_elem = TRUE;
    c = _pmath_expr_pack_array(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_LIST), 1,
            c),
          PMATH_PACKED_INT32);
  }
  else
    c = _pmath_expr_pack_array(c, PMATH_PACKED_INT32);
  
  if(pmath_is_packed_array(c)) {
    size_t dimensions;
    size_t nlen = pmath_expr_length(n);
    size_t *sizes;
    size_t i;
    size_t total_size;
    pmath_packed_type_t elem_type = pmath_packed_array_get_element_type(c);
    pmath_packed_array_t result;
    void *data;
    
    dimensions = nlen;
    if(!ignore_array_elem)
      dimensions += pmath_packed_array_get_dimensions(c);
      
    sizes = pmath_mem_alloc(dimensions * sizeof(size_t));
    if(!sizes) {
      pmath_unref(c);
      pmath_unref(n);
      return expr;
    }
    
    if(!read_sizes(expr, n, sizes)) {
      pmath_unref(n);
      pmath_unref(c);
      pmath_mem_free(sizes);
      return expr;
    }
    
    if(dimensions > nlen) {
      const size_t *c_sizes = pmath_packed_array_get_sizes(c);
      
      memcpy(sizes + nlen, c_sizes, (dimensions - nlen) * sizeof(size_t));
    }
    
    // overflow is ok. It will be detected by pmath_packed_array_new().
    total_size = pmath_packed_element_size(elem_type);
    for(i = 0;i < dimensions;++i) {
      total_size*= sizes[i];
    }
    
    result = pmath_packed_array_new(
      pmath_blob_new(total_size, FALSE),
      elem_type,
      dimensions,
      sizes,
      NULL,
      0);
    
    if(pmath_is_null(result)) {
      pmath_unref(n);
      pmath_unref(c);
      pmath_mem_free(sizes);
      return expr;
    }
    
    data = pmath_packed_array_begin_write(&result, NULL, 0);
    if(data) {
      void *end = (int8_t*)data + total_size;
      
      while(data != end) {
        data = _pmath_packed_array_repack_to(c, data);
      }
    }
    
    pmath_unref(n);
    pmath_unref(c);
    pmath_unref(expr);
    pmath_mem_free(sizes);
    return result;
  }
  
  pmath_unref(c);
  c = pmath_expr_get_item(expr, 1);
  
  return slow_const_array(expr, c, n);
}
