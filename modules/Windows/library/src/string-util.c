#include "util.h"


BSTR string_to_bstr_free(pmath_string_t str) {
  int length = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  BSTR bstr = SysAllocStringLen(buf, (UINT)length);
  pmath_unref(str);
  return bstr;
}

pmath_string_t bstr_to_string(BSTR bstr) {
  return pmath_string_insert_ucs2(PMATH_NULL, 0, bstr, SysStringLen(bstr));
}

pmath_bool_t string_to_guid(pmath_string_t string, GUID *guid) {
  BSTR bstr = string_to_bstr_free(pmath_ref(string));
  if(!SUCCEEDED(IIDFromString(bstr, guid))) {
    SysFreeString(bstr);
    return FALSE;
  }
  SysFreeString(bstr);
  return TRUE;
}

pmath_string_t guid_to_string(const GUID *guid) {
  LPOLESTR guid_string = NULL;
  if(SUCCEEDED(StringFromIID(guid, &guid_string))) {
    pmath_string_t result = pmath_string_insert_ucs2(PMATH_NULL, 0, guid_string, -1);
    CoTaskMemFree(guid_string);
    return result;
  }
  return PMATH_NULL;
}
