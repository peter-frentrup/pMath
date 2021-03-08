#ifndef RICHMATH__GUI__WIN32__OLE__PROPVAR_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__PROPVAR_H__INCLUDED

#include <combaseapi.h>
#include <propvarutil.h>
#include <gui/win32/ole/combase.h>
#include <util/pmath-extra.h>


namespace richmath {
  class PropertyVariant {
    public:
      PropertyVariant() { PropVariantInit(&value); }
      PropertyVariant(const PropertyVariant &other) {
        PropVariantCopy(&value, &other.value);
      }
      PropertyVariant(PropertyVariant &&other) {
        PropVariantInit(&value);
        swap(*this, other);
      }
      ~PropertyVariant() { HRreport(PropVariantClear(&value)); }
      
      PropertyVariant(const wchar_t *s) {
        InitPropVariantFromString(s, &value);
      }
      PropertyVariant(String s) {
        s+= String::FromChar(0);
        InitPropVariantFromString(s.buffer_wchar(), &value);
      }
      
      friend void swap(PropertyVariant &left, PropertyVariant &right) noexcept {
        using std::swap;
        swap(left.value, right.value);
      }
      
      PropertyVariant &operator=(PropertyVariant other) {
        swap(*this, other);
        return *this;
      }
      
      void reset() {
        HRreport(PropVariantClear(&value));
        PropVariantInit(&value);
      }
      
      explicit operator bool() const { return !is_empty(); }
      bool is_empty() const { return value.vt == VT_EMPTY; }
      bool is_string() const { return IsPropVariantString(value); }
      
      HRESULT to_int32(int *i) const { return PropVariantToInt32(value, i); }
      String to_string_or_null() const {
        // TODO: handle ANSI strings (VT_LPSTR)
        const wchar_t *str = PropVariantToStringWithDefault(value, nullptr);
        if(str)
          return String::FromUcs2((const uint16_t*)str, -1); 
        else
          return String();
      }
      
    public:
      PROPVARIANT value;
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__PROPVAR_H__INCLUDED
