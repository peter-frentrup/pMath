#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_SELECTION_ITEM_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_SELECTION_ITEM_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class SetterBox;
  class RadioButtonBox;
  
  class Win32UiaSelectionItemProvider:
    public ISelectionItemProvider
  {
      class Impl;
    protected:
      explicit Win32UiaSelectionItemProvider(FrontEndReference obj_ref);
      Win32UiaSelectionItemProvider(const Win32UiaSelectionItemProvider&) = delete;
      
      virtual ~Win32UiaSelectionItemProvider();
    
    public:
      static Win32UiaSelectionItemProvider *create(SetterBox      *setter);
      static Win32UiaSelectionItemProvider *create(RadioButtonBox *radio);
      static Win32UiaSelectionItemProvider *try_create(FrontEndObject *obj);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // ISelectionItemProvider methods
      //
      STDMETHODIMP Select(void) override;
      STDMETHODIMP AddToSelection(void) override;
      STDMETHODIMP RemoveFromSelection(void) override;
      STDMETHODIMP get_IsSelected(BOOL *pRetVal) override;
      STDMETHODIMP get_SelectionContainer(IRawElementProviderSimple **pRetVal) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_SELECTION_ITEM_PROVIDER_H__INCLUDED
