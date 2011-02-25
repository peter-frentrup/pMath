#include <pmath-util/concurrency/threads.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
  #include <shellapi.h>
#else
  #include <dirent.h>
  #include <errno.h>
  #include <stdio.h>
  #include <string.h>
  #include <sys/stat.h>
#endif

#ifndef PMATH_OS_WIN32
static pmath_bool_t delete_file_or_dir(
  const char       *name,
  pmath_bool_t   recursive
){
  pmath_bool_t result = TRUE;
  
  if(pmath_aborting())
    return FALSE;
  
  if(recursive){
    DIR *dir = opendir(name);
    if(dir){
      size_t len = strlen(name);
      size_t new_size = len + 2;
      char *new_name = pmath_mem_alloc(new_size);
      
      if(new_name){
        struct dirent *entry;
        
        strcpy(new_name, name);
        
        new_name[len] = '/';
        new_name[++len] = '\0';
        
        while(0 != (entry = readdir(dir))){
          if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            size_t elen = strlen(entry->d_name);
            
            if(new_size < len + elen + 1){
              new_name = pmath_mem_realloc(new_name, len + elen + 1);
              new_size = len + elen + 1;
            }
            
            if(!new_name){
              result = FALSE;
              break;
            }
            
            strcpy(new_name + len, entry->d_name);
            
            result = delete_file_or_dir(new_name, TRUE);
            if(!result)
              break;
          }
        }
      }
      else
        result = FALSE;
      
      pmath_mem_free(new_name);
    }
  }
  
  if(result)
    return remove(name) == 0;
  return FALSE;
}
#endif

PMATH_PRIVATE pmath_t builtin_deletedirectory_and_deletefile(pmath_expr_t expr){
/* DeleteDirectory(name)
   DeleteFile(name)
   
   Options:
    DeleteContents
   
   Messages:
    General::fstr
    General::ioarg
    General::opttf
    
    DeleteDirectory::dirne
    DeleteDirectory::nodir
    
    DeleteFile::fdir
 */
  pmath_bool_t delete_contents = FALSE;
  pmath_t name;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(pmath_expr_length(expr) != 1){
    if(head == PMATH_SYMBOL_DELETEFILE
    || pmath_expr_length(expr) < 1){
      pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
      return expr;
    }
  }
  
  if(head == PMATH_SYMBOL_DELETEDIRECTORY){
    pmath_t options = pmath_options_extract(expr, 1);
    if(!options)
      return expr;
      
    name = pmath_option_value(NULL, PMATH_SYMBOL_DELETECONTENTS, options);
    pmath_unref(options);
    if(name == PMATH_SYMBOL_TRUE){
      delete_contents = TRUE;
    }
    else if(name != PMATH_SYMBOL_FALSE){
      pmath_message(NULL, "opttf", 2, 
        pmath_ref(PMATH_SYMBOL_DELETECONTENTS), name);
      return expr;
    }
    
    pmath_unref(name);
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name) || pmath_string_length(name) == 0){
    pmath_message(NULL, "fstr", 1, name);
    return expr;
  }
  
  #ifdef PMATH_OS_WIN32
  {
    static const uint16_t zerozero[2] = {0, 0};
    pmath_string_t abs_name = _pmath_canonical_file_name(pmath_ref(name));
    
    if(abs_name){
      abs_name = pmath_string_insert_ucs2(abs_name, INT_MAX, zerozero, 2);
    }
    
    if(abs_name){
      HANDLE h = CreateFileW(
        (const wchar_t*)pmath_string_buffer(abs_name),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
      
      if(h != INVALID_HANDLE_VALUE){
        BY_HANDLE_FILE_INFORMATION info;
        
        if(GetFileInformationByHandle(h, &info)
        && ((head == PMATH_SYMBOL_DELETEDIRECTORY 
          && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
         || (head == PMATH_SYMBOL_DELETEFILE
          && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
        ){
          DWORD err = 0;
          CloseHandle(h);
          
          if(delete_contents){
            SHFILEOPSTRUCTW op;
            memset(&op, 0, sizeof(op));
            op.wFunc  = FO_DELETE;
            op.pFrom  = (const wchar_t*)pmath_string_buffer(abs_name);
            op.pTo    = (const wchar_t*)zerozero;
            op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;
            
            err = (DWORD)SHFileOperationW(&op);
          }
          else if(head == PMATH_SYMBOL_DELETEFILE){
            if(!DeleteFileW((const wchar_t*)pmath_string_buffer(abs_name)))
              err = GetLastError();
          }
          else{
            if(!RemoveDirectoryW((const wchar_t*)pmath_string_buffer(abs_name)))
              err = GetLastError();
          }
          
          switch(err){
            case 0:
              pmath_unref(name);
              name = NULL;
              break;
            
            case ERROR_ACCESS_DENIED:
              pmath_message(NULL, "privv", 1, expr); 
              expr = NULL;
              pmath_unref(name);
              name = pmath_ref(PMATH_SYMBOL_FAILED);
            
            case ERROR_DIR_NOT_EMPTY:
              pmath_message(NULL, "dirne", 1, name); 
              name = NULL; 
              break;
              
            default:
              pmath_message(NULL, "ioarg", 1, expr); 
              expr = NULL;
              pmath_unref(name);
              name = pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        else{
          switch(GetLastError()){
            case ERROR_ACCESS_DENIED:
              pmath_message(NULL, "privv", 1, expr); 
              expr = NULL;
              break;
            
            default:
              if(head == PMATH_SYMBOL_DELETEFILE){
                pmath_message(NULL, "fdir", 1, name);
                name = NULL;
              }
              else{
                pmath_message(NULL, "nodir", 1, name);
                name = NULL;
              }
          }
          pmath_unref(name);
          name = pmath_ref(PMATH_SYMBOL_FAILED);
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
            if(head == PMATH_SYMBOL_DELETEFILE){
              pmath_message(NULL, "nffil", 1, expr);
              expr = NULL;
            }
            else{
              pmath_message(NULL, "nodir", 1, name);
              name = NULL;
            }
        }
        pmath_unref(name);
        name = pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
  
    pmath_unref(abs_name);
  }
  #else
  {
    char *str = pmath_string_to_native(name, NULL);
    
    if(str){
      errno = 0;
      if(delete_file_or_dir(str, delete_contents)){
        pmath_unref(name);
        name = NULL;
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
            pmath_message(NULL, "dirne", 1, name); 
            name = NULL; 
            break;
          
          case ENOENT: 
          case ENOTDIR: 
            if(head == PMATH_SYMBOL_DELETEDIRECTORY){
              pmath_message(NULL, "nodir", 1, name); 
              name = NULL; 
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
        
        pmath_unref(name);
        name = pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      pmath_mem_free(str);
    }
  }
  #endif
  
  pmath_unref(expr);
  return name;
}
