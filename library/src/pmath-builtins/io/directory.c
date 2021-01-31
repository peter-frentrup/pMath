#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files/filesystem.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  include <Windows.h>
#else
#  include <errno.h>
#  include <pwd.h>
#  include <string.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif


extern pmath_symbol_t pmath_System_DollarCurrentDirectory;
extern pmath_symbol_t pmath_System_DollarDirectoryStack;
extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Prepend;

PMATH_PRIVATE
pmath_string_t _pmath_get_directory(void) {
#ifdef PMATH_OS_WIN32
  {
    pmath_string_t result;
    wchar_t *tmp_buffer;
    wchar_t *result_buffer;
    DWORD    tmplen = 0;
    DWORD    reslen = 0;

    tmplen = GetCurrentDirectory(0, NULL);
    if((int)tmplen * sizeof(wchar_t) <= 0)
      return PMATH_NULL;
    
    tmp_buffer = pmath_mem_alloc(sizeof(wchar_t) * tmplen);
    if(!tmp_buffer)
      return PMATH_NULL;

    GetCurrentDirectoryW(tmplen, tmp_buffer);
    tmp_buffer[tmplen - 1] = L'\0';

    reslen = GetFullPathNameW(tmp_buffer, 0, NULL, NULL);
    if((int)reslen * sizeof(uint16_t) <= 0) {
      pmath_mem_free(tmp_buffer);
      return PMATH_NULL;
    }

    result =  pmath_string_new_raw(reslen);
    if(pmath_string_begin_write(&result, &result_buffer, NULL)) {
      GetFullPathNameW(tmp_buffer, reslen, result_buffer, NULL);
      pmath_string_end_write(&result, &result_buffer);
    }
    else
      reslen = 1;
    
    pmath_mem_free(tmp_buffer);
    return pmath_string_part(result, 0, reslen-1);
  }
#else
  {
    char   *buffer = NULL;
    size_t  size = 256;
    for(;;) {
      buffer = pmath_mem_realloc(buffer, 2 * size);
      size *= 2;

      if(!buffer)
        return PMATH_NULL;

      if(getcwd(buffer, size)) {
        pmath_string_t result = pmath_string_from_native(buffer, -1);
        pmath_mem_free(buffer);
        return result;
      }

      if(errno != ERANGE) {
        pmath_mem_free(buffer);
        return PMATH_NULL;
      }
    }
  }
#endif
}

static pmath_bool_t try_change_directory(
  pmath_string_t name // will be freed
) {
#ifdef PMATH_OS_WIN32
  {
    static const uint16_t zero = 0;

    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);
    if(pmath_is_null(name))
      return FALSE;

    if(SetCurrentDirectoryW((const wchar_t *)pmath_string_buffer(&name))) {
      pmath_unref(name);
      return TRUE;
    }

    pmath_unref(name);
    return FALSE;
  }
#else
  {
    pmath_bool_t result = FALSE;
    char *str = pmath_string_to_native(name, NULL);

    if(str && !chdir(str)) {
      result = TRUE;
    }

    pmath_mem_free(str);
    pmath_unref(name);
    return result;
  }
#endif
}

PMATH_PRIVATE pmath_t builtin_internal_getcurrentdirectory(pmath_expr_t expr) {
  /* Internal`GetCurrentDirectory()
   */
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }

  pmath_unref(expr);

  expr = _pmath_get_directory();
  if(pmath_is_null(expr))
    return pmath_ref(pmath_System_DollarFailed);

  return expr;
}

PMATH_PRIVATE pmath_t builtin_directoryname(pmath_expr_t expr) {
  pmath_string_t name, obj;
  size_t count, exprlen;
  const uint16_t *buf;
  int len, i;

  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)) {
    pmath_unref(name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }

  count = 1;
  if(exprlen == 2) {
    obj = pmath_expr_get_item(expr, 2);
    if( !pmath_is_int32(obj) ||
        PMATH_AS_INT32(obj) < 0)
    {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intpm", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
      return expr;
    }

    count = PMATH_AS_INT32(obj);
    pmath_unref(obj);
  }

  pmath_unref(expr);
  buf = pmath_string_buffer(&name);
  len = pmath_string_length(name);
  i = len;
  while(count-- > 0) {
    --i;
#ifdef PMATH_OS_WIN32
    while(i >= 0 && buf[i] != '\\' && buf[i] != '/')
      --i;
#else
    while(i >= 0 && buf[i] != '\\' && buf[i] != '/')
      --i;
#endif
  }

  if(i < 0) {
    pmath_unref(name);
    return pmath_string_new(0);
  }

  return pmath_string_part(name, 0, i + 1);
}

PMATH_PRIVATE pmath_t builtin_parentdirectory(pmath_expr_t expr) {
  pmath_string_t dir;

  if(pmath_expr_length(expr) > 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
    return expr;
  }

  if(pmath_expr_length(expr) == 1)
    dir = pmath_expr_get_item(expr, 1);
  else
    dir = _pmath_get_directory();

  if(!pmath_is_string(dir) || pmath_string_length(dir) < 1) {
    pmath_message(PMATH_NULL, "fstr", 1, dir);
    return expr;
  }

  pmath_unref(expr);

  dir = pmath_to_absolute_file_name(dir);
  if(!pmath_is_null(dir)) {
    const uint16_t *buf = pmath_string_buffer(&dir);
    int             len = pmath_string_length(dir);
    int i;

    --len;
    while(len >= 0 && buf[len] != '\\' && buf[len] != '/')
      --len;

    i = len - 1;
    while(i >= 0 && buf[i] != '\\' && buf[i] != '/')
      --i;

    return pmath_string_part(dir, 0, len + (i < 0));
  }

  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_assign_currentdirectory(pmath_expr_t expr) {
  /* $CurrentDirectory:= ...
   */
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;

  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(!pmath_same(tag, PMATH_UNDEFINED) || !pmath_same(lhs, pmath_System_DollarCurrentDirectory)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  pmath_unref(expr);
  if(!pmath_is_string(rhs) || pmath_string_length(rhs) == 0) {
    pmath_unref(lhs);
    pmath_message(PMATH_NULL, "fstr", 1, pmath_ref(rhs));
  }
  else if(!try_change_directory(pmath_ref(rhs))) {
    pmath_message(PMATH_NULL, "cdir", 1, pmath_ref(rhs));
  }

  pmath_unref(rhs);
  return _pmath_get_directory();
}

PMATH_PRIVATE pmath_t builtin_setdirectory(pmath_expr_t expr) {
  pmath_string_t name;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name) || pmath_string_length(name) == 0) {
    pmath_message(PMATH_NULL, "fstr", 1, name);
    return expr;
  }

  pmath_unref(expr);
  expr = _pmath_get_directory();

  if(!try_change_directory(pmath_ref(name))) {
    pmath_message(PMATH_NULL, "cdir", 1, name);
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarFailed);
  }

  pmath_unref(name);

  name = pmath_evaluate(pmath_expr_new_extended(
                          pmath_ref(pmath_System_Prepend), 2,
                          pmath_thread_local_load(pmath_System_DollarDirectoryStack),
                          expr));

  pmath_unref(pmath_thread_local_save(pmath_System_DollarDirectoryStack, name));

  return _pmath_get_directory();
}

PMATH_PRIVATE pmath_t builtin_resetdirectory(pmath_expr_t expr) {
  pmath_t dirstack;
  pmath_string_t name;

  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }

  pmath_unref(expr);
  dirstack = pmath_thread_local_load(pmath_System_DollarDirectoryStack);
  if( !pmath_is_expr_of(dirstack, pmath_System_List) ||
      pmath_expr_length(dirstack) == 0)
  {
    pmath_message(PMATH_NULL, "dtop", 0);
    pmath_unref(dirstack);
    return _pmath_get_directory();
  }

  name = pmath_expr_get_item(dirstack, 1);

  if(!try_change_directory(pmath_ref(name))) {
    pmath_message(PMATH_NULL, "cdir", 1, name);
    pmath_unref(dirstack);
    return pmath_ref(pmath_System_DollarFailed);
  }

  pmath_unref(
    pmath_thread_local_save(
      pmath_System_DollarDirectoryStack,
      pmath_expr_get_item_range(dirstack, 2, SIZE_MAX)));
  pmath_unref(dirstack);

  return name;
}
