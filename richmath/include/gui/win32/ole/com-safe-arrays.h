#ifndef RICHMATH__GUI__WIN32__OLE__COM_SAFE_ARRAYS_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__COM_SAFE_ARRAYS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/combase.h>
#include <util/array.h>

namespace richmath {
  struct ComSafeArray {
    template <typename T>
    static HRESULT create(SAFEARRAY **res, ArrayView<ComBase<T>> objects) {
      *res = SafeArrayCreateVector(VT_UNKNOWN, 0, objects.length());
      if(!*res)
        return E_OUTOFMEMORY;
      
      for(int i = 0; i < objects.length(); ++i) {
        long index = i;
        HRESULT hr = HRreport(SafeArrayPutElement(*res, &index, objects[i].get()));
        if(FAILED(hr)) {
          SafeArrayDestroy(*res);
          *res = nullptr;
          return hr;
        }
      }
      
      return S_OK;
    }
    
    template <typename T>
    static HRESULT create_singleton(SAFEARRAY **res, ComBase<T> object) {
      *res = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
      if(!*res)
        return E_OUTOFMEMORY;
      
      long index = 0;
      HRESULT hr = HRreport(SafeArrayPutElement(*res, &index, object.get()));
      if(FAILED(hr)) {
        SafeArrayDestroy(*res);
        *res = nullptr;
        return hr;
      }
      
      return S_OK;
    }
    
    static HRESULT put_double(SAFEARRAY *res, long index, double value) {
      return SafeArrayPutElement(res, &index, &value);
    }
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__COM_SAFE_ARRAYS_H__INCLUDED
