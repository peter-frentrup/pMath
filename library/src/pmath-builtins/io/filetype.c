#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
#else
  #include <sys/stat.h>
#endif


PMATH_PRIVATE pmath_t builtin_filetype(pmath_expr_t expr){
/* FileType(file)
 */
  pmath_t file;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(file) || pmath_string_length(file) == 0){
    pmath_message(PMATH_NULL, "fstr", 1, file);
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
      FILE_FLAG_BACKUP_SEMANTICS,
      NULL);
    
    if(h != INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION info;
      
      if(GetFileInformationByHandle(h, &info)){
        pmath_unref(file);
        CloseHandle(h);
        
        if(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          return pmath_ref(PMATH_SYMBOL_DIRECTORY);
      
        if(info.dwFileAttributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT))
          return pmath_ref(PMATH_SYMBOL_SPECIAL);
        
        return pmath_ref(PMATH_SYMBOL_FILE);
      }
      
      CloseHandle(h);
    }
  }
  #else
  {
    char *str = pmath_string_to_native(file, PMATH_NULL);
    
    if(str){
      struct stat buf;
      
      if(stat(str, &buf) == 0){
        pmath_mem_free(str);
        pmath_unref(file);
        
        if(S_ISDIR(buf.st_mode))
          return pmath_ref(PMATH_SYMBOL_DIRECTORY);
        
        if(S_ISREG(buf.st_mode))
          return pmath_ref(PMATH_SYMBOL_FILE);
        
        return pmath_ref(PMATH_SYMBOL_SPECIAL);
      }
      
      pmath_mem_free(str);
    }
  }
  #endif
  
  pmath_unref(file);
  return pmath_ref(PMATH_SYMBOL_NONE);
}
