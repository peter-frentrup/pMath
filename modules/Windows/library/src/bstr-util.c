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
