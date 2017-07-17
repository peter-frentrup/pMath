#define COBJMACROS

#include "util.h"

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
  
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

pmath_bool_t check_succeeded(HRESULT hr) {
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
  if(SUCCEEDED(GetErrorInfo(0, &iei)) && iei) {
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
  
//  pmath_message(PMATH_NULL, "hr", 5,
//    pmath_expr_new_extended(
//      pmath_ref(PMATH_SYMBOL_BASEFORM), 2,
//      pmath_integer_new_ui32((DWORD)hr),
//      PMATH_FROM_INT32(16)),
//    pmath_ref(description),
//    PMATH_FROM_INT32(hr),
//    PMATH_FROM_INT32(facility),
//    PMATH_FROM_INT32(code));
//  return FALSE;

  pmath_gather_begin(PMATH_NULL);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RULE), 2,
      pmath_ref(PMATH_SYMBOL_HEAD),
      pmath_current_head()),
    PMATH_NULL);
      
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RULE), 2,
      PMATH_C_STRING("HResult"),
      pmath_integer_new_ui32(hr)),
    PMATH_NULL);
      
  if(!pmath_is_null(description)) {
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
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
