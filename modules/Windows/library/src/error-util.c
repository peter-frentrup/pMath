#define COBJMACROS

#include "util.h"


extern pmath_symbol_t p4win_System_DollarFailed;
extern pmath_symbol_t p4win_System_Head;
extern pmath_symbol_t p4win_System_Rule;

static pmath_t format_win32_message(DWORD errorCode) {
  wchar_t *errorText = NULL;
  
  FormatMessageW(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    errorCode,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPWSTR)&errorText,
    0,
    NULL);
    
  if(errorText) {
    pmath_string_t str = pmath_string_insert_ucs2(PMATH_NULL, 0, errorText, -1);
    LocalFree(errorText);
    return str;
  }
  
  return pmath_ref(p4win_System_DollarFailed);
}

static pmath_bool_t check_succeeded_internal(HRESULT hr, pmath_bool_t use_ierrorinfo) {
  pmath_t description, options;
  WORD facility;
  WORD code;
  IErrorInfo *iei;
  
  if(SUCCEEDED(hr))
    return TRUE;
    
// http://stackoverflow.com/questions/4597932/how-can-i-is-there-a-way-to-convert-an-hresult-into-a-system-specific-error-me/4597974#4597974
  description = PMATH_NULL;
  facility = HRESULT_FACILITY(hr);
  code = HRESULT_CODE(hr);
  
  iei = NULL;
  if(use_ierrorinfo && SUCCEEDED(GetErrorInfo(0, &iei)) && iei) {
    BSTR bstr = NULL;
    if(SUCCEEDED(IErrorInfo_GetDescription(iei, &bstr))) {
      description = bstr_to_string(bstr);
      SysFreeString(bstr);
    }
    IErrorInfo_Release(iei);
  }
  else if(facility == FACILITY_WIN32) {
    description = format_win32_message(code);
  }
  
  pmath_gather_begin(PMATH_NULL);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(p4win_System_Rule), 2,
      pmath_ref(p4win_System_Head),
      pmath_current_head()),
    PMATH_NULL);
    
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(p4win_System_Rule), 2,
      PMATH_C_STRING("HResult"),
      pmath_integer_new_ui32(hr)),
    PMATH_NULL);
    
  if(!pmath_is_null(description)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(p4win_System_Rule), 2,
        PMATH_C_STRING("Description"),
        description),
      PMATH_NULL);
  }
  
  options = pmath_gather_end();
  
  PMATH_RUN_ARGS(
    "Windows`HandleHResultError(`1`)",
    "(o)",
    options);
    
  return FALSE;
}

pmath_bool_t check_succeeded(HRESULT hr) {
  return check_succeeded_internal(hr, TRUE);
}

pmath_bool_t check_succeeded_win32(DWORD error_code) {
  return check_succeeded_internal(HRESULT_FROM_WIN32(error_code), FALSE);
}
