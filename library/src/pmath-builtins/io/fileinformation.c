#include <pmath-core/numbers.h>

#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  include <Windows.h>
#else
#  include <sys/stat.h>
#endif


PMATH_PRIVATE pmath_t builtin_developer_fileinformation(pmath_expr_t expr) {
  /* Developer`FileInformation(filename)
   */
  pmath_string_t name, fullname;
  pmath_integer_t bytecount;
  pmath_t filetype;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)) {
    pmath_message(PMATH_NULL, "fstr", 1, name);
    return expr;
  }
  
  pmath_unref(expr);
  
  name = _pmath_canonical_file_name(name);
  
  // todo add FileDate
  
  fullname  = PMATH_UNDEFINED;
  bytecount = PMATH_UNDEFINED;
  filetype  = PMATH_UNDEFINED;
  
#ifdef PMATH_OS_WIN32
  {
    const uint16_t zero = 0;
    HANDLE h;

    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);
    
    h = CreateFileW(
          (const wchar_t*)pmath_string_buffer(&name),
          0,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          NULL,
          OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS, // needed to open handles to dicretories
          NULL);

    if(h != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION info;
      
      if(GetFileInformationByHandle(h, &info)) {
        DWORD size[2];

        name = pmath_string_part(name, 0, pmath_string_length(name) - 1);
        
        fullname = pmath_ref(name);
        
        size[0] = info.nFileSizeLow;
        size[1] = info.nFileSizeHigh;

        bytecount = pmath_integer_new_data(2, -1, sizeof(DWORD), 0, 0, size);
        
        // TODO: as FileType(), check for "\\servername\pipe\..."
        if(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          filetype = pmath_ref(PMATH_SYMBOL_DIRECTORY);
        else if(info.dwFileAttributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT))
          filetype = pmath_ref(PMATH_SYMBOL_SPECIAL);
        else
          filetype = pmath_ref(PMATH_SYMBOL_FILE);
      }

      CloseHandle(h);
    }
  }
#else
  {
    char *str = pmath_string_to_native(name, NULL);

    if(str) {
      struct stat buf;

      if(stat(str, &buf) == 0) {
        pmath_mem_free(str);
        
        fullname = pmath_ref(name);
        
        bytecount = pmath_integer_new_data(1, -1, sizeof(off_t), 0, 0, &buf.st_size);
        
        if(S_ISDIR(buf.st_mode))
          filetype = pmath_ref(PMATH_SYMBOL_DIRECTORY);
        else if(S_ISREG(buf.st_mode))
          filetype = pmath_ref(PMATH_SYMBOL_FILE);
        else
          filetype = pmath_ref(PMATH_SYMBOL_SPECIAL);
      }

      pmath_mem_free(str);
    }
  }
#endif
  
  pmath_unref(name);
  
  pmath_gather_begin(PMATH_NULL);
  
  if(!pmath_same(fullname, PMATH_UNDEFINED)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_FILE), 
        fullname),
      PMATH_NULL);
  }
  
  if(!pmath_same(filetype, PMATH_UNDEFINED)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_FILETYPE), 
        filetype),
      PMATH_NULL);
  }
  
  if(!pmath_same(bytecount, PMATH_UNDEFINED)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_BYTECOUNT), 
        bytecount),
      PMATH_NULL);
  }
  
  return pmath_gather_end();
}

