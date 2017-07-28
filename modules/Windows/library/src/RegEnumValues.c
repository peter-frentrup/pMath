#define _WIN32_WINNT  0x0600

#include "registry.h"
#include "util.h"

#include <pmath.h>
#include <windows.h>


static pmath_t enum_values(HKEY key) {
  wchar_t small_buffer[16];
  wchar_t *data = small_buffer;
  DWORD capacity = sizeof(small_buffer) / sizeof(small_buffer[0]);
  DWORD i;
  
  pmath_gather_begin(PMATH_NULL);
  
  for(i = 0;!pmath_aborting();++i) {
    DWORD size = capacity;
    DWORD error_code = RegEnumValueW(key, i, data, &size, NULL, NULL, NULL, NULL);
    
    if(error_code == ERROR_NO_MORE_ITEMS)
      break;
      
    if(error_code == ERROR_SUCCESS) {
      pmath_string_t name = pmath_string_insert_ucs2(PMATH_NULL, 0, data, (int)size);
      pmath_emit(name, PMATH_NULL);
      continue;
    }
    
    if(error_code == ERROR_MORE_DATA) {
      --i;
      capacity*= 2;
      if(data == small_buffer)
        data = pmath_mem_alloc(capacity * sizeof(wchar_t));
      else
        data = pmath_mem_realloc(data, capacity * sizeof(wchar_t));
      
      if(!data)
        break;
      
      continue;
    }
    
    check_succeeded_win32(error_code);
    break;
  }
  
  if(data != small_buffer)
    pmath_mem_free(data);
  
  return pmath_gather_end();
}

pmath_t windows_RegEnumValues(pmath_expr_t expr) {
  /*  Windows`RegEnumValues(keyName)
      
      options:
        Win64Version -> Automatic (or True or False)
   */
  pmath_string_t key_name;
  pmath_expr_t options;
  const wchar_t *key_name_buf;
  HKEY root;
  HKEY key;
  DWORD error_code;
  REGSAM desired_access = KEY_READ;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  key_name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(key_name)) {
    pmath_unref(key_name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  key_name = registry_split_subkey(&root, key_name);
  if(pmath_is_null(key_name))
    return expr;
    
  key_name = pmath_string_insert_latin1(key_name, INT_MAX, "", 1);
  key_name_buf = pmath_string_buffer(&key_name);
  if(!key_name_buf) {
    pmath_unref(key_name);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(!registry_set_wow64_access_option(&desired_access, options)) {
    pmath_unref(key_name);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(options);
  
  error_code = RegOpenKeyExW(root, key_name_buf, 0, desired_access, &key);
  pmath_unref(key_name);
  
  pmath_unref(expr);
  if(!check_succeeded_win32(error_code)) 
    return pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
  
  expr = enum_values(key);
  
  RegCloseKey(key);
  return expr;
}
