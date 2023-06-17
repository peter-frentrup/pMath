#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_MULTIBOX_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_MULTIBOX_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <uiautomationcore.h>
#include <util/selections.h>
#include <gui/win32/ole/comsidechannel.h>


namespace richmath {
  class Box;
  
  /// A provider for multiple boxes in a row.
  /// Win32UiaMultiBoxProvider is used for implied constructs like SectionGroup, which have no single backing box, but are implied.
  /// Conceptually, a Win32UiaBoxProvider(box) is similar to Win32UiaMultiBoxProvider(box, 0..box.length()), 
  /// but Win32UiaMultiBoxProvider supports fewer Pattern Providers, becuase it is only "part of a whole".
  class Win32UiaMultiBoxProvider : 
      public IRawElementProviderSimple, 
      public IRawElementProviderFragment,
      public IRawElementProviderFragmentRoot,
      public ITextProvider2, 
      public IExpandCollapseProvider,
      public ComSideChannelBase 
  {
      class Impl;
    protected:
      explicit Win32UiaMultiBoxProvider(SelectionReference sel_ref);
      Win32UiaMultiBoxProvider(const Win32UiaMultiBoxProvider&) = delete;
      
      virtual ~Win32UiaMultiBoxProvider();
    
    public:
      static Win32UiaMultiBoxProvider *create_multi(VolatileSelection sel);
      static IRawElementProviderFragment *create_multi_or_inner_single(VolatileSelection sel);
      static IRawElementProviderFragment *create_multi_or_outer_single(VolatileSelection sel);
      
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
      
      //
      // IExpandCollapseProvider methods
      //
      STDMETHODIMP Expand(void) override;
      STDMETHODIMP Collapse(void) override;
      STDMETHODIMP get_ExpandCollapseState(enum ExpandCollapseState *pRetVal) override;
      
    public:
      SelectionReference get() { return sel_ref; }
      VolatileSelection get_now() { return sel_ref.get_all(); }
      
      static VolatileSelection find_outer_range(IRawElementProviderSimple *obj);
      
      static HRESULT NavigateImpl(const VolatileSelection &own_sel, enum NavigateDirection direction, IRawElementProviderFragment **pRetVal);
      
    private:
      LONG refcount;
      SelectionReference sel_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_MULTIBOX_PROVIDER_H__INCLUDED
