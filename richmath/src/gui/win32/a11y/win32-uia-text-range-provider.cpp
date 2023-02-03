#include <gui/win32/a11y/win32-uia-text-range-provider.h>

#include <eval/application.h>
#include <boxes/abstractsequence.h>

#include <gui/win32/a11y/win32-uia-box-provider.h>
#include <gui/win32/ole/com-safe-arrays.h>
#include <gui/win32/win32-widget.h>

#include <uiautomation.h>


using namespace richmath;

namespace richmath {
  class Win32UiaTextRangeProvider::Impl {
    public:
      Impl(Win32UiaTextRangeProvider &self);
      
      static SelectionReference get(ITextRangeProvider *obj);
      
      HRESULT expand_to_all();
      HRESULT expand_to_character();
      HRESULT expand_to_word();
      
      HRESULT get_IsHidden(VARIANT *pRetVal);
      HRESULT get_IsReadOnly(VARIANT *pRetVal);
      
    private:
      Win32UiaTextRangeProvider &self;
  };
}

//{ class Win32UiaTextRangeProvider ...

Win32UiaTextRangeProvider::Win32UiaTextRangeProvider(SelectionReference range) 
  : refcount(1),
    range(range)
{
  fprintf(stderr, "[delete %p = new Win32UiaTextRangeProvider(%d, %d .. %d)]\n", this, range.id, range.start, range.end);
}

Win32UiaTextRangeProvider::~Win32UiaTextRangeProvider() {
  fprintf(stderr, "[delete %p = new Win32UiaTextRangeProvider(%d, %d .. %d)]\n", this, range.id, range.start, range.end);
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaTextRangeProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IUnknown || iid == IID_ITextRangeProvider) {
    AddRef();
    *ppvObject = static_cast<ITextRangeProvider*>(this);
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaTextRangeProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaTextRangeProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// ITextRangeProvider::Clone
//
STDMETHODIMP Win32UiaTextRangeProvider::Clone(ITextRangeProvider **pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
 
  fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::Clone()]\n", this, range.id, range.start, range.end);

  *pRetVal = new Win32UiaTextRangeProvider(range);
  return S_OK;
}

//
// ITextRangeProvider::Compare
//
STDMETHODIMP Win32UiaTextRangeProvider::Compare(ITextRangeProvider *other, BOOL *pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::Compare(%p)]\n", this, range.id, range.start, range.end, other);
  *pRetVal = range == Impl::get(other);
  return S_OK;
}

//
// ITextRangeProvider::CompareEndpoints
//
STDMETHODIMP Win32UiaTextRangeProvider::CompareEndpoints(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint, int *pRetVal) {
  if(!targetRange)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  SelectionReference target = Impl::get(targetRange);
  
  if(range.id != target.id)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  int thisIndex   = (endpoint       == TextPatternRangeEndpoint_Start) ? range.start  : range.end;
  int targetIndex = (targetEndpoint == TextPatternRangeEndpoint_Start) ? target.start : target.end;
  
  *pRetVal = thisIndex - targetIndex;
  
  return S_OK;
}

//
// ITextRangeProvider::ExpandToEnclosingUnit
//
STDMETHODIMP Win32UiaTextRangeProvider::ExpandToEnclosingUnit(enum TextUnit unit) {
  if(!Application::is_running_on_gui_thread())
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
    
  fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::ExpandToEnclosingUnit(%d)]\n", this, range.id, range.start, range.end, unit);
  switch(unit) {
    case TextUnit_Character: return Impl(*this).expand_to_character();
    case TextUnit_Word:      return Impl(*this).expand_to_word();
    case TextUnit_Document:  return Impl(*this).expand_to_all();
    default: break;
  }
  return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::FindAttribute
//
STDMETHODIMP Win32UiaTextRangeProvider::FindAttribute(TEXTATTRIBUTEID attributeId, VARIANT val, BOOL backward, ITextRangeProvider **pRetVal) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::FindText
//
STDMETHODIMP Win32UiaTextRangeProvider::FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider **pRetVal) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::GetAttributeValue
//
STDMETHODIMP Win32UiaTextRangeProvider::GetAttributeValue(TEXTATTRIBUTEID attributeId, VARIANT *pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  if(!Application::is_running_on_gui_thread())
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  pRetVal->vt = VT_EMPTY;
  
  switch(attributeId) {
    case UIA_IsHiddenAttributeId:   return Impl(*this).get_IsHidden(pRetVal);
    case UIA_IsReadOnlyAttributeId: return Impl(*this).get_IsReadOnly(pRetVal);
  }
  
  return S_OK;
}

//
// ITextRangeProvider::GetBoundingRectangles
//
STDMETHODIMP Win32UiaTextRangeProvider::GetBoundingRectangles(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  if(!Application::is_running_on_gui_thread())
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  Array<RectangleF> rects;
  int num_visible = 0;
    
  Document *doc = sel.box->find_parent<Document>(true);
  Win32Widget *wid = doc ? dynamic_cast<Win32Widget*>(doc->native()) : nullptr;
  if(wid && IsWindowVisible(wid->hwnd())) {
    POINT screen_pt = {0, 0};
    ClientToScreen(wid->hwnd(), &screen_pt);
    
//    { // TODO
//      RectangleF rect = sel.box->extents().to_rectangle();
//      if(sel.box->visible_rect(rect)) {
//        rects.add(rect);
//      }
//    }
    sel.add_rectangles(rects, SelectionDisplayFlags::Default, {0.0f, 0.0f});
    
    for(RectangleF &rect : rects) {
      if(sel.box->visible_rect(rect) && !rect.is_empty()) {
        ++num_visible;
        rect = wid->map_document_rect_to_native(rect);
        rect.x+= screen_pt.x;
        rect.y+= screen_pt.y;
      }
      else {
        rect.width = 0;
        rect.height = 0;
      }
    }
  }
  
  *pRetVal = SafeArrayCreateVector(VT_R8, 0, 4 * num_visible);
  if(!*pRetVal)
    return check_HRESULT(E_OUTOFMEMORY, __func__, __FILE__, __LINE__);
  
  int j = 0;
  for(int i = 0; i < rects.length(); ++i) {
    const RectangleF &rect = rects[i];
    
    if(!rect.is_empty()) {
      HR(ComSafeArray::put_double(*pRetVal, j++,     rect.x));
      HR(ComSafeArray::put_double(*pRetVal, j++, rect.y));
      HR(ComSafeArray::put_double(*pRetVal, j++, rect.width));
      HR(ComSafeArray::put_double(*pRetVal, j++, rect.height));
    }
  }
  
  return S_OK;
}

//
// ITextRangeProvider::GetEnclosingElement
//
STDMETHODIMP Win32UiaTextRangeProvider::GetEnclosingElement(IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  *pRetVal = new Win32UiaBoxProvider(range.id);
  if(!*pRetVal)
    return check_HRESULT(E_OUTOFMEMORY, __func__, __FILE__, __LINE__);
  
  return S_OK;
}

//
// ITextRangeProvider::GetText
//
STDMETHODIMP Win32UiaTextRangeProvider::GetText(int maxLength, BSTR *pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  if(!Application::is_running_on_gui_thread())
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  *pRetVal = nullptr;
  if(auto seq = FrontEndObject::find_cast<AbstractSequence>(range.id)) {
    String str = seq->text().part(range.start, range.end - range.start);
    if(str.length() > maxLength)
      str = str.part(0, maxLength);
    
    int len = str.length();
    BSTR bstr = SysAllocStringLen((const wchar_t*)str.buffer(), len);
    for(int i = 0; i < len; ++i) {
      if(bstr[i] == PMATH_CHAR_BOX)
        bstr[i] = 0xFFFC;
    }
    
    *pRetVal = bstr;
  }
  else if(range.start < range.end){
    int len = range.end - range.start;
    if(len > maxLength)
      len = maxLength;
    
    Array<OLECHAR> content;
    content.length(len, 0xFFFC);
    *pRetVal = SysAllocStringLen(content.items(), content.length());
  }
  
  return S_OK;
}

//
// ITextRangeProvider::Move
//
STDMETHODIMP Win32UiaTextRangeProvider::Move(enum TextUnit unit, int count, int *pRetVal) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::MoveEndpointByUnit
//
STDMETHODIMP Win32UiaTextRangeProvider::MoveEndpointByUnit( enum TextPatternRangeEndpoint endpoint, enum TextUnit unit, int count, int *pRetVal) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::MoveEndpointByRange
//
STDMETHODIMP Win32UiaTextRangeProvider::MoveEndpointByRange(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::Select
//
STDMETHODIMP Win32UiaTextRangeProvider::Select(void) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::AddToSelection
//
STDMETHODIMP Win32UiaTextRangeProvider::AddToSelection(void) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::RemoveFromSelection
//
STDMETHODIMP Win32UiaTextRangeProvider::RemoveFromSelection(void) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::ScrollIntoView
//
STDMETHODIMP Win32UiaTextRangeProvider::ScrollIntoView(BOOL alignToTop) {
  return check_HRESULT(E_NOTIMPL, __func__, __FILE__, __LINE__);
}

//
// ITextRangeProvider::GetChildren
//
STDMETHODIMP Win32UiaTextRangeProvider::GetChildren(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return check_HRESULT(E_INVALIDARG, __func__, __FILE__, __LINE__);
  
  if(!Application::is_running_on_gui_thread())
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  *pRetVal = nullptr;
  
  int max_count = sel.box->count();
  int first;
  for(first = 0; first < max_count; ++first) {
    if(sel.start <= sel.box->item(first)->index())
      break;
  }
  
  int after;
  for(after = first; after < max_count; ++after) {
    if(sel.end <= sel.box->item(after)->index())
      break;
  }
  
  if(first < after) {
    *pRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, after - first);
    if(!*pRetVal)
      return check_HRESULT(E_OUTOFMEMORY, __func__, __FILE__, __LINE__);
    
    for(int i = 0; i < after - first; ++i) {
      ComBase<IRawElementProviderSimple> child;
      child.attach(new Win32UiaBoxProvider(sel.box->item(after + i)->id()));
      if(!child)
        return check_HRESULT(E_OUTOFMEMORY, __func__, __FILE__, __LINE__);
      
      long index = i;
      HR(SafeArrayPutElement(*pRetVal, &index, child.get()));
    }
  }
  
  return S_OK;
}
  
//} ... class Win32UiaTextRangeProvider

//{ class Win32UiaTextRangeProvider::Impl ...

inline Win32UiaTextRangeProvider::Impl::Impl(Win32UiaTextRangeProvider &self)
  : self(self)
{
}

SelectionReference Win32UiaTextRangeProvider::Impl::get(ITextRangeProvider *obj) {
  ComBase<ComSideChannelBase> cppBase = ComSideChannelBase::from_iunk(obj);
  if(auto cppObj = dynamic_cast<Win32UiaTextRangeProvider*>(cppBase.get())) {
    return cppObj->range;
  }
  return SelectionReference();
}

HRESULT Win32UiaTextRangeProvider::Impl::expand_to_all() {
  Box *box = self.range.get();
  if(!box)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  int len = box->length();
  self.range.start = 0;
  self.range.end   = box->length();
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::expand_to_character() {
  VolatileSelection sel = self.range.get_all();
  if(!sel)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  int len = sel.box->length();
  if(len == 0) {
    sel.start = sel.end = 0;
    return S_OK;
  }
  sel.end = sel.start + 1;
  if(sel.end > len) {
    sel.end = len;
    sel.start = len - 1;
  }
  self.range.set(sel);
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::expand_to_word() {
  VolatileSelection sel = self.range.get_all();
  if(!sel)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
  
  VolatileLocation next = sel.start_only().move_logical(LogicalDirection::Forward, true);
  if(next.box == sel.box) {
    self.range.end = next.index;
    
    VolatileLocation prev = next.move_logical(LogicalDirection::Backward, true);
    
    if(prev.box == sel.box) 
      self.range.start = prev.index;
   
    return S_OK;
  }
  
  next = sel.start_only().move_logical(LogicalDirection::Backward, true);
  if(next.box == sel.box) {
    self.range.end   = sel.start;
    self.range.start = next.index;
    return S_OK;
  }
  
  self.range.start = self.range.end = 0;
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::get_IsHidden(VARIANT *pRetVal) {
  Box *box = self.range.get();
  if(!box)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
    
  pRetVal->vt = VT_BOOL;
  RectangleF rect = box->extents().to_rectangle();
  pRetVal->boolVal = box->visible_rect(rect, nullptr) ? VARIANT_FALSE : VARIANT_TRUE;
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::get_IsReadOnly(VARIANT *pRetVal) {
  Box *box = self.range.get();
  if(!box)
    return check_HRESULT(UIA_E_ELEMENTNOTAVAILABLE, __func__, __FILE__, __LINE__);
    
  pRetVal->vt = VT_BOOL;
  pRetVal->boolVal = box->editable() ? VARIANT_FALSE : VARIANT_TRUE;
  return S_OK;
}

//} ... class Win32UiaTextRangeProvider::Impl
