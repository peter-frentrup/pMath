#include <pmath-core/strings-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


static pmath_t concat_expressions(pmath_expr_t expr){
  size_t i, len, length, resi;
  pmath_expr_t result;
  
  len = pmath_expr_length(expr);
  length = 0;
  
  { // check arguments (all are expressions with same head) ...
    pmath_t fst, fst_head;
    
    fst = pmath_expr_get_item(expr, 1);
    if(pmath_is_string(fst)){
      pmath_unref(fst);
      pmath_message(PMATH_NULL, "strexp", 0);
      return expr;
    }
    
    if(!pmath_is_expr(fst)){
      pmath_message(PMATH_NULL, "atom", 1, fst);
      return expr;
    }
    
    fst_head = pmath_expr_get_item(fst, 0);
    length = pmath_expr_length(fst);
    for(i = 2;i <= len;++i){
      pmath_t obj, obj_head;
      
      obj = pmath_expr_get_item(expr, i);
      if(pmath_is_string(obj)){
        pmath_unref(fst);
        pmath_unref(fst_head);
        pmath_unref(obj);
        pmath_message(PMATH_NULL, "strexpr", 0);
        return expr;
      }
      
      if(!pmath_is_expr(obj)){
        pmath_unref(fst);
        pmath_unref(fst_head);
        pmath_message(PMATH_NULL, "atom", 1, obj);
        return expr;
      }

      obj_head = pmath_expr_get_item(obj, 0);
      if(!pmath_equals(fst_head, obj_head)){
        pmath_message(PMATH_NULL, "heads", 4, 
          fst_head, 
          obj_head,
          pmath_integer_new_uiptr(1),
          pmath_integer_new_uiptr(i));
        pmath_unref(fst);
        pmath_unref(obj);
        return expr;
      }
      
      length+= pmath_expr_length(obj);
      pmath_unref(obj);
      pmath_unref(obj_head);
    }
    
    pmath_unref(fst);
    pmath_unref(fst_head);
  }

  result = pmath_expr_get_item(expr, 1);
  resi = pmath_expr_length(result);
  result = pmath_expr_resize(result, length);
  for(i = 2;i <= len;++i){
    pmath_expr_t arg = pmath_expr_get_item(expr, i);
    
    size_t arglen = pmath_expr_length(arg);
    size_t j;
    for(j = 1;j <= arglen;++j){
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

static pmath_t concat_strings(pmath_expr_t expr){
  size_t i, len, length;
  struct _pmath_string_t *result;
  uint16_t *str;
  
  len = pmath_expr_length(expr);
  length = 0;
  
  { // check arguments (all are strings) ...
    pmath_t fst = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr(fst)){
      pmath_unref(fst);
      pmath_message(PMATH_NULL, "strexp", 0);
      return expr;
    }
    
    if(!pmath_is_string(fst)){
      pmath_message(PMATH_NULL, "atom", 1, fst);
      return expr;
    }
    
    length = pmath_string_length(fst);
    
    for(i = 2;i <= len;++i){
      size_t objlen;
      pmath_t obj = pmath_expr_get_item(expr, i);
      
      if(pmath_is_expr(obj)){
        pmath_unref(fst);
        pmath_unref(obj);
        pmath_message(PMATH_NULL, "strexp", 0);
        return expr;
      }
      
      if(!pmath_is_string(obj)){
        pmath_unref(fst);
        pmath_message(PMATH_NULL, "atom", 1, obj);
        return expr;
      }

      objlen = pmath_string_length(obj);
      if(length + objlen < length){ // overflow
        pmath_abort_please();
        pmath_unref(fst);
        pmath_unref(obj);
        return expr;
      }
      
      length+= objlen;
      pmath_unref(obj);
    }
    
    pmath_unref(fst);
  }

  result = _pmath_new_string_buffer(length);
  if(!result)
    return expr;

  str = AFTER_STRING(result);
  for(i = 1;i <= len;++i){
    pmath_string_t stri = pmath_expr_get_item(expr, i);
    int stri_len = pmath_string_length(stri);
    
    memcpy(str, pmath_string_buffer(stri), stri_len * sizeof(uint16_t));
    str+= stri_len;
    pmath_unref(stri);
  }

  pmath_unref(expr);
  return _pmath_from_buffer(result);
}

PMATH_PRIVATE pmath_t builtin_join(pmath_expr_t expr){
/* Join(list1, list2, ...)
 */
  size_t len = pmath_expr_length(expr);
  
  if(len > 0){
    pmath_t fst = pmath_expr_get_item(expr, 1);
    if(pmath_is_expr(fst)){
      pmath_unref(fst);
      return concat_expressions(expr);
    }
    
    if(pmath_is_string(fst)){
      pmath_unref(fst);
      return concat_strings(expr);
    }

    pmath_message(PMATH_NULL, "atom", 1, fst);
    return expr;
  }
  else if(len == 1){
    pmath_t result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return result;
  }
  else{
    pmath_message_argxxx(len, 1, SIZE_MAX);
    return expr;
  }
}
