#include <pmath-util/concurrency/threads.h>
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
  #include <sys/types.h>
  #include <sys/stat.h>
#endif

#ifndef PMATH_OS_WIN32
static pmath_bool_t copy_file_or_dir(
  const char *src,
  const char *dst
){
  pmath_bool_t result = TRUE;
  char buf[256];
  size_t count;
  FILE *fsrc;
  FILE *fdst;
  DIR *dir;
  
  if(pmath_aborting())
    return FALSE;
  
  dir = opendir(src);
  if(dir){
    if(mkdir(dst, 0777) == 0 || errno == EEXIST){
      size_t src_len = strlen(src);
      size_t dst_len = strlen(dst);
      size_t new_src_size = src_len + 2;
      size_t new_dst_size = dst_len + 2;
      char *new_src = pmath_mem_alloc(new_src_size);
      char *new_dst = pmath_mem_alloc(new_dst_size);
      
      if(new_src && new_dst){
        struct dirent *entry;
        
        strcpy(new_src, src);
        strcpy(new_dst, dst);
        
        new_src[src_len] = '/';
        new_dst[dst_len] = '/';
        new_src[++src_len] = '\0';
        new_dst[++dst_len] = '\0';
        
        while(0 != (entry = readdir(dir))){
          if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            size_t elen = strlen(entry->d_name);
            
            if(new_src_size < src_len + elen + 2){
              new_src = pmath_mem_realloc(new_src, src_len + elen + 1);
              new_src_size = src_len + elen + 1;
            }
            
            if(new_dst_size < dst_len + elen + 2){
              new_dst = pmath_mem_realloc(new_dst, dst_len + elen + 1);
              new_dst_size = dst_len + elen + 1;
            }
            
            if(!new_src || !new_dst){
              result = FALSE;
              break;
            }
            
            strcpy(new_src + src_len, entry->d_name);
            strcpy(new_dst + dst_len, entry->d_name);
            
            result = copy_file_or_dir(new_src, new_dst);
            if(!result)
              break;
          }
        }
      }
      else
        result = FALSE;
      
      pmath_mem_free(new_src);
      pmath_mem_free(new_dst);
    }
    
    closedir(dir);
    return result;
  }
  
  fsrc = fopen(src, "rb");
  if(!fsrc)
    return FALSE;
  
  fdst = fopen(dst, "wb");
  if(!fdst){
    fclose(fsrc);
    return FALSE;
  }
  
  while(0 < (count = fread(buf, 1, sizeof(buf), fsrc))){
    if(fwrite(buf, 1, count, fdst) < count){
      result = FALSE;
      break;
    }
  }
  
  fclose(fdst);
  fclose(fsrc);
  return result;
}
#endif

PMATH_PRIVATE pmath_t builtin_copydirectory_and_copyfile(pmath_expr_t expr){
/* CopyDirectory(name1, name2)
   CopyFile(name1, name2)
   
   Messages:
    General::fstr
    General::ioarg
    
    CopyDirectory::filex
    CopyDirectory::nodir
    
    CopyFile::fdir
    CopyFile::filex
 */
  pmath_t name1, name2;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  name1 = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name1)
  || pmath_string_length(name1) == 0){
    pmath_message(PMATH_NULL, "fstr", 1, name1);
    return expr;
  }
  
  name2 = pmath_expr_get_item(expr, 2);
  if(!pmath_is_string(name2) || pmath_string_length(name2) == 0){
    pmath_message(PMATH_NULL, "fstr", 1, name2);
    pmath_unref(name1);
    return expr;
  }
  
  #ifdef PMATH_OS_WIN32
  {
    static const uint16_t zerozero[2] = {0, 0};
    pmath_string_t abs_name1 = _pmath_canonical_file_name(pmath_ref(name1));
    pmath_string_t abs_name2 = _pmath_canonical_file_name(pmath_ref(name2));
    
    if(!pmath_is_null(abs_name1) 
    && !pmath_is_null(abs_name2)){
      abs_name1 = pmath_string_insert_ucs2(abs_name1, INT_MAX, zerozero, 2);
      abs_name2 = pmath_string_insert_ucs2(abs_name2, INT_MAX, zerozero, 2);
    }
    
    if(!pmath_is_null(abs_name1) 
    && !pmath_is_null(abs_name2)){
      HANDLE h = CreateFileW(
        (const wchar_t*)pmath_string_buffer(abs_name1),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
      
      if(h != INVALID_HANDLE_VALUE){
        BY_HANDLE_FILE_INFORMATION info;
        
        if(GetFileInformationByHandle(h, &info)
        && ((pmath_same(head, PMATH_SYMBOL_COPYDIRECTORY)
          && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
         || (pmath_same(head, PMATH_SYMBOL_COPYFILE)
          && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
        ){
          HANDLE h2;
          CloseHandle(h);
          
          h2 = CreateFileW(
            (const wchar_t*)pmath_string_buffer(abs_name2),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);
          
          if(h2 != INVALID_HANDLE_VALUE){
            CloseHandle(h2);
            pmath_message(PMATH_NULL, "filex", 1, name2); 
            name2 = pmath_ref(PMATH_SYMBOL_FAILED);
          }
          else{
            SHFILEOPSTRUCTW op;
            
            memset(&op, 0, sizeof(op));
            op.wFunc  = FO_COPY;
            op.pFrom  = (const wchar_t*)pmath_string_buffer(abs_name1);
            op.pTo    = (const wchar_t*)pmath_string_buffer(abs_name2);
            op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;
            
            switch(SHFileOperationW(&op)){
              case 0:
                pmath_unref(name2);
                name2 = pmath_string_part(abs_name2, 0, pmath_string_length(abs_name2)-2);
                abs_name2 = PMATH_NULL;
                break;
              
              case ERROR_ACCESS_DENIED:
                pmath_message(PMATH_NULL, "privv", 1, expr); 
                expr = PMATH_NULL;
                pmath_unref(name2);
                name2 = pmath_ref(PMATH_SYMBOL_FAILED);
              
              default:
                pmath_message(PMATH_NULL, "ioarg", 1, expr); 
                expr = PMATH_NULL;
                pmath_unref(name2);
                name2 = pmath_ref(PMATH_SYMBOL_FAILED);
            }
          }
        }
        else{
          switch(GetLastError()){
            case ERROR_ACCESS_DENIED:
              pmath_message(PMATH_NULL, "privv", 1, expr); 
              expr = PMATH_NULL;
              break;
            
            default:
              if(pmath_same(head, PMATH_SYMBOL_COPYFILE)){
                pmath_message(PMATH_NULL, "fdir", 1, name1);
                name1 = PMATH_NULL;
              }
              else{
                pmath_message(PMATH_NULL, "nodir", 1, name1);
                name1 = PMATH_NULL;
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
            pmath_message(PMATH_NULL, "privv", 1, expr); 
            expr = PMATH_NULL;
            break;
          
          default:
            if(pmath_same(head, PMATH_SYMBOL_COPYFILE)){
              pmath_message(PMATH_NULL, "nffil", 1, expr);
              expr = PMATH_NULL;
            }
            else{
              pmath_message(PMATH_NULL, "nodir", 1, name1);
              name1 = PMATH_NULL;
            }
        }
        pmath_unref(name2);
        name2 = pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
  
    pmath_unref(abs_name1);
    pmath_unref(abs_name2);
  }
  #else
  {
    char   *str1 = pmath_string_to_native(name1, PMATH_NULL);
    char   *str2 = pmath_string_to_native(name2, PMATH_NULL);
    
    if(str1 && str2){
      struct stat buf;
      
      if(stat(str2, &buf) == 0){
        pmath_message(PMATH_NULL, "filex", 1, name2); 
        name2 = pmath_ref(PMATH_SYMBOL_FAILED);
      }
      else{
        if(stat(str1, &buf) == 0
        && ((pmath_same(head, PMATH_SYMBOL_COPYDIRECTORY)
          && S_ISDIR(buf.st_mode))
         || (pmath_same(head, PMATH_SYMBOL_COPYFILE)
          && !S_ISDIR(buf.st_mode)))
        ){
          errno = 0;
          if(copy_file_or_dir(str1, str2)){
            name2 = _pmath_canonical_file_name(name2);
          }
          else{
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
            pmath_unref(name2);
            name2 = pmath_ref(PMATH_SYMBOL_FAILED);
          }
        }
        else{
          switch(errno){
            case EACCES:
            case EPERM:
              pmath_message(PMATH_NULL, "privv", 1, expr);
              expr = PMATH_NULL;
              break;
            
            default:
              if(pmath_same(head, PMATH_SYMBOL_COPYDIRECTORY)){
                pmath_message(PMATH_NULL, "nodir", 1, name1); 
                name1 = PMATH_NULL; 
              }
              else{
                pmath_message(PMATH_NULL, "fdir", 1, name1); 
                name1 = PMATH_NULL; 
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
