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
      
      HRESULT move_by_all(int count, int *num_moved);
      HRESULT move_by_character(int count, int *num_moved);
      HRESULT move_by_word(int count, int *num_moved);
      
      HRESULT get_IsHidden(VARIANT *pRetVal);
      HRESULT get_IsReadOnly(VARIANT *pRetVal);
    
    private:
      static VolatileLocation move_beyond_end(VolatileLocation loc, bool jumping);
      
    private:
      Win32UiaTextRangeProvider &self;
  };
}

//{ class Win32UiaTextRangeProvider ...

Win32UiaTextRangeProvider::Win32UiaTextRangeProvider(SelectionReference range) 
  : refcount(1),
    range(range)
{
  //fprintf(stderr, "[delete %p = new Win32UiaTextRangeProvider(%d, %d .. %d)]\n", this, range.id, range.start, range.end);
}

Win32UiaTextRangeProvider::~Win32UiaTextRangeProvider() {
  //fprintf(stderr, "[delete Win32UiaTextRangeProvider %p(%d, %d .. %d)]\n", this, range.id, range.start, range.end);
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaTextRangeProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IUnknown || iid == IID_ITextRangeProvider || iid == IID_ITextRangeProvider2) {
    AddRef();
    *ppvObject = static_cast<ITextRangeProvider2*>(this);
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
    return HRreport(E_INVALIDARG);
 
  //fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::Clone()]\n", this, range.id, range.start, range.end);

  *pRetVal = new Win32UiaTextRangeProvider(range);
  return S_OK;
}

//
// ITextRangeProvider::Compare
//
STDMETHODIMP Win32UiaTextRangeProvider::Compare(ITextRangeProvider *other, BOOL *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  //fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::Compare(%p)]\n", this, range.id, range.start, range.end, other);
  *pRetVal = range == Impl::get(other);
  return S_OK;
}

//
// ITextRangeProvider::CompareEndpoints
//
STDMETHODIMP Win32UiaTextRangeProvider::CompareEndpoints(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint, int *pRetVal) {
  if(!targetRange)
    return HRreport(E_INVALIDARG);
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  VolatileLocation thisLoc = range
    .start_end_reference(
      endpoint == TextPatternRangeEndpoint_Start ? LogicalDirection::Backward : LogicalDirection::Forward)
    .get_all();
  VolatileLocation targetLoc = Impl::get(targetRange)
    .start_end_reference(
      targetEndpoint == TextPatternRangeEndpoint_Start ? LogicalDirection::Backward : LogicalDirection::Forward)
    .get_all();
  
  if(!thisLoc || !targetLoc)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = document_order(thisLoc, targetLoc);
  return S_OK;
}

//
// ITextRangeProvider::ExpandToEnclosingUnit
//
STDMETHODIMP Win32UiaTextRangeProvider::ExpandToEnclosingUnit(enum TextUnit unit) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  //fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::ExpandToEnclosingUnit(%d)]\n", this, range.id, range.start, range.end, unit);
  switch(unit) {
    case TextUnit_Character: return Impl(*this).expand_to_character();
    case TextUnit_Format:    // not supported. Use next larger unit.
    case TextUnit_Word:      return Impl(*this).expand_to_word();
    case TextUnit_Line:      // not supported. Use next larger unit.
    case TextUnit_Paragraph: // not supported. Use next larger unit.
    case TextUnit_Page:      // not supported. Use next larger unit.
    case TextUnit_Document:  return Impl(*this).expand_to_all();
    default: break;
  }
  return HRreport(E_INVALIDARG);
}

//
// ITextRangeProvider::FindAttribute
//
STDMETHODIMP Win32UiaTextRangeProvider::FindAttribute(TEXTATTRIBUTEID attributeId, VARIANT val, BOOL backward, ITextRangeProvider **pRetVal) {
  return HRreport(E_NOTIMPL);
}

//
// ITextRangeProvider::FindText
//
STDMETHODIMP Win32UiaTextRangeProvider::FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider **pRetVal) {
  return HRreport(E_NOTIMPL);
}

//
// ITextRangeProvider::GetAttributeValue
//
STDMETHODIMP Win32UiaTextRangeProvider::GetAttributeValue(TEXTATTRIBUTEID attributeId, VARIANT *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
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
    return HRreport(E_INVALIDARG);
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Array<RectangleF> rects;
  int num_visible = 0;
    
  Document *doc = sel.box->find_parent<Document>(true);
  Win32Widget *wid = doc ? dynamic_cast<Win32Widget*>(doc->native()) : nullptr;
  if(wid && IsWindowVisible(wid->hwnd())) {
    POINT screen_pt = {0, 0};
    ClientToScreen(wid->hwnd(), &screen_pt);
    
    sel.add_rectangles(rects, SelectionDisplayFlags::Default, {0.0f, 0.0f});
    
    if(sel.length() == 0) { 
      // There will be a (single) degenerate rectangle for the caret position. 
      // Widen it to be visible.
      for(RectangleF &rect : rects) {
        rect.grow(0.75f, 0.75f);
      }
    }
    
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
    return HRreport(E_OUTOFMEMORY);
  
  int j = 0;
  for(int i = 0; i < rects.length(); ++i) {
    const RectangleF &rect = rects[i];
    
    if(!rect.is_empty()) {
      HR(ComSafeArray::put_double(*pRetVal, j++, rect.x));
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
    return HRreport(E_INVALIDARG);
  
  *pRetVal = Win32UiaBoxProvider::create(range.get());
  if(!*pRetVal)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return S_OK;
}

//
// ITextRangeProvider::GetText
//
STDMETHODIMP Win32UiaTextRangeProvider::GetText(int maxLength, BSTR *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
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
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  //fprintf(stderr, "[%p(%d:%d..%d)->Win32UiaTextRangeProvider::Move(%d, %d)]\n", this, range.id, range.start, range.end, unit, count);
  switch(unit) {
    case TextUnit_Character: return Impl(*this).move_by_character(count, pRetVal);
    case TextUnit_Format:    // not supported. Use next larger unit.
    case TextUnit_Word:      return Impl(*this).move_by_word(count, pRetVal);
    case TextUnit_Line:      // not supported. Use next larger unit.
    case TextUnit_Paragraph: // not supported. Use next larger unit.
    case TextUnit_Page:      // not supported. Use next larger unit.
    case TextUnit_Document:  return Impl(*this).move_by_all(count, pRetVal);
    default: break;
  }
  return HRreport(E_INVALIDARG);
}

//
// ITextRangeProvider::MoveEndpointByUnit
//
STDMETHODIMP Win32UiaTextRangeProvider::MoveEndpointByUnit(enum TextPatternRangeEndpoint endpoint, enum TextUnit unit, int count, int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  auto orig = range;
  VolatileSelection from = range.get_all();
  if(!from)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(endpoint == TextPatternRangeEndpoint_Start) {
    range.end  = range.start; // range = the endpoint to be moved
    from.start = from.end;    // from = the other endpoint
  }
  else {
    range.start = range.end;  // range = the endpoint to be moved
    from.end    = from.start; // from = the other endpoint
  }
  
  HRESULT hr = HRreport(Move(unit, count, pRetVal));
  VolatileSelection to = range.get_all();
  if(FAILED(hr) || !to) {
    range = orig;
    return hr;
  }
  
  from.expand_to_cover(to, /* restrict_from_exists */ false);
  range.set(from);
  return S_OK;
}

//
// ITextRangeProvider::MoveEndpointByRange
//
STDMETHODIMP Win32UiaTextRangeProvider::MoveEndpointByRange(enum TextPatternRangeEndpoint endpoint, ITextRangeProvider *targetRange, enum TextPatternRangeEndpoint targetEndpoint) {
  if(!targetRange)
    return HRreport(E_INVALIDARG);
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);

  VolatileSelection from = range.get_all();
  VolatileSelection to = Impl::get(targetRange).get_all();
  
  if(!from || !to)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(endpoint == TextPatternRangeEndpoint_Start)
    from.start = from.end; // from = the other endpoint, which should stay fixed (end)
  else
    from.end = from.start; // from = the other endpoint, which should stay fixed (start)
  
  if(targetEndpoint == TextPatternRangeEndpoint_Start)
    to.end = to.start;
  else
    to.start = to.end;
  
  from.expand_to_cover(to, /* restrict_from_exists */ false);
  range.set(from);
  return S_OK;
}

//
// ITextRangeProvider::Select
//
STDMETHODIMP Win32UiaTextRangeProvider::Select(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(!sel.selectable())
    return HRreport(UIA_E_ELEMENTNOTENABLED);
  
  Document *doc = sel.box->find_parent<Document>(true);
  if(!doc)
    return HRreport(UIA_E_NOTSUPPORTED);
  
  doc->select(sel);
  return S_OK;
}

//
// ITextRangeProvider::AddToSelection
//
STDMETHODIMP Win32UiaTextRangeProvider::AddToSelection(void) {
  return HRreport(UIA_E_INVALIDOPERATION);
}

//
// ITextRangeProvider::RemoveFromSelection
//
STDMETHODIMP Win32UiaTextRangeProvider::RemoveFromSelection(void) {
  return HRreport(UIA_E_INVALIDOPERATION);
}

//
// ITextRangeProvider::ScrollIntoView
//
STDMETHODIMP Win32UiaTextRangeProvider::ScrollIntoView(BOOL alignToTop) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Box *box = range.get();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return HRreport(UIA_E_NOTSUPPORTED);
  
  // TODO: respect 'alignToTop'
  doc->async_scroll_to(range);
  return S_OK;
}

//
// ITextRangeProvider::GetChildren
//
STDMETHODIMP Win32UiaTextRangeProvider::GetChildren(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
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
      return HRreport(E_OUTOFMEMORY);
    
    for(int i = 0; i < after - first; ++i) {
      ComBase<IRawElementProviderSimple> child;
      child.attach(Win32UiaBoxProvider::create(sel.box->item(after + i)));
      if(!child)
        return HRreport(E_OUTOFMEMORY);
      
      long index = i;
      HR(SafeArrayPutElement(*pRetVal, &index, child.get()));
    }
  }
  
  return S_OK;
}

//
// ITextRangeProvider2::ShowContextMenu
//
STDMETHODIMP Win32UiaTextRangeProvider::ShowContextMenu(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  VolatileSelection sel = range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Document *doc = sel.box->find_parent<Document>(true);
  if(!doc)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid || IsWindowVisible(wid->hwnd()))
    return HRreport(UIA_E_INVALIDOPERATION);
  
  wid->show_popup_menu(sel);
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
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  int len = box->length();
  self.range.start = 0;
  self.range.end   = box->length();
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::expand_to_character() {
  VolatileSelection sel = self.range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
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
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
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

HRESULT Win32UiaTextRangeProvider::Impl::move_by_all(int count, int *num_moved) {
  *num_moved = 0;
  Box *box = self.range.get();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  bool was_degenerate = self.range.length() == 0;
  if(count > 0) {
    *num_moved = 1;
    self.range.end   = box->length();
    self.range.start = was_degenerate ? self.range.end : 0;
  }
  else if(count < 0) {
    *num_moved = -1;
    self.range.start = 0;
    self.range.end   = was_degenerate ? 0 : box->length();
  }
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::move_by_character(int count, int *num_moved) {
  *num_moved = 0;
  VolatileSelection sel = self.range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  LogicalDirection dir;
  if(count < 0) {
    count = -count;
    dir = LogicalDirection::Backward;
  }
  else
    dir = LogicalDirection::Forward;
  
  bool keep_empty = sel.length() == 0;
  VolatileLocation loc = sel.start_only();
  VolatileLocation prev{};
  while(count-- > 0) {
    // FIXME: move_logical() will skip the non-selectable parts of a TemplateBox. But we probably want to reach them.
    VolatileLocation next = loc.move_logical(dir, false);
    if(!next || next == loc)
      break;
    
    prev = loc;
    loc = next;
    ++*num_moved;
  }
  
  if(dir == LogicalDirection::Forward) {
    // exit the end of a box before selecting its last character
    if(!keep_empty)
      loc = move_beyond_end(loc, false);
  }
  
  if(dir == LogicalDirection::Backward)
    *num_moved = -*num_moved;
  
  self.range.set(loc);
  if(keep_empty)
    return S_OK;
  
  HR(expand_to_character());
  
  if(dir == LogicalDirection::Forward) {
    if(self.range.get_all().start_only() == prev)
      --*num_moved;
  }
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::move_by_word(int count, int *num_moved) {
  *num_moved = 0;
  VolatileSelection sel = self.range.get_all();
  if(!sel)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  LogicalDirection dir;
  if(count < 0) {
    count = -count;
    dir = LogicalDirection::Backward;
  }
  else
    dir = LogicalDirection::Forward;
  
  bool keep_empty = sel.length() == 0;
  VolatileLocation loc = sel.start_only();
  VolatileLocation prev{};
  while(count-- > 0) {
    // FIXME: move_logical() will skip the non-selectable parts of a TemplateBox. But we probably want to reach them.
    VolatileLocation next_char = loc.move_logical(dir, false);
    if(!next_char || next_char == loc)
      break;
    
    if(next_char.box == loc.box) { // stayed within the same box
      VolatileLocation next = loc.move_logical(dir, true);
      if(!next) // should not happen
        next = next_char;
      
      prev = loc;
      loc = next;
      ++*num_moved;
    }
    else { // switched to inner or outer box
      prev = loc;
      loc = next_char;
      ++*num_moved;
    }
  }
  
  if(dir == LogicalDirection::Forward) {
    // exit the end of a box before selecting its last character
    if(!keep_empty)
      loc = move_beyond_end(loc, true);
  }
  
  if(dir == LogicalDirection::Backward)
    *num_moved = -*num_moved;
  
  self.range.set(loc);
  if(keep_empty)
    return S_OK;
  
  HR(expand_to_word());
  
  if(dir == LogicalDirection::Forward) {
    if(self.range.get_all().start_only() == prev)
      --*num_moved;
  }
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::get_IsHidden(VARIANT *pRetVal) {
  Box *box = self.range.get();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  pRetVal->vt = VT_BOOL;
  RectangleF rect = box->extents().to_rectangle();
  pRetVal->boolVal = box->visible_rect(rect, nullptr) ? VARIANT_FALSE : VARIANT_TRUE;
  return S_OK;
}

HRESULT Win32UiaTextRangeProvider::Impl::get_IsReadOnly(VARIANT *pRetVal) {
  Box *box = self.range.get();
  if(!box)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
    
  pRetVal->vt = VT_BOOL;
  pRetVal->boolVal = box->editable() ? VARIANT_FALSE : VARIANT_TRUE;
  return S_OK;
}

VolatileLocation Win32UiaTextRangeProvider::Impl::move_beyond_end(VolatileLocation loc, bool jumping) {
  if(loc && loc.index == loc.box->length()) {
    // exit while at end
    while(loc && loc.index == loc.box->length()) {
      VolatileLocation next = loc.move_logical(LogicalDirection::Forward, jumping);
      if(!next || loc == next)
        break;
      
      loc = next;
    }
    
//    // enter while before new box
//    while(loc) {
//      // FIXME: move_logical() will skip the non-selectable parts of a TemplateBox. But we probably want to reach them.
//      VolatileLocation next = loc.move_logical(LogicalDirection::Forward, false);
//      if(next.box == loc.box)
//        break;
//      
//      loc = next;
//    }
  }
  return loc;
}

//} ... class Win32UiaTextRangeProvider::Impl
