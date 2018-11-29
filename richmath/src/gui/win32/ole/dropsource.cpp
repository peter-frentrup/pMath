#define OEMRESOURCE

#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>

#include <shobjidl.h>
#include <shlguid.h>


using namespace richmath;

#define DDWM_UPDATEWINDOW_2  (WM_USER + 2)
enum class DropDescriptionDefault {
  Unknown = 0, // should use "DropDescription" HGLOBAL object
  Stop = 1,
  Move = 2,
  Copy = 3,
  Link = 4
};

//{ class DropSource ...

DropSource::DropSource()
  : refcount{1},
    must_set_cursor{true}
{
  CoCreateInstance(
    CLSID_DragDropHelper,
    nullptr,
    CLSCTX_INPROC_SERVER,
    helper.iid(),
    (void**)helper.get_address_of());
}

DropSource::~DropSource() {
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) DropSource::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) DropSource::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP DropSource::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IDropSource || iid == IID_IUnknown) {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  
  *ppvObject = 0;
  return E_NOINTERFACE;
}

//
//  IDropSource::QueryContinueDrag
//
STDMETHODIMP DropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
  if(fEscapePressed)
    return DRAGDROP_S_CANCEL;
    
  if((grfKeyState & MK_LBUTTON) == 0)
    return DRAGDROP_S_DROP;
    
  return S_OK;
}

//
//  IDropSource::GiveFeedback
//
STDMETHODIMP DropSource::GiveFeedback(DWORD dwEffect) {
  /* default impl ... */
  if(description_data) {
    if(DataObject::get_global_data_dword(description_data.get(), DataObject::Formats::IsShowingLayered)) {
      if(must_set_cursor) {
        HCURSOR cursor = (HCURSOR)LoadImageW(
                           nullptr,
                           MAKEINTRESOURCEW(OCR_NORMAL),
                           IMAGE_CURSOR,
                           0, 0,
                           LR_DEFAULTSIZE | LR_SHARED);
        
        SetCursor(cursor);
        must_set_cursor = false;
      }
      
      set_drag_image_cursor(dwEffect);
      return S_OK;
    }
    else
      must_set_cursor = true;
  }
  
  // default implementation ...
  
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

HRESULT DropSource::set_drag_image_from_window(HWND hwnd, POINT *point) {
  if(!helper)
    return E_NOINTERFACE;
  
  POINT pt = { 0, 0 };
  if(!point)
    point = &pt;
  
  HR(helper->InitializeFromWindow(hwnd, point, description_data.get()));
  return S_OK;
}

bool DropSource::set_drag_image_cursor(DWORD effect) {
  HWND hwnd = (HWND)ULongToHandle(DataObject::get_global_data_dword(description_data.get(), DataObject::Formats::DragWindow));
  if(!hwnd) 
    return false;
  
  DropDescriptionDefault wParam = DropDescriptionDefault::Unknown;
  switch(effect & ~DROPEFFECT_SCROLL) {
    case DROPEFFECT_NONE: wParam = DropDescriptionDefault::Stop; break;
    case DROPEFFECT_COPY: wParam = DropDescriptionDefault::Copy; break;
    case DROPEFFECT_MOVE: wParam = DropDescriptionDefault::Move; break;
    case DROPEFFECT_LINK: wParam = DropDescriptionDefault::Link; break;
  }
  SendMessageW(hwnd, DDWM_UPDATEWINDOW_2, (WPARAM)wParam, 0);
  
  return true;
}

//} ... class DropSource
