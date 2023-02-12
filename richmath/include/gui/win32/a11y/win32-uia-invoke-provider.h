#ifndef RICHMATH__GUI__WIN32__WIN32_UIA_INVOKE_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_UIA_INVOKE_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class AbstractButtonBox;
  
  class Win32UiaInvokeProvider:
    public IInvokeProvider
  {
      class Impl;
    protected:
      explicit Win32UiaInvokeProvider(FrontEndReference obj_ref);
      Win32UiaInvokeProvider(const Win32UiaInvokeProvider&) = delete;
      
      virtual ~Win32UiaInvokeProvider();
    
    public:
      static Win32UiaInvokeProvider *create(AbstractButtonBox *box);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IInvokeProvider methods
      //
      STDMETHODIMP STDMETHODCALLTYPE Invoke(void) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_UIA_INVOKE_PROVIDER_H__INCLUDED
