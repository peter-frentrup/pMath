#include <pmath-core/numbers.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


#ifdef PMATH_OS_WIN32
  #define PATH_SEP  '\\'
  #define IS_PATH_SEP(c)  ((c) == '\\' || (c) == '/')
#else
  #define PATH_SEP  '/'
  #define IS_PATH_SEP(c)  ((c) == '/')
#endif

PMATH_PRIVATE pmath_t builtin_tofilename(pmath_expr_t expr){
  struct _pmath_string_t *result;
  pmath_t dir, name, sub;
  uint16_t *buf;
  const uint16_t *subbuf;
  size_t exprlen = pmath_expr_length(expr);
  int len, sublen;
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  dir  = pmath_expr_get_item(expr, 1);
  
  len = 0;
  if(pmath_is_string(dir)){
    if(pmath_string_length(dir) > 0){
      sublen = pmath_string_length(dir);
      subbuf = pmath_string_buffer(&dir);
      
      len = sublen;
      if(!IS_PATH_SEP(subbuf[sublen - 1]))
        ++len;
    }
  }
  else if(pmath_is_expr_of(dir, PMATH_SYMBOL_LIST)){
    size_t i;
    for(i = pmath_expr_length(dir);i > 0;--i){
      sub = pmath_expr_get_item(dir, i);
      
      if(!pmath_is_string(sub)){
        pmath_unref(dir);
        pmath_unref(sub);
        pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
        return expr;
      }
      
      sublen = pmath_string_length(sub);
      subbuf = pmath_string_buffer(&sub);
      if(sublen > 0){
        len+= sublen;
        if(IS_PATH_SEP(subbuf[0]) && i > 1)
          --len;
        if(!IS_PATH_SEP(subbuf[sublen - 1]))
          ++len;
      }
      
      pmath_unref(sub);
    }
  }
  else{
    pmath_unref(dir);
    pmath_message(PMATH_NULL, "strse", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  if(exprlen == 2){
    name = pmath_expr_get_item(expr, 2);
    if(!pmath_is_string(name)){
      pmath_unref(dir);
      pmath_unref(name);
      pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
      return expr;
    }
  }
  else
    name = PMATH_C_STRING("");
  pmath_unref(expr);
  
  sublen = pmath_string_length(name);
  subbuf = pmath_string_buffer(&name);
  len+= sublen;
  if(sublen > 0 && IS_PATH_SEP(subbuf[0]))
    --len;
  
  result = _pmath_new_string_buffer(len);
  if(!result){
    pmath_unref(dir);
    pmath_unref(name);
    return PMATH_NULL;
  }
  
  buf = AFTER_STRING(result);
  if(pmath_is_string(dir)){
    sublen = pmath_string_length(dir);
    subbuf = pmath_string_buffer(&dir);
    
    if(sublen > 0){
      memcpy(buf, subbuf, sizeof(uint16_t) * sublen);
      buf+= sublen;
      if(IS_PATH_SEP(subbuf[sublen - 1]))
        --buf;
      
      *buf++ = PATH_SEP;
    }
  }
  else{
    size_t i;
    for(i = 1;i <= pmath_expr_length(dir);++i){
      sub = pmath_expr_get_item(dir, i);
      
      sublen = pmath_string_length(sub);
      subbuf = pmath_string_buffer(&sub);
      if(sublen > 0){
        if(IS_PATH_SEP(subbuf[0]) && i > 1){
          ++subbuf;
          --sublen;
        }
        
        memcpy(buf, subbuf, sizeof(uint16_t) * sublen);
        buf+= sublen;
        if(IS_PATH_SEP(subbuf[sublen - 1]))
          --buf;
      
        *buf++ = PATH_SEP;
      }
        
      pmath_unref(sub);
    }
  }
  
  pmath_unref(dir);
  sublen = pmath_string_length(name);
  subbuf = pmath_string_buffer(&name);
  if(sublen > 0 && IS_PATH_SEP(subbuf[0])){
    ++subbuf;
    --sublen;
  }
  memcpy(buf, subbuf, sizeof(uint16_t) * sublen);
  pmath_unref(name);
  return _pmath_from_buffer(result);
}
