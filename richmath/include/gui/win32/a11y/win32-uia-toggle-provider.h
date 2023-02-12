#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TOGGLE_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TOGGLE_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class OpenerBox;
  class CheckboxBox;
  class SetterBox;
  
  class Win32UiaToggleProvider:
    public IToggleProvider
  {
      class Impl;
    protected:
      explicit Win32UiaToggleProvider(FrontEndReference obj_ref);
      Win32UiaToggleProvider(const Win32UiaToggleProvider&) = delete;
      
      virtual ~Win32UiaToggleProvider();
    
    public:
      static Win32UiaToggleProvider *create(CheckboxBox *box);
      static Win32UiaToggleProvider *create(OpenerBox *box);
      static Win32UiaToggleProvider *try_create(FrontEndObject *obj);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IToggleProvider methods
      //
      STDMETHODIMP Toggle(void) override;
      STDMETHODIMP get_ToggleState(enum ToggleState *pRetVal) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TOGGLE_PROVIDER_H__INCLUDED
