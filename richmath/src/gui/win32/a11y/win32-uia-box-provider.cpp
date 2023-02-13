#include <initguid.h>

#include <gui/win32/a11y/win32-uia-box-provider.h>

#include <eval/application.h>
#include <boxes/abstractsequence.h>
#include <boxes/buttonbox.h>
#include <boxes/checkboxbox.h>
#include <boxes/graphics/graphicsbox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/openerbox.h>
#include <boxes/radiobuttonbox.h>
#include <util/text-gathering.h>

#include <gui/win32/ole/com-safe-arrays.h>
#include <gui/win32/a11y/win32-uia-grid-provider.h>
#include <gui/win32/a11y/win32-uia-grid-item-provider.h>
#include <gui/win32/a11y/win32-uia-invoke-provider.h>
#include <gui/win32/a11y/win32-uia-selection-item-provider.h>
#include <gui/win32/a11y/win32-uia-text-range-provider.h>
#include <gui/win32/a11y/win32-uia-toggle-provider.h>
#include <gui/win32/win32-widget.h>

#include <propvarutil.h>
#include <uiautomation.h>


using namespace richmath;

namespace richmath {
  class Win32UiaBoxProvider::Impl {
    public:
      Impl(Win32UiaBoxProvider &self);
      
      HRESULT get_BoundingRectangle(struct UiaRect *pRetVal);
      HRESULT get_BoundingRectangle(VARIANT *pRetVal);
      HRESULT get_ClassName(VARIANT *pRetVal);
      HRESULT get_ControlType(VARIANT *pRetVal);
      HRESULT get_HasKeyboardFocus(VARIANT *pRetVal);
      HRESULT get_IsEnabled(VARIANT *pRetVal);
      HRESULT get_IsKeyboardFocusable(VARIANT *pRetVal);
      HRESULT get_Name(VARIANT *pRetVal);
      
    private:
      Win32UiaBoxProvider &self;
  };
}

//{ class Win32UiaBoxProvider ...

Win32UiaBoxProvider::Win32UiaBoxProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
  //fprintf(stderr, "[%p = new Win32UiaBoxProvider(%d)]\n", this, obj_ref);
}

Win32UiaBoxProvider::~Win32UiaBoxProvider() {
  //fprintf(stderr, "[delete Win32UiaBoxProvider %p(%d)]\n", this, obj_ref);
}

Win32UiaBoxProvider *Win32UiaBoxProvider::create(Box *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaBoxProvider(box->id());
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaBoxProvider::QueryInterface(REFIID iid, void **ppvObject) {
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
STDMETHODIMP_(ULONG) Win32UiaBoxProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaBoxProvider::Release(void) {
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
STDMETHODIMP Win32UiaBoxProvider::get_ProviderOptions(enum ProviderOptions *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!obj_ref)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = ProviderOptions(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
  return S_OK;
}

//
//  IRawElementProviderSimple::GetPatternProvider
//
STDMETHODIMP Win32UiaBoxProvider::GetPatternProvider(PATTERNID patternId, IUnknown **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  switch(patternId) {
    case UIA_TextPatternId:
    case UIA_TextPattern2Id: {
      FrontEndObject *obj = get_object();
      if(auto owner = dynamic_cast<OwnerBox*>(obj)) {
        *pRetVal = static_cast<ITextProvider2 *>(Win32UiaBoxProvider::create(owner->content()));
      }
//      else if(auto sect = dynamic_cast<AbstractSequenceSection*>(obj)) {
//        *pRetVal = static_cast<ITextProvider2 *>(Win32UiaBoxProvider::create(sect->content()));
//      }
      else /*if(dynamic_cast<AbstractSequence*>(obj) || dynamic_cast<Document*>(obj))*/ {
        *pRetVal = static_cast<ITextProvider2 *>(this);
        (*pRetVal)->AddRef();
      }
    } return S_OK;
    
    case UIA_GridPatternId:          *pRetVal = static_cast<IGridProvider*>(         Win32UiaGridProvider::create(get<GridBox>())); return S_OK;
    case UIA_TablePatternId:         *pRetVal = static_cast<ITableProvider*>(        Win32UiaGridProvider::create(get<GridBox>())); return S_OK;
    
    case UIA_GridItemPatternId:      *pRetVal = static_cast<IGridItemProvider*>(     Win32UiaGridItemProvider::create(get<GridItem>())); return S_OK;
    case UIA_TableItemPatternId:     *pRetVal = static_cast<ITableItemProvider*>(    Win32UiaGridItemProvider::create(get<GridItem>())); return S_OK;
  
    case UIA_InvokePatternId:        *pRetVal = static_cast<IInvokeProvider*>(       Win32UiaInvokeProvider::try_create(       get_object())); return S_OK;
    case UIA_SelectionItemPatternId: *pRetVal = static_cast<ISelectionItemProvider*>(Win32UiaSelectionItemProvider::try_create(get_object())); return S_OK;
    case UIA_TogglePatternId:        *pRetVal = static_cast<IToggleProvider*>(       Win32UiaToggleProvider::try_create(       get_object())); return S_OK;
  }
  
  return S_OK;
}

//
//  IRawElementProviderSimple::GetPropertyValue
//
STDMETHODIMP Win32UiaBoxProvider::GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) {
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
STDMETHODIMP Win32UiaBoxProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::get_HostRawElementProvider()]\n", this, obj_ref);
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  if(auto doc = dynamic_cast<Document*>(box)) {
    if(auto wid = dynamic_cast<Win32Widget*>(doc->native()))
      return UiaHostProviderFromHwnd(wid->hwnd(), pRetVal); 
  }
  
  *pRetVal = nullptr;
  return S_OK; 
}

//
// IRawElementProviderFragment::Navigate
//
STDMETHODIMP Win32UiaBoxProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::Navigate(%d)]\n", this, obj_ref, direction);
  *pRetVal = nullptr;
  switch(direction) {
    case NavigateDirection_Parent: {
        if(Box *parent = box->parent()) 
          *pRetVal = Win32UiaBoxProvider::create(parent);
      } return S_OK;
    
    case NavigateDirection_NextSibling: {
        if(Box *parent = box->parent()) {
          if(auto parseq = dynamic_cast<AbstractSequence*>(parent)) {
            int count = parent->count();
            for(int i = 0; i < count; ++i) {
              Box *item = parent->item(i);
              if(item == box) {
                if(i + 1 < count)
                  *pRetVal = Win32UiaBoxProvider::create(parent->item(i + 1));
                return S_OK;
              }
            }
          }
          else {
            int i = box->index();
            if(i + 1 < parent->count())
              *pRetVal = Win32UiaBoxProvider::create(parent->item(i + 1));
          }
        }
      } return S_OK;
    
    case NavigateDirection_PreviousSibling: {
        if(Box *parent = box->parent()) {
          if(auto parseq = dynamic_cast<AbstractSequence*>(parent)) {
            int count = parent->count();
            for(int i = 0; i < count; ++i) {
              Box *item = parent->item(i);
              if(item == box) {
                if(i > 0)
                  *pRetVal = Win32UiaBoxProvider::create(parent->item(i - 1));
                return S_OK;
              }
            }
          }
          else {
            int i = box->index();
            if(i > 0)
              *pRetVal = Win32UiaBoxProvider::create(parent->item(i - 1));
          }
        }
      } return S_OK;
    
    case NavigateDirection_FirstChild: {
        if(int count = box->count())
          *pRetVal = Win32UiaBoxProvider::create(box->item(0));
      } return S_OK;
      
    case NavigateDirection_LastChild: {
        if(int count = box->count())
          *pRetVal = Win32UiaBoxProvider::create(box->item(count - 1));
      } return S_OK;
  }
  
  return HRreport(E_INVALIDARG);
}

//
// IRawElementProviderFragment::GetRuntimeId
//
STDMETHODIMP Win32UiaBoxProvider::GetRuntimeId(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::GetRuntimeId()]\n", this, obj_ref);
  *pRetVal = nullptr;
  if(box->parent()) {
    int rId[] = { UiaAppendRuntimeId, (int)(uint32_t)(uintptr_t)FrontEndReference::unsafe_cast_to_pointer(obj_ref) };
    SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, 2);
    if(!psa)
      return E_OUTOFMEMORY;
    
    for(long i = 0; i < 2; ++i) {
      SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
    }
    
    *pRetVal = psa;
  }
  
  return S_OK;
}

//
// IRawElementProviderFragment::get_BoundingRectangle
//
STDMETHODIMP Win32UiaBoxProvider::get_BoundingRectangle(struct UiaRect *pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return Impl(*this).get_BoundingRectangle(pRetVal);
}

//
// IRawElementProviderFragment::GetEmbeddedFragmentRoots
//
STDMETHODIMP Win32UiaBoxProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = nullptr;
  return S_OK;
}

//
// IRawElementProviderFragment::SetFocus
//
STDMETHODIMP Win32UiaBoxProvider::SetFocus(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return S_OK;
}

//
// IRawElementProviderFragment::get_FragmentRoot
//
STDMETHODIMP Win32UiaBoxProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::get_FragmentRoot()]\n", this, obj_ref);
  Document *doc = box->find_parent<Document>(/* selfincluding */true);
  if(!doc || doc == box) {
    AddRef();
    *pRetVal = this;
  }
  else
    *pRetVal = Win32UiaBoxProvider::create(doc);
    
  return S_OK;
}

//
// IRawElementProviderFragmentRoot::ElementProviderFromPoint
//
STDMETHODIMP Win32UiaBoxProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = get<Document>();
  if(!doc)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  
  Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return S_OK;
  
  POINT screen_pt = {0, 0};
  ClientToScreen(wid->hwnd(), &screen_pt);
  
  Point pt = wid->map_native_point_to_document(Point(x - screen_pt.x, y - screen_pt.y));
  
  bool was_inside_start;
  VolatileSelection sel = doc->mouse_selection(pt, &was_inside_start);
  Box *receiver = sel.box ? sel.box->mouse_sensitive() : sel.box;
  if(receiver == doc)
    receiver = sel.box;
  
  if(receiver == doc) {
    AddRef();
    *pRetVal = this;
  }
  else if(receiver) 
    *pRetVal = Win32UiaBoxProvider::create(receiver);
  
  return S_OK;
}

//
// IRawElementProviderFragmentRoot::GetFocus
//
STDMETHODIMP Win32UiaBoxProvider::GetFocus(IRawElementProviderFragment **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = get<Document>();
  if(!doc)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  if(Box *sel_box = doc->selection_box()) {
    if(sel_box == doc) {
      AddRef();
      *pRetVal = this;
    }
    else
      *pRetVal = Win32UiaBoxProvider::create(sel_box);
  }
  
  return S_OK;
}

//
// ITextProvider::GetSelection
//
STDMETHODIMP Win32UiaBoxProvider::GetSelection(SAFEARRAY **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::GetSelection()]\n", this, obj_ref);
  
  *pRetVal = nullptr;
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return S_OK;
  
  VolatileSelection sel = doc->selection_now();
  if(!box->is_parent_of(sel.box))
    return S_OK;
  
  ComBase<ITextRangeProvider> range;
  range.attach(new Win32UiaTextRangeProvider(SelectionReference(sel)));
  HR(ComSafeArray::create_singleton(pRetVal, range));
  return S_OK;
}

//
// ITextProvider::GetVisibleRanges
//
STDMETHODIMP Win32UiaBoxProvider::GetVisibleRanges(SAFEARRAY **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Document>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::GetVisibleRanges()]\n", this, obj_ref);
  // FIXME: collect all continuous ranges that are visible
  ComBase<ITextRangeProvider> range;
  range.attach(new Win32UiaTextRangeProvider(SelectionReference(box, 0, box->length())));
  
  HR(ComSafeArray::create_singleton(pRetVal, range));
  
  return S_OK;
}

//
// ITextProvider::RangeFromChild
//
STDMETHODIMP Win32UiaBoxProvider::RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *this_box = get<Box>();
  if(!this_box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::RangeFromChild(%p)]\n", this, obj_ref, childElement);
  
  *pRetVal = nullptr;
  FrontEndObject *child_feo = find(childElement);
  if(!child_feo)
    return S_OK;
  
  if(auto *child_box = dynamic_cast<Box*>(child_feo)) {
    VolatileSelection range(child_box, 0);
    range.expand_to_parent();
    if(this_box->is_parent_of(range.box)) {
      *pRetVal = new Win32UiaTextRangeProvider(SelectionReference(range));
      return S_OK;
    }
  }
  
  return S_OK;
}

//
// ITextProvider::RangeFromPoint
//
STDMETHODIMP Win32UiaBoxProvider::RangeFromPoint(struct UiaPoint point, ITextRangeProvider **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = get<Document>();
  if(!doc)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  if(!IsWindowVisible(wid->hwnd()))
    return S_OK; // default = empty rect = invisible
  
  POINT origin = {0, 0};
  ClientToScreen(wid->hwnd(), &origin);
  
  Point doc_pt = wid->map_native_point_to_document(Point(point.x - origin.x, point.y - origin.y));
  bool was_inside_start;
  VolatileSelection sel = doc->mouse_selection(doc_pt, &was_inside_start);
  
  *pRetVal = new Win32UiaTextRangeProvider(SelectionReference(sel));
  
  return S_OK;
}

//
// ITextProvider::get_DocumentRange
//
STDMETHODIMP Win32UiaBoxProvider::get_DocumentRange(ITextRangeProvider **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  //fprintf(stderr, "[%p(%d)->Win32UiaBoxProvider::get_DocumentRange()]\n", this, obj_ref);
  *pRetVal = new Win32UiaTextRangeProvider(SelectionReference(box, 0, box->length()));
  return S_OK;
}

//
// ITextProvider::get_SupportedTextSelection
//
STDMETHODIMP Win32UiaBoxProvider::get_SupportedTextSelection(enum SupportedTextSelection *pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = SupportedTextSelection_Single;
  return S_OK;
}

//
// ITextProvider2::RangeFromAnnotation
//
STDMETHODIMP Win32UiaBoxProvider::RangeFromAnnotation(IRawElementProviderSimple *annotationElement, ITextRangeProvider **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  return HRreport(E_INVALIDARG);
}

//
// ITextProvider2::GetCaretRange
//
STDMETHODIMP Win32UiaBoxProvider::GetCaretRange(BOOL *isActive, ITextRangeProvider **pRetVal) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = nullptr;
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc || !doc->selection())
    return S_OK;
  
  VolatileSelection sel = doc->selection_now();
  if(!box->is_parent_of(sel.box))
    return S_OK;
  
  sel.start = sel.end;
  *pRetVal  = new Win32UiaTextRangeProvider(SelectionReference(sel));
  *isActive = doc->native()->is_focused_widget();
  
  return S_OK;
}

FrontEndObject *Win32UiaBoxProvider::get_object() {
  return FrontEndObject::find(obj_ref);
}

FrontEndObject *Win32UiaBoxProvider::find(IRawElementProviderSimple *obj) {
  ComBase<ComSideChannelBase> cppChild = ComSideChannelBase::from_iunk(obj);
  if(auto childObj = dynamic_cast<Win32UiaBoxProvider*>(cppChild.get())) {
    return FrontEndObject::find(childObj->obj_ref);
  }
  return nullptr;
}

//} ... class Win32UiaBoxProvider

//{ class Win32UiaBoxProvider::Impl ...

inline Win32UiaBoxProvider::Impl::Impl(Win32UiaBoxProvider &self)
 : self(self) 
{
}

HRESULT Win32UiaBoxProvider::Impl::get_BoundingRectangle(struct UiaRect *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  pRetVal->left   = 0.0;
  pRetVal->top    = 0.0;
  pRetVal->width  = 0.0;
  pRetVal->height = 0.0;
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return S_OK; // default = empty rect = invisible
  
  Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid || !IsWindowVisible(wid->hwnd()))
    return S_OK; // default = empty rect = invisible
  
  RECT client;
  if(!GetClientRect(wid->hwnd(), &client))
    return S_OK; // default = empty rect = invisible
  
  MapWindowPoints(wid->hwnd(), nullptr, (LPPOINT)&client, 2);
  
  RectangleF rect = RectangleF(0, 0, 0, 0);
  if(box->extents().is_empty()) { 
    // For inline-spans, box->extents().to_rectangle() would be empty
    //  box->as_inline_span() || box->as_inline_text_span() ...
    Array<RectangleF> rects;
    box->selection_rectangles(rects, SelectionDisplayFlags::BigCenterBlob, {0.0f, 0.0f}, 0, box->length());
    if(rects.length() > 0) {
      rect = rects[0];
      for(int i = 1; i < rects.length(); ++i)
        rect = rect.union_hull(rects[i]);
    }
  }
  else {
    rect = box->extents().to_rectangle();
  }
  
  if(box->visible_rect(rect)) {
    rect = wid->map_document_rect_to_native(rect);
    
    pRetVal->left   = client.left + rect.x;
    pRetVal->top    = client.top  + rect.y;
    pRetVal->width  = rect.width;
    pRetVal->height = rect.height;
  }
  
  return S_OK;
}

HRESULT Win32UiaBoxProvider::Impl::get_BoundingRectangle(VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  UiaRect rect;
  HR(get_BoundingRectangle(&rect));
  
  pRetVal->vt = VT_EMPTY;
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(rect.width == 0 && rect.height == 0)
    return S_OK; // default = empty rect = invisible
  
  return InitVariantFromDoubleArray(&rect.left, 4, pRetVal);
}

HRESULT Win32UiaBoxProvider::Impl::get_ClassName(VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
    
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(Expr sym = box->to_pmath_symbol()) {
    String sym_name = sym.to_string();
    pRetVal->vt = VT_BSTR;
    pRetVal->bstrVal = SysAllocStringLen((const wchar_t*)sym_name.buffer(), sym_name.length());  
  }
  else
    pRetVal->vt = VT_EMPTY; // Fall back to default value.
  
  return S_OK;
}

HRESULT Win32UiaBoxProvider::Impl::get_ControlType(VARIANT *pRetVal) {
  FrontEndObject *obj = self.get_object();
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(dynamic_cast<Document*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_DocumentControlTypeId; // Document control type imples it supports text pattern.
  }
  else if(dynamic_cast<AbstractSequence*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_TextControlTypeId; // Text control type imples it supports text pattern.
  }
  else if(dynamic_cast<GridBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_TableControlTypeId;
  }
  else if(dynamic_cast<GridItem*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_DataItemControlTypeId;
  }
  else if(dynamic_cast<GraphicsBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_ImageControlTypeId;
  }
  else if(dynamic_cast<InputFieldBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_EditControlTypeId;
  }
  else if(dynamic_cast<AbstractButtonBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_ButtonControlTypeId;
  }
  else if(dynamic_cast<CheckboxBox*>(obj) || dynamic_cast<OpenerBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_CheckBoxControlTypeId;
  }
  else if(dynamic_cast<RadioButtonBox*>(obj)) {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_RadioButtonControlTypeId;
  }
  else {
    pRetVal->vt   = VT_I4;
    pRetVal->lVal = UIA_GroupControlTypeId;
  }
  
  return S_OK;    
}

HRESULT Win32UiaBoxProvider::Impl::get_HasKeyboardFocus(VARIANT *pRetVal) {
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = VARIANT_FALSE;
  if(Document *doc = box->find_parent<Document>(true)) {
    if(doc->native()->is_focused_widget() && box->is_parent_of(doc->selection_box()))
      pRetVal->boolVal = VARIANT_TRUE;
  }
  
  return S_OK;
}

HRESULT Win32UiaBoxProvider::Impl::get_IsEnabled(VARIANT *pRetVal) {
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = box->enabled() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

HRESULT Win32UiaBoxProvider::Impl::get_IsKeyboardFocusable(VARIANT *pRetVal) {
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  pRetVal->vt      = VT_BOOL;
  pRetVal->boolVal = box->selectable() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

HRESULT Win32UiaBoxProvider::Impl::get_Name(VARIANT *pRetVal) {
  Box *box = self.get<Box>();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  SimpleTextGather name(1000);
  if(auto doc = dynamic_cast<Document*>(box))
    name.append_text(doc->native()->window_title());
  else if(!dynamic_cast<InputFieldBox*>(box))
    name.append(box);
  
  pRetVal->vt = VT_BSTR;
  pRetVal->bstrVal = SysAllocStringLen((const wchar_t*)name.text.buffer(), name.text.length());
  return S_OK;
}

//} ... class Win32UiaBoxProvider::Impl
