#include "stdafx.h"

extern pmath_symbol_t pmath_System_ByteCount;
extern pmath_symbol_t pmath_System_Directory;
extern pmath_symbol_t pmath_System_File;
extern pmath_symbol_t pmath_System_FileType;
extern pmath_symbol_t pmath_System_None;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_Special;

extern pmath_symbol_t pmath_System_FileInformation;

#ifdef PMATH_OS_WIN32
static pmath_string_t get_final_path_name(HANDLE h) {
  DWORD size;
  DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
  pmath_string_t result;
  uint16_t *buf;
  
  size = GetFinalPathNameByHandleW(h, NULL, 0, flags);
  if(size == 0 || (int)size <= 0) {
    return PMATH_UNDEFINED;
  }
  
  result = pmath_string_new_raw(size);
  if(pmath_string_begin_write(&result, &buf, NULL)) {
    DWORD length;
    
    length = GetFinalPathNameByHandleW(h, (wchar_t*)buf, size, flags);
    
    if(length < size) {
      if( length >= 7 && 
          buf[0] == '\\' && 
          buf[1] == '\\' && 
          buf[2] == '?' && 
          buf[3] == '\\' && 
          buf[5] == ':' && 
          buf[6] == '\\') 
      {
        memmove(buf, buf + 4, sizeof(uint16_t) * (length - 4));
        length-= 4;
      }
    }
    
    pmath_string_end_write(&result, &buf);
    if(length > 0 && length < size) {
      result = pmath_string_part(result, 0, (int)length);
      
      return result;
    }
  }
  
  pmath_unref(result);
  return PMATH_UNDEFINED;
}
#endif

PMATH_PRIVATE pmath_t eval_System_FileInformation(pmath_expr_t expr) {
  /* FileInformation(filename)
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
  
  name = pmath_to_absolute_file_name(name);
  
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
        
        fullname = get_final_path_name(h);
        if(pmath_same(fullname, PMATH_UNDEFINED))
          fullname = pmath_ref(name);
        
        size[0] = info.nFileSizeLow;
        size[1] = info.nFileSizeHigh;

        bytecount = pmath_integer_new_data(2, -1, sizeof(DWORD), 0, 0, size);
        
        // TODO: as FileType(), check for "\\servername\pipe\..."
        if(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          filetype = pmath_ref(pmath_System_Directory);
        else if(info.dwFileAttributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT))
          filetype = pmath_ref(pmath_System_Special);
        else
          filetype = pmath_ref(pmath_System_File);
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
        fullname = pmath_ref(name);
        
        bytecount = pmath_integer_new_data(1, -1, sizeof(off_t), 0, 0, &buf.st_size);
        
        if(S_ISDIR(buf.st_mode))
          filetype = pmath_ref(pmath_System_Directory);
        else if(S_ISREG(buf.st_mode))
          filetype = pmath_ref(pmath_System_File);
        else
          filetype = pmath_ref(pmath_System_Special);
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
        pmath_ref(pmath_System_Rule), 2,
        pmath_ref(pmath_System_File), 
        fullname),
      PMATH_NULL);
  }
  
  if(!pmath_same(filetype, PMATH_UNDEFINED)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(pmath_System_Rule), 2,
        pmath_ref(pmath_System_FileType), 
        filetype),
      PMATH_NULL);
  }
  
  if(!pmath_same(bytecount, PMATH_UNDEFINED)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(pmath_System_Rule), 2,
        pmath_ref(pmath_System_ByteCount), 
        bytecount),
      PMATH_NULL);
  }
  
  return pmath_gather_end();
}

