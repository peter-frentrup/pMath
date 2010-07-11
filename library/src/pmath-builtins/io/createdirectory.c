#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
#else
  #include <errno.h>
  #include <sys/stat.h>
#endif

#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>
#include <pmath-builtins/lists-private.h>

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
  if(!pmath_instance_of(name, PMATH_TYPE_STRING)
  || pmath_string_length(name) == 0){
    pmath_message(NULL, "fstr", 1, name);
    return expr;
  }
  
  #ifdef PMATH_OS_WIN32
  {
    static const uint16_t zero = 0;
    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);
    
    if(name){
      if(!CreateDirectoryW((const wchar_t*)pmath_string_buffer(name), NULL)){
        switch(GetLastError()){
          case ERROR_ALREADY_EXISTS:
            name = pmath_string_part(name, 0, pmath_string_length(name) - 1);
            name = _pmath_canonical_file_name(name);
            break;
            
          case ERROR_ACCESS_DENIED:
            pmath_message(NULL, "privv", 1, expr); 
            expr = NULL;
            pmath_unref(name);
            name = pmath_ref(PMATH_SYMBOL_FAILED);
            break;
          
          default:
            pmath_message(NULL, "ioarg", 1, expr);
            expr = NULL;
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
            pmath_message(NULL, "privv", 1, expr); 
            expr = NULL;
            break;
          
          default:
            pmath_message(NULL, "ioarg", 1, expr);
            expr = NULL;
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
