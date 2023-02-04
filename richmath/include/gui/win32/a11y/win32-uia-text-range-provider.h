#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_RANGE_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_RANGE_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/selections.h>
#include <gui/win32/ole/comsidechannel.h>
#include <uiautomationcore.h>

namespace richmath {
  class Win32UiaTextRangeProvider : 
      public ITextRangeProvider,
      public ComSideChannelBase 
  {
      class Impl;
    public:
      explicit Win32UiaTextRangeProvider(SelectionReference range);
      Win32UiaTextRangeProvider(const Win32UiaTextRangeProvider&) = delete;
      
    protected:
      virtual ~Win32UiaTextRangeProvider();
    
    public:
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // ITextRangeProvider methods
      //
      STDMETHODIMP Clone(ITextRangeProvider **pRetVal) override;
      STDMETHODIMP Compare(ITextRangeProvider *other, BOOL *pRetVal) override;
      STDMETHODIMP CompareEndpoints(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint, int *pRetVal) override;
      STDMETHODIMP ExpandToEnclosingUnit(enum TextUnit unit) override;
      STDMETHODIMP FindAttribute(TEXTATTRIBUTEID attributeId, VARIANT val, BOOL backward, ITextRangeProvider **pRetVal) override;
      STDMETHODIMP FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider **pRetVal) override;
      STDMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attributeId, VARIANT *pRetVal) override;
      STDMETHODIMP GetBoundingRectangles(SAFEARRAY **pRetVal) override;
      STDMETHODIMP GetEnclosingElement(IRawElementProviderSimple **pRetVal) override;
      STDMETHODIMP GetText(int maxLength, BSTR *pRetVal) override;
      STDMETHODIMP Move(enum TextUnit unit, int count, int *pRetVal) override;
      STDMETHODIMP MoveEndpointByUnit( enum TextPatternRangeEndpoint endpoint, enum TextUnit unit, int count, int *pRetVal) override;
      STDMETHODIMP MoveEndpointByRange(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint) override;
      STDMETHODIMP Select(void) override;
      STDMETHODIMP AddToSelection(void) override;
      STDMETHODIMP RemoveFromSelection(void) override;
      STDMETHODIMP ScrollIntoView(BOOL alignToTop) override;
      STDMETHODIMP GetChildren(SAFEARRAY * *pRetVal) override;
      
    private:
      LONG refcount;
      SelectionReference range;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_RANGE_PROVIDER_H__INCLUDED
