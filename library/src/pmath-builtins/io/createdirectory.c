#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
#else
  #include <errno.h>
  #include <sys/stat.h>
#endif

PMATH_PRIVATE pmath_t builtin_createdirectory(pmath_expr_t expr){
/* CreateDirectory(name)
   
   Messages:
    General::fstr
    General::ioarg
 */
  pmath_t name;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name) || pmath_string_length(name) == 0){
    pmath_message(PMATH_NULL, "fstr", 1, name);
    return expr;
  }
  
  #ifdef PMATH_OS_WIN32
  {
    static const uint16_t zero = 0;
    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);
    
    if(!pmath_is_null(name)){
      if(!CreateDirectoryW((const wchar_t*)pmath_string_buffer(&name), NULL)){
        switch(GetLastError()){
          case ERROR_ALREADY_EXISTS:
            name = pmath_string_part(name, 0, pmath_string_length(name) - 1);
            name = _pmath_canonical_file_name(name);
            break;
            
          case ERROR_ACCESS_DENIED:
            pmath_message(PMATH_NULL, "privv", 1, expr); 
            expr = PMATH_NULL;
            pmath_unref(name);
            name = pmath_ref(PMATH_SYMBOL_FAILED);
            break;
          
          default:
            pmath_message(PMATH_NULL, "ioarg", 1, expr);
            expr = PMATH_NULL;
            pmath_unref(name);
            name = pmath_ref(PMATH_SYMBOL_FAILED);
        }
      }
      else{
        name = pmath_string_part(name, 0, pmath_string_length(name) - 1);
        name = _pmath_canonical_file_name(name);
      }
    }
  }
  #else
  {
    char *str = pmath_string_to_native(name, NULL);
    
    if(str){
      errno = 0;
      
      if(mkdir(str, 0777) != 0 && errno != EEXIST){
        switch(errno){
          case EACCES:
          case EPERM:
            pmath_message(PMATH_NULL, "privv", 1, expr); 
            expr = PMATH_NULL;
            break;
          
          default:
            pmath_message(PMATH_NULL, "ioarg", 1, expr);
            expr = PMATH_NULL;
        }
        
        pmath_unref(name);
        name = pmath_ref(PMATH_SYMBOL_FAILED);
      }
      else{
        name = _pmath_canonical_file_name(name);
      }
      
      pmath_mem_free(str);
    }
  }
  #endif
  
  pmath_unref(expr);
  return name;
}
