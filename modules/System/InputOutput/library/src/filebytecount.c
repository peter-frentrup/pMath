#include "stdafx.h"


extern pmath_symbol_t pmath_System_DollarFailed;

static pmath_t file_to_bytecount(pmath_string_t name){ // name will be freed
#ifdef PMATH_OS_WIN32
  {
    const uint16_t zero = 0;
    HANDLE h;

    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);

    // use CreateFile() instead of GetFileAttributes() to follow symbolic links.
    h = CreateFileW(
          (const wchar_t*)pmath_string_buffer(&name),
          0,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          NULL,
          OPEN_EXISTING,
          0,
          NULL);

    if(h != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION info;

      if(GetFileInformationByHandle(h, &info)) {
        DWORD size[2];
        pmath_unref(name);
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
    char *str = pmath_string_to_native(name, NULL);

    if(str) {
      struct stat buf;

      if(stat(str, &buf) == 0) {
        pmath_mem_free(str);
        pmath_unref(name);

        return pmath_integer_new_data(1, -1, sizeof(off_t), 0, 0, &buf.st_size);
      }

      pmath_mem_free(str);
    }
  }
#endif

  pmath_unref(name);
  return pmath_ref(pmath_System_DollarFailed);
}


PMATH_PRIVATE pmath_t eval_System_FileByteCount(pmath_expr_t expr) {
  /* FileByteCount(file)
   */
  pmath_t obj;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(obj) || pmath_string_length(obj) == 0) {
    pmath_message(PMATH_NULL, "fstr", 1, obj);
    return expr;
  }
  obj = pmath_to_absolute_file_name(obj);
  
  obj = file_to_bytecount(obj);
  
  if(pmath_same(obj, pmath_System_DollarFailed)) {
    pmath_message(PMATH_NULL, "nffil", 1, pmath_ref(expr));
  }
  
  pmath_unref(expr);
  return obj;
}
