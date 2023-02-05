#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_BOX_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_BOX_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <uiautomationcore.h>
#include <util/frontendobject.h>
#include <gui/win32/ole/comsidechannel.h>

namespace richmath {
  class FrontEndObject;
  class Box;
  
  class Win32UiaBoxProvider : 
      public IRawElementProviderSimple, 
      public IRawElementProviderFragment,
      public IRawElementProviderFragmentRoot,
      public ITextProvider2, 
      public ComSideChannelBase 
  {
      class Impl;
    protected:
      explicit Win32UiaBoxProvider(FrontEndReference obj_ref);
      Win32UiaBoxProvider(const Win32UiaBoxProvider&) = delete;
      
      virtual ~Win32UiaBoxProvider();
    
    public:
      static Win32UiaBoxProvider *create(Box * box);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IRawElementProviderSimple methods
      //
      STDMETHODIMP get_ProviderOptions(enum ProviderOptions *pRetVal) override;
      STDMETHODIMP GetPatternProvider(PATTERNID patternId, IUnknown **pRetVal) override;
      STDMETHODIMP GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) override;
      STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple **pRetVal) override;
      
      //
      // IRawElementProviderFragment methods
      //
      STDMETHODIMP Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal) override;
      STDMETHODIMP GetRuntimeId(SAFEARRAY **pRetVal) override;
      STDMETHODIMP get_BoundingRectangle(struct UiaRect *pRetVal) override;
      STDMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal) override;
      STDMETHODIMP SetFocus(void) override;
      STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal) override;
      
      //
      // IRawElementProviderFragmentRoot methods
      //
      STDMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment **pRetVal) override;
      STDMETHODIMP GetFocus(IRawElementProviderFragment **pRetVal) override;
      
      //
      // ITextProvider methods (from ITextProvider2)
      //
      STDMETHODIMP GetSelection(SAFEARRAY **pRetVal) override;
      STDMETHODIMP GetVisibleRanges(SAFEARRAY **pRetVal) override;
      STDMETHODIMP RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal) override;
      STDMETHODIMP RangeFromPoint(struct UiaPoint point, ITextRangeProvider **pRetVal) override;
      STDMETHODIMP get_DocumentRange(ITextRangeProvider **pRetVal) override;
      STDMETHODIMP get_SupportedTextSelection(enum SupportedTextSelection *pRetVal) override;
        
      //
      // ITextProvider2 methods
      //
      STDMETHODIMP RangeFromAnnotation(IRawElementProviderSimple *annotationElement, ITextRangeProvider **pRetVal) override;
      STDMETHODIMP GetCaretRange(BOOL *isActive, ITextRangeProvider **pRetVal) override;
      
    public:
      FrontEndObject *get_object();
      template <typename T> T *get() { return dynamic_cast<T*>(get_object()); }
      
      static FrontEndObject *find(IRawElementProviderSimple *obj);
      
      template <typename T>
      static T *find_cast(IRawElementProviderSimple *obj) { return dynamic_cast<T*>(find(obj)); }
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_BOX_PROVIDER_H__INCLUDED
