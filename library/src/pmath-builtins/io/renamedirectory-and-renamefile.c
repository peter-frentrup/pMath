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
  #include <stdio.h>
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

PMATH_PRIVATE pmath_t builtin_renamedirectory_and_renamefile(pmath_expr_t expr){
/* RenameDirectory(name1, name2)
   RenameFiley(name1, name2)
   
   Messages:
    General::fstr
    General::ioarg
    
    RenameDirectory::dirne
    RenameDirectory::filex
    RenameDirectory::nodir
    
    RenameFile::fdir
    RenameFile::filex
 */
  pmath_t name1, name2;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  name1 = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(name1, PMATH_TYPE_STRING)
  || pmath_string_length(name1) == 0){
    pmath_message(NULL, "fstr", 1, name1);
    return expr;
  }
  
  name2 = pmath_expr_get_item(expr, 2);
  if(!pmath_instance_of(name2, PMATH_TYPE_STRING)
  || pmath_string_length(name2) == 0){
    pmath_message(NULL, "fstr", 1, name2);
    pmath_unref(name1);
    return expr;
  }
  
  #ifdef PMATH_OS_WIN32
  {
    const uint16_t zero = 0;
    
    name1 = pmath_string_insert_ucs2(name1, INT_MAX, &zero, 1);
    name2 = pmath_string_insert_ucs2(name2, INT_MAX, &zero, 1);
    
    if(name1 && name2){
      HANDLE h = CreateFileW(
        (const wchar_t*)pmath_string_buffer(name1),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
      
      if(h != INVALID_HANDLE_VALUE){
        BY_HANDLE_FILE_INFORMATION info;
        
        if(GetFileInformationByHandle(h, &info)
        && ((head == PMATH_SYMBOL_RENAMEDIRECTORY 
          && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
         || (head == PMATH_SYMBOL_RENAMEFILE
          && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
        ){
          CloseHandle(h);
          
          if(MoveFileExW(
              (const wchar_t*)pmath_string_buffer(name1),
              (const wchar_t*)pmath_string_buffer(name2),
              MOVEFILE_COPY_ALLOWED)
          ){
            name2 = pmath_string_part(name2, 0, pmath_string_length(name2) - 1);
            name2 = _pmath_canonical_file_name(name2);
          }
          else{
            switch(GetLastError()){
              case ERROR_ACCESS_DENIED:
                pmath_message(NULL, "privv", 1, expr); 
                expr = NULL;
                break;
                
              case ERROR_FILE_EXISTS:
              case ERROR_ALREADY_EXISTS:
                pmath_message(NULL, "filex", 1, 
                  pmath_string_part(name2, 0, pmath_string_length(name2) - 1)); 
                name2 = NULL; 
                break;
              
              case ERROR_DIR_NOT_EMPTY:
                pmath_message(NULL, "dirne", 1, 
                  pmath_string_part(name1, 0, pmath_string_length(name1) - 1)); 
                name1 = NULL; 
                break;
              
              case ERROR_FILE_NOT_FOUND: 
              case ERROR_PATH_NOT_FOUND: 
                if(head == PMATH_SYMBOL_RENAMEDIRECTORY){
                  pmath_message(NULL, "nodir", 1,
                    pmath_string_part(name1, 0, pmath_string_length(name1) - 1)); 
                  name1 = NULL;
                }
                else{
                  pmath_message(NULL, "nffil", 1, expr);
                  expr = NULL;
                }
                break;
              
              default:
                pmath_message(NULL, "ioarg", 1, expr); 
                expr = NULL;
                break;
            }
            
            pmath_unref(name2);
            name2 = pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        else{
          switch(GetLastError()){
            case ERROR_ACCESS_DENIED:
              pmath_message(NULL, "privv", 1, expr); 
              expr = NULL;
              break;
            
            default:
              if(head == PMATH_SYMBOL_RENAMEFILE){
                pmath_message(NULL, "fdir", 1, 
                  pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                name1 = NULL;
              }
              else{
                pmath_message(NULL, "nodir", 1, 
                  pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                name1 = NULL;
              }
          }
          pmath_unref(name2);
          name2 = pmath_ref(PMATH_SYMBOL_FAILED);
          CloseHandle(h);
        }
      }
      else{
        switch(GetLastError()){
          case ERROR_ACCESS_DENIED:
            pmath_message(NULL, "privv", 1, expr); 
            expr = NULL;
            break;
          
          default:
            if(head == PMATH_SYMBOL_RENAMEFILE){
              pmath_message(NULL, "nffil", 1, expr);
              expr = NULL;
            }
            else{
              pmath_message(NULL, "nodir", 1, 
                pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
              name1 = NULL;
            }
        }
        pmath_unref(name2);
        name2 = pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
  }
  #else
  {
    char   *str1 = pmath_string_to_native(name1, NULL);
    char   *str2 = pmath_string_to_native(name2, NULL);
    
    if(str1 && str2){
      struct stat buf;
      
      if(stat(str2, &buf) == 0){
        pmath_message(NULL, "filex", 1, name2); 
        name2 = pmath_ref(PMATH_SYMBOL_FAILED);
      }
      else{
        errno = 0;
        if(stat(str1, &buf) == 0
        && ((head == PMATH_SYMBOL_RENAMEDIRECTORY 
          && S_ISDIR(buf.st_mode))
         || (head == PMATH_SYMBOL_RENAMEFILE 
          && !S_ISDIR(buf.st_mode)))
        ){
          errno = 0;
          if(rename(str1, str2) == 0){
            name2 = _pmath_canonical_file_name(name2);
          }
          else{
            switch(errno){
              case EACCES:
              case EPERM:
                pmath_message(NULL, "privv", 1, expr);
                expr = NULL;
                break;
                
              case ENOTEMPTY:
              case EEXIST:
                pmath_message(NULL, "dirne", 1, name2); 
                name2 = NULL; 
                break;
              
              case ENOENT:
                pmath_message(NULL, "nodir", 1, name1); 
                name1 = NULL; 
                break;
                
              case EISDIR:
              case ENOTDIR:
                pmath_message(NULL, "nodir", 1, name2); 
                name2 = NULL; 
                break;
              
              default:
                pmath_message(NULL, "ioarg", 1, expr);
                expr = NULL;
                break;
            }
            
            pmath_unref(name2);
            name2 = pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        else{
          switch(errno){
            case EACCES:
            case EPERM:
              pmath_message(NULL, "privv", 1, expr);
              expr = NULL;
              break;
            
            default:
              if(head == PMATH_SYMBOL_RENAMEDIRECTORY){
                pmath_message(NULL, "nodir", 1, name1); 
                name1 = NULL; 
              }
              else{
                pmath_message(NULL, "fdir", 1, name1); 
                name1 = NULL; 
              }
          }
          pmath_unref(name2);
          name2 = pmath_ref(PMATH_SYMBOL_FAILED);
        }
      }
    }
    
    pmath_mem_free(str1);
    pmath_mem_free(str2);
  }
  #endif
  
  pmath_unref(expr);
  pmath_unref(name1);
  return name2;
}
