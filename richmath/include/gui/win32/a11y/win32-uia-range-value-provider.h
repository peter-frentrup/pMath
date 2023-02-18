#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_RANGE_VALUE_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_RANGE_VALUE_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class ProgressIndicatorBox;
  
  class Win32UiaRangeValueProvider:
    public IRangeValueProvider
  {
      class Impl;
    protected:
      explicit Win32UiaRangeValueProvider(FrontEndReference obj_ref);
      Win32UiaRangeValueProvider(const Win32UiaRangeValueProvider&) = delete;
      
      virtual ~Win32UiaRangeValueProvider();
    
    public:
      static Win32UiaRangeValueProvider *create(ProgressIndicatorBox *box);
      static Win32UiaRangeValueProvider *try_create(FrontEndObject *obj);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IRangeValueProvider  methods
      //
      STDMETHODIMP SetValue(double val) override;
      STDMETHODIMP get_Value(double *pRetVal) override;
      STDMETHODIMP get_IsReadOnly(BOOL *pRetVal) override;
      STDMETHODIMP get_Maximum(double *pRetVal) override;
      STDMETHODIMP get_Minimum(double *pRetVal) override;
      STDMETHODIMP get_LargeChange(double *pRetVal) override;
      STDMETHODIMP get_SmallChange(double *pRetVal) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_RANGE_VALUE_PROVIDER_H__INCLUDED
