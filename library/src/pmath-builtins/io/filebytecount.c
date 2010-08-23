#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>
#include <pmath-util/memory.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
#else
  #include <sys/stat.h>
#endif

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_filebytecount(pmath_expr_t expr){
/* FileByteCount(file)
 */
  pmath_t file;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(file, PMATH_TYPE_STRING)
  || pmath_string_length(file) == 0){
    pmath_message(NULL, "fstr", 1, file);
    return expr;
  }
  pmath_unref(expr);
  
  #ifdef PMATH_OS_WIN32
  {
    const uint16_t zero = 0;
    HANDLE h;
    
    file = pmath_string_insert_ucs2(file, INT_MAX, &zero, 1);
    
    // use CreateFile() instead of GetFileAttributes() to follow symbolic links.
    h = CreateFileW(
      (const wchar_t*)pmath_string_buffer(file),
      0,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      0,
      NULL);
    
    if(h != INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION info;
      
      if(GetFileInformationByHandle(h, &info)){
        DWORD size[2];
        pmath_unref(file);
        CloseHandle(h);
        
        size[0] = info.nFileSizeLow;
        size[1] = info.nFileSizeHigh;
        
        return pmath_integer_new_data(2, -1, sizeof(DWORD), 0, 0, size);
      }
      
      CloseHandle(h);
    }
  }
  #else
  {
    char *str = pmath_string_to_native(file, NULL);
    
    if(str){
      struct stat buf;
      
      if(stat(str, &buf) == 0){
        pmath_mem_free(str);
        pmath_unref(file);
        
        return pmath_integer_new_data(1, -1, sizeof(off_t), 0, 0, &buf.st_size);
      }
      
      pmath_mem_free(str);
    }
  }
  #endif
  
  pmath_unref(file);
  return pmath_integer_new_si(0);
}
