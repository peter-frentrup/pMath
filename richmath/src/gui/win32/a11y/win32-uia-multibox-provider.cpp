#include <initguid.h>

#include <gui/win32/a11y/win32-uia-multibox-provider.h>

#include <boxes/box.h>
#include <boxes/abstractsequence.h>
#include <boxes/inputfieldbox.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>
#include <boxes/tooltipbox.h>

#include <eval/application.h>
#include <gui/document.h>
#include <gui/win32/win32-widget.h>

#include <util/text-gathering.h>

#include <gui/win32/a11y/win32-uia-box-provider.h>
#include <gui/win32/a11y/win32-uia-text-range-provider.h>
#include <gui/win32/ole/com-safe-arrays.h>

#include <propvarutil.h>
#include <uiautomation.h>


using namespace richmath;

namespace richmath {
  class Win32UiaMultiBoxProvider::Impl {
    public:
      Impl(Win32UiaMultiBoxProvider &self);
      
      HRESULT get_BoundingRectangle(struct UiaRect *pRetVal);
      HRESULT get_BoundingRectangle(VARIANT *pRetVal);
      HRESULT get_ClassName(VARIANT *pRetVal);
      HRESULT get_ControlType(VARIANT *pRetVal);
      HRESULT get_HasKeyboardFocus(VARIANT *pRetVal);
      HRESULT get_HelpText(VARIANT *pRetVal);
      HRESULT get_IsEnabled(VARIANT *pRetVal);
      HRESULT get_IsKeyboardFocusable(VARIANT *pRetVal);
      HRESULT get_Name(VARIANT *pRetVal);
      
      static IRawElementProviderSimple *create_single_or_multi(VolatileSelection sel);
      
    private:
      Win32UiaMultiBoxProvider &self;
  };
}

namespace richmath{namespace strings{
  extern String PlainText;
}}

extern pmath_symbol_t richmath_FE_BoxesToText;

//{ class Win32UiaMultiBoxProvider ...

Win32UiaMultiBoxProvider::Win32UiaMultiBoxProvider(SelectionReference sel_ref)
  : refcount(1),
    sel_ref(sel_ref)
{
  //fprintf(stderr, "[%p = new Win32UiaMultiBoxProvider(%d, %d..%d)]\n", this, sel_ref.id, sel_ref.start, sel_ref.end);
}

Win32UiaMultiBoxProvider::~Win32UiaMultiBoxProvider() {
  //fprintf(stderr, "[delete Win32UiaMultiBoxProvider %p(%d, %d..%d)]\n", this, sel_ref.id, sel_ref.start, sel_ref.end);
}

Win32UiaMultiBoxProvider *Win32UiaMultiBoxProvider::create_multi(VolatileSelection sel) {
  if(!sel)
    return nullptr;
  
  return new Win32UiaMultiBoxProvider(SelectionReference(sel));
}

IRawElementProviderFragment *Win32UiaMultiBoxProvider::create_multi_or_inner_single(VolatileSelection sel) {
  if(!sel)
    return nullptr;
  
  if(auto inner = sel.contained_box())
    return Win32UiaBoxProvider::create(inner);
  
  return new Win32UiaMultiBoxProvider(SelectionReference(sel));
}

IRawElementProviderFragment *Win32UiaMultiBoxProvider::create_multi_or_outer_single(VolatileSelection sel) {
  if(!sel)
    return nullptr;
  
  if(sel.start == 0 && sel.end == sel.box->length())
    return Win32UiaBoxProvider::create(sel.box);
  
  return new Win32UiaMultiBoxProvider(SelectionReference(sel));
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaMultiBoxProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IRawElementProviderSimple) {
    AddRef();
    *ppvObject = static_cast<IRawElementProviderSimple*>(this);
    return S_OK;
  }
  else if(iid == IID_IRawElementProviderFragment) {
    AddRef();
    *ppvObject = static_cast<IRawElementProviderFragment*>(this);
    return S_OK;
  }
  else if(iid == IID_IRawElementProviderFragmentRoot) {
    AddRef();
    *ppvObject = static_cast<IRawElementProviderFragmentRoot*>(this);
    return S_OK;
  }
  else if(iid == IID_ITextProvider) {
    AddRef();
    *ppvObject = static_cast<ITextProvider*>(this);
    return S_OK;
  }
  else if(iid == IID_ITextProvider2) {
    AddRef();
    *ppvObject = static_cast<ITextProvider2*>(this);
    return S_OK;
  }
  else if(iid == IID_IExpandCollapseProvider) {
    AddRef();
    *ppvObject = static_cast<IExpandCollapseProvider*>(this);
    return S_OK;
  }
  else if(iid == IID_IRichmathComSideChannel) {
    AddRef();
    *ppvObject = static_cast<IRichmathComSideChannel*>(this);
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaMultiBoxProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaMultiBoxProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
//  IRawElementProviderSimple::get_ProviderOptions
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_ProviderOptions(enum ProviderOptions *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!sel_ref)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = ProviderOptions(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
  return S_OK;
}

//
//  IRawElementProviderSimple::GetPatternProvider
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetPatternProvider(PATTERNID patternId, IUnknown **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread()) // should not happen, because we specify ProviderOptions_UseComThreading
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  switch(patternId) {
    case UIA_TextPatternId:
    case UIA_TextPattern2Id: 
      if(VolatileSelection sel = get_now()) {
        if(dynamic_cast<AbstractSequence*>(sel.box) || dynamic_cast<SectionList*>(sel.box)) {
          *pRetVal = static_cast<ITextProvider2 *>(this);
          (*pRetVal)->AddRef();
          return S_OK;
        }
//      FrontEndObject *obj = get_object();
//      if(auto owner = dynamic_cast<OwnerBox*>(obj)) {
//        *pRetVal = static_cast<ITextProvider2 *>(Win32UiaBoxProvider::create(owner->content()));
//      }
////      else if(auto sect = dynamic_cast<AbstractSequenceSection*>(obj)) {
////        *pRetVal = static_cast<ITextProvider2 *>(Win32UiaBoxProvider::create(sect->content()));
////      }
//      else /*if(dynamic_cast<AbstractSequence*>(obj) || dynamic_cast<Document*>(obj))*/ {
//        *pRetVal = static_cast<ITextProvider2 *>(this);
//        (*pRetVal)->AddRef();
//      }
      } 
      return S_OK;
//    
//    case UIA_GridPatternId:          *pRetVal = static_cast<IGridProvider*>(         Win32UiaGridProvider::create(get<GridBox>())); return S_OK;
//    case UIA_TablePatternId:         *pRetVal = static_cast<ITableProvider*>(        Win32UiaGridProvider::create(get<GridBox>())); return S_OK;
//    
//    case UIA_GridItemPatternId:      *pRetVal = static_cast<IGridItemProvider*>(     Win32UiaGridItemProvider::create(get<GridItem>())); return S_OK;
//    case UIA_TableItemPatternId:     *pRetVal = static_cast<ITableItemProvider*>(    Win32UiaGridItemProvider::create(get<GridItem>())); return S_OK;
//  
//    case UIA_InvokePatternId:        *pRetVal = static_cast<IInvokeProvider*>(       Win32UiaInvokeProvider::try_create(       get_object())); return S_OK;
//    case UIA_SelectionItemPatternId: *pRetVal = static_cast<ISelectionItemProvider*>(Win32UiaSelectionItemProvider::try_create(get_object())); return S_OK;
//    case UIA_TogglePatternId:        *pRetVal = static_cast<IToggleProvider*>(       Win32UiaToggleProvider::try_create(       get_object())); return S_OK;
//  
//    case UIA_RangeValuePatternId:    *pRetVal = static_cast<IRangeValueProvider*>(   Win32UiaRangeValueProvider::try_create(   get_object())); return S_OK;
  
    case UIA_ExpandCollapsePatternId: 
      if(VolatileSelection sel = get_now()) {
        if(dynamic_cast<SectionList*>(sel.box)) {
            *pRetVal = static_cast<IExpandCollapseProvider *>(this);
            (*pRetVal)->AddRef();
            return S_OK;
        }
      }
      return S_OK;
  }
  
  return S_OK;
}

//
//  IRawElementProviderSimple::GetPropertyValue
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  pRetVal->vt = VT_EMPTY; // Fall back to default value.
  
  switch(propertyId) {
    case UIA_ControlTypePropertyId: return Impl(*this).get_ControlType(pRetVal);
    case UIA_ClassNamePropertyId:   return Impl(*this).get_ClassName(pRetVal);
    case UIA_NamePropertyId:        return Impl(*this).get_Name(pRetVal);
      
    case UIA_AutomationIdPropertyId: 
//      pRetVal->bstrVal = SysAllocString(L"Text Area"); // TODO: use the BoxID if available.
//      if(pRetVal->bstrVal) {
//        pRetVal->vt = VT_BSTR;
//      } 
      break;
    
    case UIA_BoundingRectanglePropertyId: return Impl(*this).get_BoundingRectangle(pRetVal);
    
    case UIA_IsControlElementPropertyId: 
      pRetVal->vt      = VT_BOOL;
      pRetVal->boolVal = VARIANT_TRUE;
      break;
    
    case UIA_IsContentElementPropertyId: 
      pRetVal->vt      = VT_BOOL;
      pRetVal->boolVal = VARIANT_TRUE;
      break;
    
    case UIA_HasKeyboardFocusPropertyId:    return Impl(*this).get_HasKeyboardFocus(pRetVal);
    case UIA_HelpTextPropertyId:            return Impl(*this).get_HelpText(pRetVal);
    case UIA_IsEnabledPropertyId:           return Impl(*this).get_IsEnabled(pRetVal);
    case UIA_IsKeyboardFocusablePropertyId: return Impl(*this).get_IsKeyboardFocusable(pRetVal);
    
//    case UIA_ProviderDescriptionPropertyId:
//      pRetVal->bstrVal = SysAllocString(L"Richmath: Uia Box Content");
//      if(pRetVal->bstrVal) {
//        pRetVal->vt = VT_BSTR;
//      }
//      break;
  }
  
  return S_OK;
}

//
//  IRawElementProviderSimple::get_HostRawElementProvider
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

//  if(auto doc = dynamic_cast<Document*>(sel.box)) {
//    if(sel.start == 0 && sel.end == sel.box->length())
//      if(auto wid = dynamic_cast<Win32Widget*>(doc->native()))
//        return UiaHostProviderFromHwnd(wid->hwnd(), pRetVal); 
//  }
  
  *pRetVal = nullptr;
  return S_OK; 
}

//
// IRawElementProviderFragment::Navigate
//
STDMETHODIMP Win32UiaMultiBoxProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection own_sel = get_now();
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  switch(direction) {
    case NavigateDirection_Parent:
    case NavigateDirection_LastChild:
    case NavigateDirection_FirstChild:
    case NavigateDirection_PreviousSibling:
      return HRreport(NavigateImpl(own_sel, direction, pRetVal));
    
    case NavigateDirection_NextSibling:
      HR(NavigateImpl(own_sel, direction, pRetVal));
      if(*pRetVal) return S_OK;
      HR(Win32UiaBoxProvider::NavigatePopupsImpl(own_sel.box, NavigateDirection_FirstChild, pRetVal));
      return S_OK;
  }
  
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::NavigateImpl(const VolatileSelection &own_sel, enum NavigateDirection direction, IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[Win32UiaMultiBoxProvider::NavigateImpl(%p/%d:%d..%d, %d)]", own_sel.box, FrontEndReference::of(own_sel.box), own_sel.start, own_sel.end, direction);
  *pRetVal = nullptr;
  switch(direction) {
    case NavigateDirection_Parent: {
        auto par = own_sel.expanded();
        if(par != own_sel) {
          *pRetVal = create_multi_or_outer_single(par);
          //fprintf(stderr, "[=> Parent = (%p/%d:%d..%d) = %p]\n", par.box, FrontEndReference::of(par.box), par.start, par.end, *pRetVal);
        }
        else { 
          //fprintf(stderr, "[=> Parent = none]\n");
        }
      } return S_OK;
    
    case NavigateDirection_NextSibling: {
        auto par = own_sel.expanded();
        if(own_sel.end < par.end) {
          VolatileSelection sib(own_sel.box, own_sel.end, own_sel.end + 1);
          sib.expand_up_to_sibling(own_sel);
          *pRetVal = create_multi_or_inner_single(sib);
          //fprintf(stderr, "[=> NextSibling = (%p/%d:%d..%d) = %p]\n", sib.box, FrontEndReference::of(sib.box), sib.start, sib.end, *pRetVal);
        }
        else { 
          //fprintf(stderr, "[=> NextSibling = none]\n");
        }
      } return S_OK;
    
    case NavigateDirection_PreviousSibling: {
        auto par = own_sel.expanded();
        if(own_sel.start > par.start) {
          VolatileSelection sib(own_sel.box, own_sel.start - 1, own_sel.start);
          sib.expand_up_to_sibling(own_sel);
          *pRetVal = create_multi_or_inner_single(sib);
          //fprintf(stderr, "[=> PreviousSibling = (%p/%d:%d..%d) = %p]\n", sib.box, FrontEndReference::of(sib.box), sib.start, sib.end, *pRetVal);
        }
        else { 
          //fprintf(stderr, "[=> PreviousSibling = none]\n");
        }
      } return S_OK;
    
    case NavigateDirection_FirstChild: {
        if(own_sel.length() > 1) {
          VolatileSelection inner(own_sel.box, own_sel.start, own_sel.start + 1);
          
          while(inner) {
            auto next = inner.expanded();
            if(!next || next.logically_contains(own_sel) || next == inner)
              break;
            
            inner = next;
          }
          
          *pRetVal = create_multi_or_inner_single(inner);
          //fprintf(stderr, "[=> FirstChild = (%p/%d:%d..%d) = %p]\n", inner.box, FrontEndReference::of(inner.box), inner.start, inner.end, *pRetVal);
        }
        else if(int count = own_sel.box->count()){
          //fprintf(stderr, "[=> FirstChild = %p]\n", own_sel.box->item(0));
          *pRetVal = Win32UiaBoxProvider::create(own_sel.box->item(0));
        }
        else { 
          //fprintf(stderr, "[=> FirstChild = none]\n");
        }
      } return S_OK;
      
    case NavigateDirection_LastChild: {
        if(own_sel.length() > 1) {
          VolatileSelection inner(own_sel.box, own_sel.end - 1, own_sel.end);
          
          while(inner) {
            auto next = inner.expanded();
            if(!next || next.logically_contains(own_sel) || next == inner)
              break;
            
            inner = next;
          }
          
          *pRetVal = create_multi_or_inner_single(inner);
          //fprintf(stderr, "[=> LastChild = (%p/%d:%d..%d) = %p]\n", inner.box, FrontEndReference::of(inner.box), inner.start, inner.end, *pRetVal);
        }
        else if(int count = own_sel.box->count()){
          *pRetVal = Win32UiaBoxProvider::create(own_sel.box->item(count - 1));
          //fprintf(stderr, "[=> LastChild = %p = %p]\n", own_sel.box->item(0), *pRetVal);
        }
        else { 
          //fprintf(stderr, "[=> LastChild = none]\n");
        }
      } return S_OK;
  }
  
  return HRreport(E_INVALIDARG);
}

//
// IRawElementProviderFragment::GetRuntimeId
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetRuntimeId(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
//  fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaMultiBoxProvider::GetRuntimeId()]", this, sel_ref.id, sel_ref.start, sel_ref.end);
  *pRetVal = nullptr;
  int rId[] = {
    UiaAppendRuntimeId, 
    -(int)(uint32_t)(uintptr_t)FrontEndReference::unsafe_cast_to_pointer(sel.box->id()),
    sel.start,
    sel.end };

//  int rId[2] = { 0, 0 };
//  // Must return *two* element array [UiaAppendRuntimeId, id] with some unique id representing "sel"
//  // Since/if this class is only used for SectionGroups, the beginning section identifies the group end, 
//  // so we could use that. Its id() also represents that section alone, so we modify it slightly by using "-id()"
//  if(auto slist = dynamic_cast<SectionList*>(sel.box)) {
//    if(sel.length() > 0 && sel.start < slist->count()) {
//      rId[0] = UiaAppendRuntimeId;
//      rId[1] = - (int)(uint32_t)(uintptr_t)FrontEndReference::unsafe_cast_to_pointer(slist->section(sel.start)->id());
//  
//      fprintf(stderr, "[= {%d, %d}]\n", rId[0], rId[1]);
//    }
//  }
//  
//  if(rId[0] == 0) {
//    fprintf(stderr, "[= error]\n");
//    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
//  }
  
  SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, sizeof(rId)/sizeof(rId[0]));
  if(!psa)
    return E_OUTOFMEMORY;
  
  for(LONG i = 0; i < sizeof(rId)/sizeof(rId[0]); ++i) {
    SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
  }
  
  *pRetVal = psa;
  return S_OK;
}

//
// IRawElementProviderFragment::get_BoundingRectangle
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_BoundingRectangle(struct UiaRect *pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return Impl(*this).get_BoundingRectangle(pRetVal);
}

//
// IRawElementProviderFragment::GetEmbeddedFragmentRoots
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = nullptr;
  return S_OK;
}

//
// IRawElementProviderFragment::SetFocus
//
STDMETHODIMP Win32UiaMultiBoxProvider::SetFocus(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return S_OK;
}

//
// IRawElementProviderFragment::get_FragmentRoot
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = sel.box->find_parent<Document>(/* selfincluding */true);
  *pRetVal = Win32UiaBoxProvider::create(doc);
    
  return S_OK;
}

//
// IRawElementProviderFragmentRoot::ElementProviderFromPoint
//
STDMETHODIMP Win32UiaMultiBoxProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  // Win32UiaMultiBoxProvider also only supports this method for the top-level Document
  *pRetVal = nullptr;
  return S_OK;
}

//
// IRawElementProviderFragmentRoot::GetFocus
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetFocus(IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  // Win32UiaMultiBoxProvider also only supports this method for the top-level Document
  *pRetVal = nullptr;
  return S_OK;
}

//
// ITextProvider::GetSelection
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetSelection(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection this_range = get_now();
  if(!this_range)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  
  Document *doc = this_range.box->find_parent<Document>(true);
  if(!doc)
    return S_OK;
  
  VolatileSelection sel = doc->selection_now();
  if(!this_range.logically_contains(sel))
    return S_OK;
  
  ComBase<ITextRangeProvider> range;
  range.attach(new Win32UiaTextRangeProvider(SelectionReference(sel)));
  HR(ComSafeArray::create_singleton(pRetVal, range));
  return S_OK;
}

//
// ITextProvider::GetVisibleRanges
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetVisibleRanges(SAFEARRAY **pRetVal) {
  // Win32UiaBoxProvider also only implements this for the top-level (Document).
  return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
}

//
// ITextProvider::RangeFromChild
//
STDMETHODIMP Win32UiaMultiBoxProvider::RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection this_range = get_now();
  if(!this_range)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::RangeFromChild(%p)]\n", this, obj_ref, childElement);
  
  *pRetVal = nullptr;
  VolatileSelection range = Win32UiaMultiBoxProvider::find_outer_range(childElement);
  if(!range)
    return S_OK;
  
  if(this_range.logically_contains(range)) {
    *pRetVal = new Win32UiaTextRangeProvider(SelectionReference(range));
    return S_OK;
  }
  
  return S_OK;
}

//
// ITextProvider::RangeFromPoint
//
STDMETHODIMP Win32UiaMultiBoxProvider::RangeFromPoint(struct UiaPoint point, ITextRangeProvider **pRetVal) {
  // Win32UiaBoxProvider also only implements this for the top-level (Document).
  return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
}

//
// ITextProvider::get_DocumentRange
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_DocumentRange(ITextRangeProvider **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(!sel_ref.get_all())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = new Win32UiaTextRangeProvider(sel_ref);
  return S_OK;
}

//
// ITextProvider::get_SupportedTextSelection
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_SupportedTextSelection(enum SupportedTextSelection *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = SupportedTextSelection_Single;
  return S_OK;
}

//
// ITextProvider2::RangeFromAnnotation
//
STDMETHODIMP Win32UiaMultiBoxProvider::RangeFromAnnotation(IRawElementProviderSimple *annotationElement, ITextRangeProvider **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  return HRreport(E_INVALIDARG);
}

//
// ITextProvider2::GetCaretRange
//
STDMETHODIMP Win32UiaMultiBoxProvider::GetCaretRange(BOOL *isActive, ITextRangeProvider **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection own_sel = get_now();
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  
  Document *doc = own_sel.box->find_parent<Document>(true);
  if(!doc || !doc->selection())
    return S_OK;
  
  VolatileSelection sel = doc->selection_now();
  if(!own_sel.logically_contains(sel))
    return S_OK;
  
  sel.start = sel.end;
  *pRetVal  = new Win32UiaTextRangeProvider(SelectionReference(sel));
  *isActive = doc->native()->is_focused_widget();
  
  return S_OK;
}

//
// IExpandCollapseProvider::Expand
//
STDMETHODIMP Win32UiaMultiBoxProvider::Expand(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection own_sel = get_now();
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  if(auto slist = dynamic_cast<SectionList*>(own_sel.box)) {
    if(own_sel.start >= slist->length())
      return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
    slist->set_open_close_group(own_sel.start, true);
    
    return S_OK;
  }
  
  return HRreport(E_NOTIMPL);
}

//
// IExpandCollapseProvider::Collapse
//
STDMETHODIMP Win32UiaMultiBoxProvider::Collapse(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection own_sel = get_now();
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  if(auto slist = dynamic_cast<SectionList*>(own_sel.box)) {
    if(own_sel.start >= slist->length())
      return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
    slist->set_open_close_group(own_sel.start, false);
    
    return S_OK;
  }
  
  return HRreport(E_NOTIMPL);
}

//
// IExpandCollapseProvider::get_ExpandCollapseState
//
STDMETHODIMP Win32UiaMultiBoxProvider::get_ExpandCollapseState(enum ExpandCollapseState *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection own_sel = get_now();
  if(!own_sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = ExpandCollapseState_LeafNode;
  if(auto slist = dynamic_cast<SectionList*>(own_sel.box)) {
    if(own_sel.start >= slist->length())
      return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
      
    const SectionGroupInfo &group = slist->group_info(own_sel.start);
    
    if(     group.close_rel <  0) *pRetVal = ExpandCollapseState_Expanded;
    else if(group.close_rel == 0) *pRetVal = ExpandCollapseState_Collapsed;
    else                          *pRetVal = ExpandCollapseState_PartiallyExpanded;
  }
  
  return S_OK;
}

VolatileSelection Win32UiaMultiBoxProvider::find_outer_range(IRawElementProviderSimple *obj) {
  ComBase<ComSideChannelBase> cppChild = ComSideChannelBase::from_iunk(obj);
  if(auto childObj = dynamic_cast<Win32UiaBoxProvider*>(cppChild.get())) {
    return VolatileSelection(childObj->get<Box>(), 0).expanded_to_parent();
  }
  if(auto childMulti = dynamic_cast<Win32UiaMultiBoxProvider*>(cppChild.get())) {
    return childMulti->get_now();
  }
  return VolatileSelection();
}

//} ... class Win32UiaMultiBoxProvider

//{ class Win32UiaMultiBoxProvider::Impl ...

inline Win32UiaMultiBoxProvider::Impl::Impl(Win32UiaMultiBoxProvider &self)
 : self(self) 
{
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_BoundingRectangle(struct UiaRect *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  pRetVal->left   = 0.0;
  pRetVal->top    = 0.0;
  pRetVal->width  = 0.0;
  pRetVal->height = 0.0;
  
  Document *doc = sel.box->find_parent<Document>(true);
  if(!doc)
    return S_OK; // default = empty rect = invisible
  
  Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid || !IsWindowVisible(wid->hwnd()))
    return S_OK; // default = empty rect = invisible
  
  RECT client;
  if(!GetClientRect(wid->hwnd(), &client))
    return S_OK; // default = empty rect = invisible
  
  MapWindowPoints(wid->hwnd(), nullptr, (LPPOINT)&client, 2);
  
  Array<RectangleF> rects;
  sel.add_rectangles(rects, SelectionDisplayFlags::BigCenterBlob, {0.0f, 0.0f});
  if(rects.length() > 0) {
    RectangleF rect = rects[0];
    for(int i = 1; i < rects.length(); ++i)
      rect = rect.union_hull(rects[i]);
      
    if(sel.box->visible_rect(rect)) {
      rect = wid->map_document_rect_to_native(rect);
      
      pRetVal->left   = client.left + rect.x;
      pRetVal->top    = client.top  + rect.y;
      pRetVal->width  = rect.width;
      pRetVal->height = rect.height;
    }
  }
  
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_BoundingRectangle(VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  pRetVal->vt = VT_EMPTY;
  
  UiaRect rect;
  HR(get_BoundingRectangle(&rect));
  
  if(rect.width == 0 && rect.height == 0)
    return S_OK; // default = empty rect = invisible
  
  return InitVariantFromDoubleArray(&rect.left, 4, pRetVal);
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_ClassName(VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(dynamic_cast<SectionList*>(sel.box)) {
    pRetVal->vt = VT_BSTR;
    pRetVal->bstrVal = SysAllocString(L"SectionGroup"); 
  }
  else if(dynamic_cast<MathSequence*>(sel.box)) {
    pRetVal->vt = VT_BSTR;
    pRetVal->bstrVal = SysAllocString(L"MathSequence part"); 
  }
  else if(dynamic_cast<TextSequence*>(sel.box)) {
    pRetVal->vt = VT_BSTR;
    pRetVal->bstrVal = SysAllocString(L"TextSequence part"); 
  }
  else if(Expr sym = sel.box->to_pmath_symbol()) {
    String sym_name = sym.to_string();
    sym_name += " part";
    pRetVal->vt = VT_BSTR;
    pRetVal->bstrVal = SysAllocStringLen((const wchar_t*)sym_name.buffer(), sym_name.length());  
  }
  else {
    pRetVal->vt = VT_EMPTY; // Fall back to default value.
  }
  
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_ControlType(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(dynamic_cast<AbstractSequence*>(sel.box)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_TextControlTypeId; // Text control type imples it supports text pattern.
  }
  else {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_GroupControlTypeId;
  }
  
  return S_OK;    
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_HasKeyboardFocus(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = VARIANT_FALSE;
  if(Document *doc = sel.box->find_parent<Document>(true)) {
    if(doc->native()->is_focused_widget() && sel.box->is_parent_of(doc->selection_box()))
      pRetVal->boolVal = VARIANT_TRUE;
  }
  
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_HelpText(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(auto tooltip = sel.box->find_parent<TooltipBox>(true)) {
    String text = Application::interrupt_wait(
                    Call(Symbol(richmath_FE_BoxesToText), tooltip->tooltip_boxes(), strings::PlainText),
                    Application::edit_interrupt_timeout).to_string();
    
    pRetVal->vt      = VT_BSTR;
    pRetVal->bstrVal = SysAllocStringLen((const wchar_t*)text.buffer(), text.length());
    return S_OK;
  }
  
  pRetVal->vt = VT_EMPTY;
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_IsEnabled(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = sel.box->enabled() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_IsKeyboardFocusable(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = sel.selectable() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

HRESULT Win32UiaMultiBoxProvider::Impl::get_Name(VARIANT *pRetVal) {
  VolatileSelection sel = self.get_now();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  SimpleTextGather name(1000);
#ifndef NDEBUG
  {
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "[%d .. %d]", sel.start, sel.end);
    name.append_text(tmp);
  }
#endif
  if(sel.length() > 0) {
    if(auto slist = dynamic_cast<SectionList*>(sel.box)) {
      name.append(slist->section(sel.start));
    }
    else
      name.append(sel);
  }
  
  pRetVal->vt = VT_BSTR;
  pRetVal->bstrVal = SysAllocStringLen((const wchar_t*)name.text.buffer(), name.text.length());
  return S_OK;
}

//} ... class Win32UiaMultiBoxProvider::Impl
