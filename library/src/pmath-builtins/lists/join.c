#include <pmath-core/strings-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


static pmath_bool_t chech_concat_expr_arguments(pmath_expr_t expr, size_t *length) {
  pmath_t fst;
  pmath_t fst_head;
  size_t i;
  size_t len;
  
  *length = 0;
  
  fst = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(fst)) {
    pmath_unref(fst);
    pmath_message(PMATH_NULL, "strexp", 0);
    return FALSE;
  }
  
  if(!pmath_is_expr(fst)) {
    pmath_message(PMATH_NULL, "atom", 1, fst);
    return FALSE;
  }
  
  fst_head = pmath_expr_get_item(fst, 0);
  *length = pmath_expr_length(fst);
  len = pmath_expr_length(expr);
  for(i = 2; i <= len; ++i) {
    pmath_t obj;
    pmath_t obj_head;
    
    obj = pmath_expr_get_item(expr, i);
    if(pmath_is_string(obj)) {
      pmath_unref(fst);
      pmath_unref(fst_head);
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "strexpr", 0);
      return FALSE;
    }
    
    if(!pmath_is_expr(obj)) {
      pmath_unref(fst);
      pmath_unref(fst_head);
      pmath_message(PMATH_NULL, "atom", 1, obj);
      return FALSE;
    }
    
    obj_head = pmath_expr_get_item(obj, 0);
    if(!pmath_equals(fst_head, obj_head)) {
      pmath_message(PMATH_NULL, "heads", 4,
          fst_head,
          obj_head,
          pmath_integer_new_uiptr(1),
          pmath_integer_new_uiptr(i));
      pmath_unref(fst);
      pmath_unref(obj);
      return FALSE;
    }
    
    *length += pmath_expr_length(obj);
    pmath_unref(obj);
    pmath_unref(obj_head);
  }
  
  pmath_unref(fst);
  pmath_unref(fst_head);
  return TRUE;
}

static pmath_t concat_expressions(pmath_expr_t expr) {
  size_t i, len, length, resi;
  pmath_expr_t result;
  
  if(!chech_concat_expr_arguments(expr, &length))
    return expr;
    
    
  len = pmath_expr_length(expr);
  result = pmath_expr_get_item(expr, 1);
  resi = pmath_expr_length(result);
  result = pmath_expr_resize(result, length);
  for(i = 2; i <= len; ++i) {
    pmath_expr_t arg = pmath_expr_get_item(expr, i);
    
    size_t arglen = pmath_expr_length(arg);
    size_t j;
    for(j = 1; j <= arglen; ++j) {
      result = pmath_expr_set_item(
          result,
          ++resi,
          pmath_expr_get_item(arg, j));
    }
    pmath_unref(arg);
  }
  
  pmath_unref(expr);
  return result;
}

static pmath_bool_t check_concat_string_arguments(pmath_expr_t expr, int *length) {
  pmath_t str = pmath_expr_get_item(expr, 1);
  size_t i;
  size_t len;
  unsigned result_length;
  
  *length = 0;
  
  if(pmath_is_expr(str)) {
    pmath_unref(str);
    pmath_message(PMATH_NULL, "strexp", 0);
    return FALSE;
  }
  
  if(!pmath_is_string(str)) {
    pmath_message(PMATH_NULL, "atom", 1, str);
    return FALSE;
  }
  
  result_length = (unsigned)pmath_string_length(str);
  pmath_unref(str);
  
  len = pmath_expr_length(expr);
  for(i = 2; i <= len; ++i) {
    unsigned strlen;
    str = pmath_expr_get_item(expr, i);
    
    if(pmath_is_expr(str)) {
      pmath_unref(str);
      pmath_message(PMATH_NULL, "strexp", 0);
      return FALSE;
    }
    
    if(!pmath_is_string(str)) {
      pmath_message(PMATH_NULL, "atom", 1, str);
      return FALSE;
    }
    
    strlen = (unsigned)pmath_string_length(str);
    if(result_length + strlen < result_length) { // overflow
      pmath_abort_please();
      pmath_unref(str);
      return FALSE;
    }
    
    result_length += strlen;
    pmath_unref(str);
  }
  
  if(result_length > INT_MAX) { // overflow
    pmath_abort_please();
    return FALSE;
  }
  
  *length = (int)result_length;
  return TRUE;
}

static pmath_t concat_strings(pmath_expr_t expr) {
  size_t i, len;
  int length;
  struct _pmath_string_t *result;
  uint16_t *str;
  
  if(!check_concat_string_arguments(expr, &length))
    return expr;
  
  result = _pmath_new_string_buffer((int)length);
  if(!result)
    return expr;
    
  str = AFTER_STRING(result);
  len = pmath_expr_length(expr);
  for(i = 1; i <= len; ++i) {
    pmath_string_t stri = pmath_expr_get_item(expr, i);
    int stri_len = pmath_string_length(stri);
    
    memcpy(str, pmath_string_buffer(&stri), stri_len * sizeof(uint16_t));
    str += stri_len;
    pmath_unref(stri);
  }
  
  pmath_unref(expr);
  return _pmath_from_buffer(result);
}

PMATH_PRIVATE pmath_t builtin_join(pmath_expr_t expr) {
  /* Join(list1, list2, ...)
   */
  size_t len = pmath_expr_length(expr);
  
  if(len > 0) {
    pmath_t fst = pmath_expr_get_item(expr, 1);
    if(pmath_is_expr(fst)) {
      pmath_unref(fst);
      return concat_expressions(expr);
    }
    
    if(pmath_is_string(fst)) {
      pmath_unref(fst);
      return concat_strings(expr);
    }
    
    pmath_message(PMATH_NULL, "atom", 1, fst);
    return expr;
  }
  else if(len == 1) {
    pmath_t result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return result;
  }
  else {
    pmath_message_argxxx(len, 1, SIZE_MAX);
    return expr;
  }
}
