#include <pmath-core/numbers.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <limits.h>


#ifdef PMATH_OS_WIN32
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
#endif

PMATH_PRIVATE pmath_t builtin_environment(pmath_expr_t expr){
  pmath_t name;
  
  if(pmath_expr_length(expr) > 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  if(pmath_expr_length(expr) == 0){
    #ifdef PMATH_OS_WIN32
    {
      WCHAR *env = GetEnvironmentStringsW();
      
      pmath_unref(expr);
      pmath_gather_begin(PMATH_NULL);
      {
        const WCHAR *start = env;
        while(*start){
          const WCHAR *end = start;
          while(*end && *end != '=')
            ++end;
          
          if(*end == '='){
            pmath_string_t value = pmath_string_insert_ucs2(
              PMATH_NULL, 0, 
              (const uint16_t*)(end + 1), 
              -1);
              
            name = pmath_string_insert_ucs2(
              PMATH_NULL, 0, 
              (const uint16_t*)start, 
              ((size_t)end - (size_t)start)/sizeof(uint16_t));
            
            pmath_emit(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_RULE), 2,
                name,
                value),
              PMATH_NULL);
          }
          
          start = end + 1;
        }
      }
      expr = pmath_gather_end();
      
      FreeEnvironmentStringsW(env);
      
      return expr;
    }
    #else
    {
      char **env = environ;
      
      pmath_unref(expr);
      pmath_gather_begin(PMATH_NULL);
      for(env = environ;*env;++env){
        const char *start = *env;
        const char *end = start;
        
        while(*end && *end != '=')
          ++end;
        
        if(end){
          pmath_string_t value = pmath_string_from_native(end + 1, -1);
          name = pmath_string_from_native(
            start, (int)((size_t)end - (size_t)start));
          
          pmath_emit(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_RULE), 2,
              name,
              value),
            PMATH_NULL);
        }
      }
      return pmath_gather_end();
    }
    #endif
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)){
    pmath_unref(name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  pmath_unref(expr);
  
  #ifdef PMATH_OS_WIN32
  {
    name = pmath_string_insert_latin1(name, INT_MAX, "", 1);
    if(!pmath_is_null(name)){
      int len = (int)GetEnvironmentVariableW(pmath_string_buffer(name), NULL, 0);
      if(len > 0){
        struct _pmath_string_t *result = _pmath_new_string_buffer(len);
        
        if(result){
          GetEnvironmentVariableW(pmath_string_buffer(name), AFTER_STRING(result), len);
          pmath_unref(name);
          result->length = len-1;
          return PMATH_FROM_PTR(result);
        }
      }
    }
    
    pmath_unref(name);
  }
  #else
  {
    char *n = pmath_string_to_native(name, NULL);
    pmath_unref(name);
    
    if(n){
      const char *value = getenv(n);
      pmath_mem_free(n);
      
      if(value)
        return pmath_string_from_native(value, -1);
    }
  }
  #endif
  
  return PMATH_C_STRING("");
}

PMATH_PRIVATE pmath_t builtin_assign_environment(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_same(tag, PMATH_UNDEFINED)
  || !pmath_is_expr_of_len(lhs, PMATH_SYMBOL_ENVIRONMENT, 1)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  tag = pmath_expr_get_item(lhs, 1);
  if(!pmath_is_string(tag)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  pmath_unref(lhs);
  
  if(pmath_same(rhs, PMATH_UNDEFINED)){
    #ifdef PMATH_OS_WIN32
    {
      // zero-teminate:
      tag = pmath_string_insert_latin1(tag, INT_MAX, "", 1);
      
      if(!pmath_is_null(tag)){
        SetEnvironmentVariableW((const WCHAR*)pmath_string_buffer(tag), NULL);
      }
    }
    #else
    {
      char *tagstr = pmath_string_to_native(tag, NULL);
      
      if(tagstr)
        unsetenv(tagstr);
      
      pmath_mem_free(tagstr);
    }
    #endif
  }
  else if(pmath_is_string(rhs)){
    #ifdef PMATH_OS_WIN32
    {
      // zero-teminate:
      tag = pmath_string_insert_latin1(tag, INT_MAX, "", 1);
      rhs = pmath_string_insert_latin1(rhs, INT_MAX, "", 1);
      
      if(!pmath_is_null(tag) && !pmath_is_null(rhs)){
        SetEnvironmentVariableW(
          (const WCHAR*)pmath_string_buffer(tag), 
          (const WCHAR*)pmath_string_buffer(rhs));
      }
    }
    #else
    {
      char *tagstr = pmath_string_to_native(tag, NULL);
      char *rhsstr = pmath_string_to_native(tag, NULL);
      
      if(tagstr && rhsstr)
        setenv(tagstr, rhsstr, 1);
      
      pmath_mem_free(tagstr);
      pmath_mem_free(rhsstr);
    }
    #endif
  }
  else{
    pmath_unref(tag);
    pmath_unref(rhs);
    return expr;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  return rhs;
}
