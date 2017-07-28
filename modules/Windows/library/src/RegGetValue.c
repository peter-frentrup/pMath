#define _WIN32_WINNT  0x0600

#include "registry.h"
#include "util.h"

#include <pmath.h>
#include <windows.h>


#define EQUAL_IGNORECASE(BUF, LEN, STR) \
  (CSTR_EQUAL == CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, BUF, LEN, STR, sizeof(STR)/sizeof(STR[0])))

extern const pmath_symbol_t *Windows_Win64Version;


pmath_string_t registry_split_subkey(HKEY *out_base, pmath_string_t fullname) {
  const wchar_t *buf = pmath_string_buffer(&fullname);
  int len = pmath_string_length(fullname);
  int i;
  
  assert(out_base != NULL);
  
  i = 0;
  while(i < len && buf[i] != L'\\')
    ++i;
    
  if(EQUAL_IGNORECASE(buf, i, L"HKEY_CLASSES_ROOT")) {
    *out_base = HKEY_CLASSES_ROOT;
  }
  else if(EQUAL_IGNORECASE(buf, i, L"HKEY_CURRENT_USER")) {
    *out_base = HKEY_CURRENT_USER;
  }
  else if(EQUAL_IGNORECASE(buf, i, L"HKEY_LOCAL_MACHINE")) {
    *out_base = HKEY_LOCAL_MACHINE;
  }
  else if(EQUAL_IGNORECASE(buf, i, L"HKEY_USERS")) {
    *out_base = HKEY_USERS;
  }
  else if(EQUAL_IGNORECASE(buf, i, L"HKEY_CURRENT_CONFIG")) {
    *out_base = HKEY_CURRENT_CONFIG;
  }
  else {
    *out_base = NULL;
    pmath_message(PMATH_NULL, "key", 1, fullname);
    return PMATH_NULL;
  }
  
  if(i < len)
    ++i;
    
  return pmath_string_part(fullname, i, -1);
}

pmath_bool_t registry_set_wow64_access_option(REGSAM *inout_acces_rights, pmath_t options) {
  pmath_t opt;
  if(pmath_is_null(options))
    return FALSE;
    
  opt = pmath_option_value(PMATH_NULL, *Windows_Win64Version, options);
  
  if(pmath_same(opt, PMATH_SYMBOL_TRUE)) {
    *inout_acces_rights |= KEY_WOW64_64KEY;
    pmath_unref(opt);
    return TRUE;
  }
  
  if(pmath_same(opt, PMATH_SYMBOL_FALSE)) {
    *inout_acces_rights |= KEY_WOW64_32KEY;
    pmath_unref(opt);
    return TRUE;
  }
  
  if(pmath_same(opt, PMATH_SYMBOL_AUTOMATIC)) {
    pmath_unref(opt);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "opttfa", 2, pmath_ref(*Windows_Win64Version), opt);
  return FALSE;
}

static pmath_bool_t set_noexpand_flag(DWORD *inout_flags, pmath_t options) {
  pmath_t opt;
  if(pmath_is_null(options))
    return FALSE;
    
  opt = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_EXPAND, options);
  
  if(pmath_same(opt, PMATH_SYMBOL_FALSE)) {
    *inout_flags |= RRF_NOEXPAND;
  }
  else if(!pmath_same(opt, PMATH_SYMBOL_TRUE)) {
    pmath_message(PMATH_NULL, "opttfa", 2, pmath_ref(PMATH_SYMBOL_EXPAND), opt);
    return FALSE;
  }
  
  pmath_unref(opt);
  return TRUE;
}

static pmath_t binary_to_pmath(const uint8_t *data, DWORD size) {
  size_t dim = size;
  pmath_packed_array_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &dim, NULL, 0);
  int32_t *arr_data = pmath_packed_array_begin_write(&arr, NULL, 0);
  if(arr_data) {
    DWORD i;
    for(i = 0; i < size; ++i)
      arr_data[i] = (int32_t)data[i];
  }
  return arr;
}

static pmath_t reg_string_to_pmath(const wchar_t *str, DWORD size_in_bytes) {
  int len = (int)(size_in_bytes / sizeof(wchar_t));
  if(len > 0 && str[len - 1] == L'\0')
    --len;
    
  return pmath_string_insert_ucs2(PMATH_NULL, 0, str, len);
}

static pmath_t stringlist_to_pmath(const wchar_t *str, DWORD size_in_bytes) {
  const wchar_t *end = str + size_in_bytes / sizeof(wchar_t);
  
  pmath_gather_begin(PMATH_NULL);
  
  // TODO: stop at first empty string
  while(str != end) {
    const wchar_t *next;
    pmath_string_t substring;
    
    if(str + 2 == end && !str[0] && !str[1])
      break;
      
    next = str;
    while(next != end && *next)
      ++next;
      
    substring = pmath_string_insert_ucs2(PMATH_NULL, 0, str, (int)(next - str));
    pmath_emit(substring, PMATH_NULL);
    
    str = next;
    if(str != end)
      ++str;
  }
  
  return pmath_gather_end();
}

static pmath_t reg_data_to_pmath(DWORD type, const void *data, DWORD size) {
  switch(type) {
    case REG_NONE:
      return pmath_ref(PMATH_SYMBOL_NONE);
      
    case REG_SZ:
    case REG_EXPAND_SZ:
      return reg_string_to_pmath(data, size);
      
    case REG_LINK:
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_HOLD), 1, // TODO: use some designate "Link" head instead of "Hold"
               pmath_string_insert_ucs2(PMATH_NULL, 0, data, (int)size - 1));
               
    case REG_BINARY:
      return binary_to_pmath((const uint8_t*)data, size);
      
    case REG_DWORD:
      return pmath_integer_new_ui32(*(DWORD*)data);
      
    case REG_DWORD_BIG_ENDIAN:
      return pmath_integer_new_data(1, +1, size, +1, 0, data);
      
    case REG_QWORD:
      return pmath_integer_new_ui64(*(uint64_t*)data);
      
    case REG_MULTI_SZ:
      return stringlist_to_pmath((const wchar_t*)data, size);
  }
  
  pmath_message(PMATH_NULL, "type", 1, pmath_integer_new_ui32(type));
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

pmath_t windows_RegGetValue(pmath_expr_t expr) {
  /*  Windows`RegGetValue(keyName, valueName)
  
      options:
        Expand -> True
        Win64Version -> Automatic (or True or False)
   */
  pmath_string_t key_name;
  pmath_string_t value_name;
  pmath_t options;
  const wchar_t *key_name_buf;
  const wchar_t *value_name_buf;
  HKEY root;
  HKEY key;
  REGSAM desired_access = KEY_QUERY_VALUE;
  DWORD flags = RRF_RT_ANY;
  DWORD type;
  void *data;
  char default_data[8];
  DWORD required_size;
  LONG error_code;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, 2);
    return expr;
  }
  
  key_name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(key_name)) {
    pmath_unref(key_name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  value_name = pmath_expr_get_item(expr, 2);
  if(!pmath_is_string(value_name)) {
    pmath_unref(key_name);
    pmath_unref(value_name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    return expr;
  }
  
  key_name = registry_split_subkey(&root, key_name);
  if(pmath_is_null(key_name))
    return expr;
    
  key_name = pmath_string_insert_latin1(key_name, INT_MAX, "", 1);
  value_name = pmath_string_insert_latin1(value_name, INT_MAX, "", 1);
  
  key_name_buf = pmath_string_buffer(&key_name);
  value_name_buf = pmath_string_buffer(&value_name);
  if(!key_name_buf || !value_name_buf) {
    pmath_unref(key_name);
    pmath_unref(value_name);
    return expr;
  }
  
  options = pmath_options_extract(expr, 2);
  if( !registry_set_wow64_access_option(&desired_access, options) ||
      !set_noexpand_flag(&flags, options))
  {
    pmath_unref(key_name);
    pmath_unref(value_name);
    pmath_unref(options);
    return expr;
  }
  pmath_unref(options);
  
  data = default_data;
  
  /* Since RRF_SUBKEY_WOW6464KEY and RRF_SUBKEY_WOW6432KEY are Windows 10 only, we have to open the
     key manually with KEY_WOW64_64KEY or KEY_WOW64_32KEY access.
   */
  error_code = RegOpenKeyExW(root, key_name_buf, 0, desired_access, &key);
  if(!check_succeeded_win32(error_code))
    goto FAIL_OPENKEY;
    
  required_size = sizeof(default_data);
  error_code = RegGetValueW(key, NULL, value_name_buf, flags, &type, data, &required_size);
  if(error_code == ERROR_MORE_DATA) {
    DWORD size;
    if(root == HKEY_PERFORMANCE_DATA)
      size = 2 * sizeof(default_data);
    else
      size = required_size;
      
    data = pmath_mem_alloc(size);
    while(data && !pmath_aborting()) {
      required_size = size;
      error_code = RegGetValueW(key, NULL, value_name_buf, flags, &type, data, &required_size);
      if(error_code != ERROR_MORE_DATA)
        break;
        
      size *= 2;
      data = pmath_mem_realloc(data, size);
    }
  }
  
  if(!check_succeeded_win32(error_code))
    goto FAIL_GETVALUE;
    
  RegCloseKey(key);
  pmath_unref(expr);
  
  expr = reg_data_to_pmath(type, data, required_size);
  
  pmath_unref(key_name);
  pmath_unref(value_name);
  if(data != default_data)
    pmath_mem_free(data);
    
  return expr;
  
FAIL_GETVALUE:
  RegCloseKey(key);
FAIL_OPENKEY:
  pmath_unref(expr);
  pmath_unref(key_name);
  pmath_unref(value_name);
  if(data != default_data)
    pmath_mem_free(data);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
