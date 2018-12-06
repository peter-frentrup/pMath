#define OEMRESOURCE

#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>
#include <gui/document.h>

#include <shobjidl.h>
#include <shlguid.h>

#include <math.h>


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
    
  if(!description_data)
    return E_FAIL;
    
  POINT pt = { 0, 0 };
  if(!point)
    point = &pt;
    
  HR(helper->InitializeFromWindow(hwnd, point, description_data.get()));
  return S_OK;
}

namespace {
  // see https://www.codeproject.com/Articles/886711/Drag-Drop-Images-and-Drop-Descriptions-for-MFC-App
  static HBITMAP replace_black(HBITMAP hBitmap) {
    if(!hBitmap)
      return hBitmap;
      
    DIBSECTION ds;
    int nSize = GetObject(hBitmap, sizeof(ds), &ds);
    if(nSize < sizeof(BITMAP))
      return hBitmap;
      
    if(ds.dsBm.bmBitsPixel < 24)
      return hBitmap;
      
    if(nSize < sizeof(ds)) {
      // not a DIBSECTION
      // Create a DIBSECTION copy and delete the original bitmap.
      HBITMAP hCopy = (HBITMAP)CopyImage(
                        hBitmap, IMAGE_BITMAP, 0, 0,
                        LR_CREATEDIBSECTION | LR_COPYDELETEORG);
      if(hCopy) {
        hBitmap = hCopy;
        nSize = GetObject(hBitmap, sizeof(ds), &ds);
      }
    }
    
    if(nSize != sizeof(ds))
      return hBitmap;
    
    BYTE *pixel_row = (BYTE*)ds.dsBm.bmBits;
    int bytes_per_pixel = ds.dsBm.bmBitsPixel / 8;
    for(int i = 0; i < ds.dsBm.bmHeight; i++) {
      BYTE *pixels = pixel_row;
      for(int j = 0; j < ds.dsBm.bmWidth; j++) {
        if(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 0) {
          pixels[0] = 0x01;
          pixels[1] = 0x01;
          pixels[2] = 0x01;
        }
        pixels += bytes_per_pixel;
      }
      pixel_row += ds.dsBm.bmWidthBytes;
    }
    return hBitmap;
  }
}

HRESULT DropSource::set_drag_image_from_document(const Point &mouse, SelectionReference source) {
  if(!helper)
    return E_NOINTERFACE;
    
  if(!description_data)
    return E_FAIL;
    
  Box *box = source.get();
  if(!box)
    return E_INVALIDARG;
    
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return E_UNEXPECTED;
    
  {
    AutoResetSelection auto_sel{ doc };
    doc->select(box, source.start, source.end);
    
    cairo_format_t format = CAIRO_FORMAT_RGB24;
    cairo_surface_t *image = cairo_win32_surface_create_with_ddb(nullptr, format, 1, 1);
    Rectangle rect;
    doc->prepare_copy_to_image(image, &rect);
    cairo_surface_destroy(image);
    
    int w = (int)ceil(rect.width - 0.001);
    int h = (int)ceil(rect.height - 0.001);
    if(w < 1) w = 1;
    if(h < 1) h = 1;
    
    image = cairo_win32_surface_create_with_ddb(nullptr, format, w, h);
    doc->finish_copy_to_image(image, rect);
  }
  
  SHDRAGIMAGE di = {0};
  di.crColorKey = CLR_NONE; // or 0xFFFFFF;
  di.sizeDragImage.cx = w;
  di.sizeDragImage.cy = h;
  //di.hbmpDragImage = nullptr;
  di.ptOffset.x = (int)ceil(mouse.x - rect.x);
  di.ptOffset.y = (int)ceil(mouse.y - rect.y);
  
  if(di.ptOffset.x < 0) di.ptOffset.x = 0;
  if(di.ptOffset.y < 0) di.ptOffset.y = 0;
  if(di.ptOffset.x > w) di.ptOffset.x = w;
  if(di.ptOffset.y > h) di.ptOffset.y = h;
  
  // TODO: change every black pixel to e.g. #010101, because black implicitly gets alpha=0
  
  if(HDC dc = cairo_win32_surface_get_dc(image)) {
    /* We cannot use the selected HBITMAP from dc, because cairo still
       owns it and will DeleteObject it when image gets freed.
    */
    HDC memDC = CreateCompatibleDC(dc);
    if(!memDC) {
      cairo_surface_destroy(image);
      return E_OUTOFMEMORY;
    }
    
    di.hbmpDragImage = CreateCompatibleBitmap(dc, di.sizeDragImage.cx, di.sizeDragImage.cy);
    if(!di.hbmpDragImage) {
      DeleteDC(memDC);
      cairo_surface_destroy(image);
      return E_OUTOFMEMORY;
    }
    
    SelectObject(memDC, di.hbmpDragImage);
    BitBlt(memDC, 0, 0, di.sizeDragImage.cx, di.sizeDragImage.cy, dc, 0, 0, SRCCOPY);
    
    DeleteDC(memDC);
    cairo_surface_destroy(image);
  }
  
  di.hbmpDragImage = replace_black(di.hbmpDragImage);
  
  HRESULT hr = helper->InitializeFromBitmap(&di, description_data.get());
  if(!HRbool(hr)) {
    DeleteObject(di.hbmpDragImage); di.hbmpDragImage = nullptr;
    return hr;
  }
  
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
