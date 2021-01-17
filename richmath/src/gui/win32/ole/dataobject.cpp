#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/combase.h>

#include <gui/document.h>
#include <gui/win32/ole/enumformatetc.h>
#include <gui/win32/win32-clipboard.h>

#include <graphics/cairostream.h>

#include <math.h>


namespace richmath { namespace strings {
  extern String EmptyString;
}}

using namespace richmath;
using namespace pmath;


namespace {
  class StgMediumObject : public IUnknown, public Base {
    public:
      explicit StgMediumObject(STGMEDIUM medium);
      ~StgMediumObject();
      
      static HRESULT copy(STGMEDIUM *stgm_in, STGMEDIUM *stgm_out, bool copy_from_external);
      static HRESULT make_ref_counted(STGMEDIUM *stgm);
      
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
    private:
      STGMEDIUM medium;
      LONG      refcount;
  };
  
  struct ImageSizeUserData {
    ImageSizeUserData(int w, int h) : width(w), height(h) {}
    
    int width;
    int height;
    
    static void destroy(void *p) {
      ImageSizeUserData *self = (ImageSizeUserData*)p;
      delete self;
    }
  };

  static cairo_user_data_key_t ImageSizeUserDataKey;
  static DataObject *_currently_dragged_data_object = nullptr;
}

static HGLOBAL GlobalClone(HGLOBAL hglobIn) {
  HGLOBAL hglobOut = NULL;

  void *pvIn = GlobalLock(hglobIn);
  if (pvIn) {
    SIZE_T cb = GlobalSize(hglobIn);
    HGLOBAL hglobOut = GlobalAlloc(GMEM_FIXED, cb);
    if (hglobOut) {
        CopyMemory(hglobOut, pvIn, cb);
    }
    GlobalUnlock(hglobIn);
  }

  return hglobOut;
}

//{ class DataObject ...

DataObject::DataObject() 
: refcount{1},
  data{ nullptr },
  data_count{ 0 }
{
}

DataObject::~DataObject() {
  if(_currently_dragged_data_object == this)
    _currently_dragged_data_object = nullptr;
  
  for(int i = 0; i < data_count; ++i) {
    CoTaskMemFree(data[i].format_etc.ptd);
    ReleaseStgMedium(&data[i].stg_medium);
  }
  CoTaskMemFree(data);
}

HRESULT DataObject::do_drag_drop(IDropSource *pDropSource, DWORD dwOKEffects, DWORD *pdwEffect) {
  _currently_dragged_data_object = this;
  HRESULT hr = DoDragDrop(this, pDropSource, dwOKEffects, pdwEffect);
  _currently_dragged_data_object = nullptr;
  return hr;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) DataObject::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) DataObject::Release(void) {
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
STDMETHODIMP DataObject::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IDataObject || iid == IID_IUnknown) {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

void DataObject::add_source_format(CLIPFORMAT cfFormat, TYMED tymed) {
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.tymed    = tymed;
  fmt.cfFormat = cfFormat;
  source_formats.add(fmt);
}

HRESULT DataObject::make_ref_counted(STGMEDIUM *pMedium) {
  return StgMediumObject::make_ref_counted(pMedium);
}

HRESULT DataObject::find_data_format_etc(const FORMATETC *format, struct SavedData **entry, bool add_missing) {
  *entry = nullptr;
  
  if(format->ptd) // ignore DVTARGETDEVICE
    return DV_E_DVTARGETDEVICE;
  
  for(int i = 0; i < data_count; ++i) {
    if( format->cfFormat == data[i].format_etc.cfFormat && 
        format->dwAspect == data[i].format_etc.dwAspect &&
        format->lindex   == data[i].format_etc.lindex)
    {
      if(add_missing || (format->tymed & data[i].format_etc.tymed)) {
        *entry = &data[i];
        return S_OK;
      }
      else
        return DV_E_TYMED;
    }
  }
  
  if(!add_missing)
    return DV_E_FORMATETC;
  
  struct SavedData *new_array = (struct SavedData*)CoTaskMemRealloc(data, sizeof(struct SavedData) * (data_count + 1));
  if(!new_array) 
    return E_OUTOFMEMORY;

  data = new_array;
  data[data_count].format_etc = *format;
  ZeroMemory(&data[data_count].stg_medium, sizeof(STGMEDIUM));
  *entry = &data[data_count];
  ++data_count;
  return S_OK;
}

bool DataObject::has_source_format(const FORMATETC *pFormatEtc) {
  for(int i = 0; i < source_formats.length(); ++i) {
    if( (pFormatEtc->tymed   &  source_formats[i].tymed) &&
        pFormatEtc->cfFormat == source_formats[i].cfFormat &&
        pFormatEtc->dwAspect == source_formats[i].dwAspect)
    {
      return true;
    }
  }
  
  return false;
}

//
//  IDataObject::GetData
//
STDMETHODIMP DataObject::GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) {
  if(!pFormatEtc)
    return DV_E_FORMATETC;
    
  if(!pMedium)
    return STG_E_MEDIUMFULL;
  
  struct SavedData *entry;
  if(SUCCEEDED(find_data_format_etc(pFormatEtc, &entry, false))) {
    HR(StgMediumObject::copy(&entry->stg_medium, pMedium, false));
    return S_OK;
  }
  
  if(Box *srcbox = source.get()) {
    Document *doc = srcbox->find_parent<Document>(true);
    if(!doc)
      return OLE_E_NOTRUNNING;
      
    if(!has_source_format(pFormatEtc)) 
      return DV_E_FORMATETC;
    
    AutoResetSelection auto_sel{ doc };
    doc->select(srcbox, source.start, source.end);
    
    if(cairo_surface_t *image = try_create_image(pFormatEtc, CAIRO_FORMAT_RGB24, 1, 1)) {
      RectangleF rect = doc->prepare_copy_to_image(image);
      cairo_surface_destroy(image);
      
      image = try_create_image(pFormatEtc, CAIRO_FORMAT_RGB24, rect.width, rect.height);
      if(image) {
        doc->finish_copy_to_image(image, rect);
        HRESULT hr = HRreport(image_to_medium(pFormatEtc, pMedium, image));
        cairo_surface_destroy(image);
        return hr;
      }
      
      return E_UNEXPECTED;
    }
    
    if(pFormatEtc->cfFormat == CF_TEXT) {
      String text = doc->copy_to_text(Clipboard::PlainText);
      int len;
      char *ansi = pmath_string_to_native(text.get(), &len);
      if(!ansi)
        return E_OUTOFMEMORY;
      
      HRESULT hr = HRreport(buffer_to_medium(pFormatEtc, pMedium, (const uint8_t*)ansi, len + 1));
      pmath_mem_free(ansi);
      return hr;
    }
    
    if( pFormatEtc->cfFormat == CF_UNICODETEXT || 
        pFormatEtc->cfFormat == Win32Clipboard::AtomBoxesText) 
    {
      String text = doc->copy_to_text(Win32Clipboard::win32cbformat_to_mime[pFormatEtc->cfFormat]);
      
      int len = text.length();
      const uint16_t *buf = text.buffer();
      if(!buf)
        return E_OUTOFMEMORY;
      
      return HRreport(buffer_to_medium(pFormatEtc, pMedium, (const uint8_t*)buf, len * sizeof(uint16_t)));
    }
    
    return DV_E_TYMED;
  }
  
  if(!source_content.is_null()) {
    if(pFormatEtc->cfFormat == Win32Clipboard::AtomBoxesText) {
      String text = Application::interrupt_wait(
                      Call(Symbol(PMATH_SYMBOL_MAKEBOXES), source_content),
                      Application::edit_interrupt_timeout
                    ).to_string(PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM);
      
      int len = text.length();
      const uint16_t *buf = text.buffer();
      if(!buf)
        return E_OUTOFMEMORY;
      
      return HRreport(buffer_to_medium(pFormatEtc, pMedium, (const uint8_t*)buf, len * sizeof(uint16_t)));
    }
    
    if(pFormatEtc->cfFormat == CF_UNICODETEXT || pFormatEtc->cfFormat == CF_TEXT) {
      String text = source_content.to_string();
      
      if(pFormatEtc->cfFormat == CF_UNICODETEXT) {
        int len = text.length();
        const uint16_t *buf = text.buffer();
        if(!buf)
          return E_OUTOFMEMORY;
        
        return HRreport(buffer_to_medium(pFormatEtc, pMedium, (const uint8_t*)buf, len * sizeof(uint16_t)));
      }
      else {
        int len;
        char *ansi = pmath_string_to_native(text.get(), &len);
        if(!ansi)
          return E_OUTOFMEMORY;
        
        HRESULT hr = HRreport(buffer_to_medium(pFormatEtc, pMedium, (const uint8_t*)ansi, len + 1));
        pmath_mem_free(ansi);
        return hr;
      }
    }
    
    return DV_E_TYMED;
  }
  
  return OLE_E_NOTRUNNING;
}

//
//  IDataObject::GetDataHere
//
STDMETHODIMP DataObject::GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) {
  // GetDataHere is only required for IStream and IStorage mediums
  // It is an error to call GetDataHere for things like HGLOBAL and other clipboard formats
  //
  //  OleFlushClipboard
  //
  return DATA_E_FORMATETC;
}

//
//  IDataObject::QueryGetData
//
STDMETHODIMP DataObject::QueryGetData(FORMATETC *pFormatEtc) {
  struct SavedData *entry;
  HRESULT hr = find_data_format_etc(pFormatEtc, &entry, false);
  if(SUCCEEDED(hr))
    return hr;

  if(!has_source_format(pFormatEtc))
    return DV_E_FORMATETC;
    
  return S_OK;
}

//
//  IDataObject::GetCanonicalFormatEtc
//
STDMETHODIMP DataObject::GetCanonicalFormatEtc(FORMATETC *pFormatEct, FORMATETC *pFormatEtcOut) {
  pFormatEtcOut->ptd = nullptr;
  return E_NOTIMPL;
}

//
//  IDataObject::SetData
//
STDMETHODIMP DataObject::SetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium, BOOL fRelease) {
  if(false) {
    char buffer[256];
    const char *name = "";
    if(0 != GetClipboardFormatNameA(pFormatEtc->cfFormat, buffer, sizeof(buffer) / sizeof(*buffer)))
      name = buffer;
    else switch(pFormatEtc->cfFormat) {
      case CF_TEXT:         name = "CF_TEXT";         break;
      case CF_BITMAP:       name = "CF_BITMAP";       break;
      case CF_METAFILEPICT: name = "CF_METAFILEPICT"; break;
      case CF_SYLK:         name = "CF_SYLK";         break;
      case CF_DIF:          name = "CF_DIF";          break;
      case CF_TIFF:         name = "CF_TIFF";         break;
      case CF_OEMTEXT:      name = "CF_OEMTEXT";      break;
      case CF_DIB:          name = "CF_DIB";          break;
      case CF_PALETTE:      name = "CF_PALETTE";      break;
      case CF_PENDATA:      name = "CF_PENDATA";      break;
      case CF_RIFF:         name = "CF_RIFF";         break;
      case CF_WAVE:         name = "CF_WAVE";         break;
      case CF_UNICODETEXT:  name = "CF_UNICODETEXT";  break;
      case CF_ENHMETAFILE:  name = "CF_ENHMETAFILE";  break;
      case CF_HDROP:        name = "CF_HDROP";        break;
      case CF_LOCALE:       name = "CF_LOCALE";       break;
      case CF_DIBV5:        name = "CF_DIBV5";        break;
    }
    
    char val_buf[30] = "";
    if(pMedium->tymed == TYMED_HGLOBAL) {
      void *p = GlobalLock(pMedium->hGlobal);
      size_t size = GlobalSize(pMedium->hGlobal);
      switch(size) {
        case sizeof(DWORD): 
          snprintf(val_buf, sizeof(val_buf), "= DWORD 0x%x", *(DWORD*)p);
          break;
        default:
          snprintf(val_buf, sizeof(val_buf), "= %d bytes", (int)size);
          break;
      }
      GlobalUnlock(pMedium->hGlobal);
    }
    
    pmath_debug_print(
      "[DataObject::SetData %s (0x%x) %s%s%s%s%s%s%s(0x%x) %s]\n", 
      name, 
      (int)pFormatEtc->cfFormat,
      pFormatEtc->tymed & TYMED_HGLOBAL ?  "HGLOBAL " : "",
      pFormatEtc->tymed & TYMED_FILE ?     "FILE " : "",
      pFormatEtc->tymed & TYMED_ISTREAM ?  "ISTREAM " : "",
      pFormatEtc->tymed & TYMED_ISTORAGE ? "ISTORAGE " : "",
      pFormatEtc->tymed & TYMED_GDI ?      "GDI" : "",
      pFormatEtc->tymed & TYMED_MFPICT ?   "MFPICT" : "",
      pFormatEtc->tymed & TYMED_ENHMF ?    "ENHMF" : "",
      (int)pFormatEtc->tymed,
      val_buf);
  }
  
  struct SavedData *entry;
  HR(find_data_format_etc(pFormatEtc, &entry, true));
  
  if(entry->stg_medium.tymed) {
    ReleaseStgMedium(&entry->stg_medium);
    ZeroMemory(&entry->stg_medium, sizeof(STGMEDIUM));
  }
  
  HRESULT hr;
  if(fRelease) {
    entry->stg_medium = *pMedium;
    hr = S_OK;
  }
  else
    hr = StgMediumObject::copy(pMedium, &entry->stg_medium, true);
  
  return hr;
}

//
//  IDataObject::EnumFormatEtc
//
STDMETHODIMP DataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc) {
  if(dwDirection == DATADIR_GET) {
    if(!ppEnumFormatEtc)
      return E_INVALIDARG;
      
    *ppEnumFormatEtc = new richmath::EnumFormatEtc(this);
    return (*ppEnumFormatEtc) ? S_OK : E_OUTOFMEMORY;
  }
  
  return E_NOTIMPL;
}

//
//  IDataObject::DAdvise
//
STDMETHODIMP DataObject::DAdvise(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) {
  return OLE_E_ADVISENOTSUPPORTED;
}

//
//  IDataObject::DUnadvise
//
STDMETHODIMP DataObject::DUnadvise(DWORD dwConnection) {
  return OLE_E_ADVISENOTSUPPORTED;
}

//
//  IDataObject::EnumDAdvise
//
STDMETHODIMP DataObject::EnumDAdvise(IEnumSTATDATA **ppEnumAdvise) {
  return OLE_E_ADVISENOTSUPPORTED;
}

DataObject *DataObject::as_current_data_object(IDataObject *obj) {
  if(!obj || !_currently_dragged_data_object)
    return nullptr;
  
  if(get_canonical_iunknown(_currently_dragged_data_object) == get_canonical_iunknown(obj))
    return _currently_dragged_data_object;
  
  return nullptr;
}

HRESULT DataObject::buffer_to_medium(
  const FORMATETC *pFormatEtc, 
  STGMEDIUM       *pMedium, 
  const uint8_t   *data, 
  size_t           len
) {
  if(pFormatEtc->tymed & TYMED_HGLOBAL) {
    ZeroMemory(pMedium, sizeof(*pMedium));
    pMedium->tymed = TYMED_HGLOBAL;
    pMedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE, len);
    if(pMedium->hGlobal) {
      size_t alloc_size = GlobalSize(pMedium->hGlobal);
      
      uint8_t *dst = (uint8_t*)GlobalLock(pMedium->hGlobal);
      memcpy(dst, data, len);
      memset(dst + len, 0, alloc_size - len);
      
      GlobalUnlock(pMedium->hGlobal);
      return S_OK;
    }
    return E_OUTOFMEMORY;
  }
  
  return DV_E_TYMED;
}

namespace {
  struct BufferToMediumContext {
    const FORMATETC *pFormatEtc;
    STGMEDIUM       *pMedium;
    HRESULT          hresult;
    
    static void flush(
      uint8_t        *readable,
      uint8_t       **writable,
      const uint8_t  *end,
      void           *closure
    ) {
      BufferToMediumContext *self = (BufferToMediumContext *)closure;
      
      self->hresult = DataObject::buffer_to_medium(
                        self->pFormatEtc,
                        self->pMedium,
                        readable,
                        (size_t) *writable - (size_t)readable);
    }
  };
}

HRESULT DataObject::buffer_to_medium(
  const FORMATETC *pFormatEtc, 
  STGMEDIUM       *pMedium, 
  BinaryFile       binbuffer
) {
  BufferToMediumContext context = { 0 };
  
  context.pFormatEtc = pFormatEtc;
  context.pMedium    = pMedium;
  context.hresult    = E_UNEXPECTED;
  
  // TODO: IStream should be handles by delay-loading from binbuffer
  
  pmath_file_binary_buffer_manipulate(
    binbuffer.get(), BufferToMediumContext::flush, &context);
    
  return context.hresult;
}

HRESULT DataObject::image_to_medium(const FORMATETC *pFormatEtc, STGMEDIUM *pMedium, cairo_surface_t *image) {
  COM_ASSERT(pFormatEtc != nullptr);
  COM_ASSERT(pMedium != nullptr);
  COM_ASSERT(image != nullptr);
  
  cairo_status_t status = cairo_surface_status(image);
  if(status != CAIRO_STATUS_SUCCESS) {
    pmath_debug_print("[cairo error: %s]\n", cairo_status_to_string(status));
    return E_OUTOFMEMORY;
  }
  
  if(cairo_surface_get_type(image) == CAIRO_SURFACE_TYPE_WIN32) {
    if(pFormatEtc->tymed & TYMED_GDI) {
      cairo_surface_flush(image);
      if(HDC dc = cairo_win32_surface_get_dc(image)) {
        /* We cannot use the selected HBITMAP from dc, because cairo still 
           owns it and will DeleteObject it when image gets freed.
        */
        ImageSizeUserData *size = (ImageSizeUserData*)cairo_surface_get_user_data(image, &ImageSizeUserDataKey);
        if(size) {
          //int width  = cairo_image_surface_get_width( image);
          //int height = cairo_image_surface_get_height(image);
          
          HDC memDC = CreateCompatibleDC(dc);
          if(!memDC) 
            return E_OUTOFMEMORY;
            
          HBITMAP memBM = CreateCompatibleBitmap(dc, size->width, size->height);
          if(!memBM) {
            DeleteDC(memDC);
            return E_OUTOFMEMORY;
          }
          
          SelectObject(memDC, memBM);
          BitBlt(memDC, 0, 0, size->width, size->height, dc, 0, 0, SRCCOPY);
          
          ZeroMemory(pMedium, sizeof(*pMedium));
          pMedium->tymed = TYMED_GDI;
          pMedium->hBitmap = memBM;
          
          DeleteDC(memDC);
          return S_OK;
        }
      }
    }
    
    cairo_surface_t *img = cairo_win32_surface_get_image(image);
    if(!img)
      return DV_E_FORMATETC;
    
    image = img;
  }
  
  if(cairo_surface_get_type(image) == CAIRO_SURFACE_TYPE_IMAGE) {
    int height = cairo_image_surface_get_height(image);
    int width  = cairo_image_surface_get_width(image);
    int stride = cairo_image_surface_get_stride(image);
    
    BITMAPINFOHEADER bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.biSize = sizeof(bmi);
    bmi.biWidth  = width;
    bmi.biHeight = height; /* upside down */
    bmi.biPlanes = 1;
    bmi.biXPelsPerMeter = (LONG)((double)96 * 100 / 2.54 + 0.5);
    bmi.biYPelsPerMeter = (LONG)((double)96 * 100 / 2.54 + 0.5);
      
    cairo_format_t format = cairo_image_surface_get_format(image);
    switch(format) {
      case CAIRO_FORMAT_RGB24:
      case CAIRO_FORMAT_ARGB32:
        bmi.biBitCount     = 32;
        bmi.biCompression  = BI_RGB;
        bmi.biClrUsed      = 0;
        bmi.biClrImportant = 0;
        if(stride != 4 * width)
          return DV_E_FORMATETC;
        break;
        
      default:
        return DV_E_FORMATETC;
    }
    
    cairo_surface_flush(image);
    const uint8_t *bits = cairo_image_surface_get_data(image);
    if(!bits)
      return DV_E_FORMATETC;
    
    WriteableBinaryFile mem { BinaryFile::create_buffer(sizeof(bmi) + stride * height) };
    if(mem.is_null()) 
      return E_OUTOFMEMORY;
    
    mem.write(&bmi, sizeof(bmi));
    for(int row = height - 1; row >= 0; --row) 
      mem.write(bits + row * stride, stride);
    
    return buffer_to_medium(pFormatEtc, pMedium, mem);
  }
  
  if(auto stream = CairoStream::from_surface(image)) {
    //cairo_surface_finish(image);
    cairo_surface_flush(image);
    
    return buffer_to_medium(pFormatEtc, pMedium, stream->file);
  }
  
  return DATA_E_FORMATETC;
}

static bool is_valid_ptd(const DVTARGETDEVICE *ptd) {
  if(!ptd)
    return false;
  
  if( ptd->tdDriverNameOffset >= ptd->tdSize ||
      ptd->tdDeviceNameOffset >= ptd->tdSize ||
      ptd->tdPortNameOffset >= ptd->tdSize ||
      ptd->tdExtDevmodeOffset + sizeof(DEVMODEA) >= ptd->tdSize
  ) {
    return false;
  }
  
  const char *buf = (const char*)ptd;
  size_t offset = ptd->tdDriverNameOffset;
  if(strnlen(buf + offset, ptd->tdSize - offset) < ptd->tdSize - offset)
    return false;
  
  offset = ptd->tdDeviceNameOffset;
  if(strnlen(buf + offset, ptd->tdSize - offset) < ptd->tdSize - offset)
    return false;
  
  offset = ptd->tdPortNameOffset;
  if(strnlen(buf + offset, ptd->tdSize - offset) < ptd->tdSize - offset)
    return false;
  
  return true;
}

static HDC create_target_dc(const DVTARGETDEVICE *ptd) {
  if(is_valid_ptd(ptd)) {
    const char *buf = (const char*)ptd;
    
    const char *driver = buf + ptd->tdDriverNameOffset;
    const char *device = buf + ptd->tdDeviceNameOffset;
    const char *port   = buf + ptd->tdPortNameOffset;
    const DEVMODEA *dm = (DEVMODEA*)(buf + ptd->tdExtDevmodeOffset);
    
    return CreateDCA(driver, device, port, dm);
  }
  
  return nullptr;
}

cairo_surface_t *DataObject::try_create_image(const FORMATETC *pFormatEtc, cairo_format_t img_format, double width, double height) {
  if(pFormatEtc->cfFormat == CF_DIB) {
    int w = (int)ceil(width);
    int h = (int)ceil(height);
    
    if(w < 1)
      w = 1;
    if(h < 1)
      h = 1;
      
    return cairo_win32_surface_create_with_dib(img_format, w, h);
  }
  
  if(pFormatEtc->cfFormat == CF_BITMAP) {
    int w = (int)ceil(width);
    int h = (int)ceil(height);
    
    if(w < 1)
      w = 1;
    if(h < 1)
      h = 1;
    
    HDC hdc = nullptr;
    if(pFormatEtc->ptd) 
      hdc = create_target_dc(pFormatEtc->ptd);
    else
      hdc = GetDC(nullptr);
      
    if(!hdc)
      return nullptr;
    
    cairo_surface_t *image = cairo_win32_surface_create_with_ddb(hdc, img_format, w, h);
    
    if(pFormatEtc->ptd)
      DeleteDC(hdc);
    else
      ReleaseDC(nullptr, hdc);
    
    cairo_surface_set_user_data(image, &ImageSizeUserDataKey, new ImageSizeUserData(w, h), ImageSizeUserData::destroy);
    return image;
  }
  
#if CAIRO_HAS_SVG_SURFACE
  if(pFormatEtc->cfFormat == Win32Clipboard::AtomSvgImage) {
    return CairoStream::create_svg_surface(WriteableBinaryFile{BinaryFile::create_buffer(0)}, width, height);
  }
#endif

  return nullptr;
}

void DataObject::clear_drop_description(IDataObject *obj) {
  if(!obj)
    return;
  
  FORMATETC desc_format;
  STGMEDIUM desc_medium;
  if( SUCCEEDED(DataObject::get_global_data(obj, 
                                            Win32Clipboard::Formats::DropDescription, 
                                            &desc_format, 
                                            &desc_medium,
                                            sizeof(DROPDESCRIPTION)))) 
  {
    // Workaround for Windows bug (issue #94: explorer.exe crashes due to drop-descriptions)
    // We create a new STGMEDIUM instead of changing desc_medium in-place, because
    // explorer.exe would crash otherwise when the mouse leaves our window.
    DROPDESCRIPTION new_desc;
    {
      const DROPDESCRIPTION *orig_desc = (DROPDESCRIPTION*)GlobalLock(desc_medium.hGlobal);
      new_desc = *orig_desc;
      GlobalUnlock(desc_medium.hGlobal);
      ReleaseStgMedium(&desc_medium);
      ZeroMemory(&desc_medium, sizeof(desc_medium));
    }
    
    bool change_desc = DataObject::clear_drop_description(&new_desc);
    if(change_desc) 
      DataObject::set_global_data(obj, Win32Clipboard::Formats::DropDescription, &new_desc, sizeof(new_desc));
  }
}

bool DataObject::clear_drop_description(DROPDESCRIPTION *desc) {
	COM_ASSERT(desc);

	bool did_change = desc->type != DROPIMAGE_INVALID || desc->szMessage[0] != L'\0' || desc->szInsert[0] != L'\0'; 
	desc->type = DROPIMAGE_INVALID;
	desc->szMessage[0] = L'\0';
	desc->szInsert[0] = L'\0';
	
	return did_change;
}

namespace {
  static bool copy_string(wchar_t *dst, size_t dstsize, const wchar_t *src, size_t srclen) {
    if(dstsize == 0)
      return false;
    
    if(srclen >= dstsize)
      srclen = dstsize - 1;
    
    bool changed = dst[srclen] != L'\0' || 0 != memcmp(dst, src, sizeof(wchar_t) * srclen);
    if(!changed)
      return false;
    
    memcpy(dst, src, sizeof(wchar_t) * srclen);
    dst[srclen] = L'\0';
    return true;
  }
  static bool copy_string(wchar_t *dst, size_t dstsize, const String &src) {
    return copy_string(dst, dstsize, (const wchar_t*)src.buffer(), (size_t)src.length());
  }
}

void DataObject::set_drop_description(IDataObject *obj, DROPIMAGETYPE image, const String &message, const String &insertion, bool create) {
  if(!obj)
    return;
  
  FORMATETC desc_format;
  STGMEDIUM desc_medium;
  if( SUCCEEDED(DataObject::get_global_data(obj, 
                                            Win32Clipboard::Formats::DropDescription, 
                                            &desc_format, 
                                            &desc_medium,
                                            sizeof(DROPDESCRIPTION)))) 
  {
    // Workaround for Windows bug (issue #94: explorer.exe crashes due to drop-descriptions)
    // We create a new STGMEDIUM instead of changing desc_medium in-place, because
    // explorer.exe would crash otherwise when the mouse leaves our window.
    DROPDESCRIPTION new_desc;
    {
      const DROPDESCRIPTION *orig_desc = (DROPDESCRIPTION*)GlobalLock(desc_medium.hGlobal);
      new_desc = *orig_desc;
      
      GlobalUnlock(desc_medium.hGlobal);
      ReleaseStgMedium(&desc_medium);
      ZeroMemory(&desc_medium, sizeof(desc_medium));
    }
    
    bool change_desc = false;
    if(image == DROPIMAGE_INVALID) {
      change_desc = clear_drop_description(&new_desc);
    }
    else {
      if(new_desc.type != image) {
        change_desc = true;
        new_desc.type = image;
      }
      
      change_desc = copy_string(new_desc.szMessage, sizeof(new_desc.szMessage) / sizeof(wchar_t), message) || change_desc;
      change_desc = copy_string(new_desc.szInsert, sizeof(new_desc.szInsert) / sizeof(wchar_t), insertion) || change_desc;
    }
    
    if(change_desc) 
      DataObject::set_global_data(obj, Win32Clipboard::Formats::DropDescription, &new_desc, sizeof(new_desc));
  }
  else if(create && image != DROPIMAGE_INVALID) {
    desc_medium.tymed = TYMED_HGLOBAL;
    desc_medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPDESCRIPTION));
    desc_medium.pUnkForRelease = nullptr;
    
    if(desc_medium.hGlobal) {
      DROPDESCRIPTION *desc = (DROPDESCRIPTION*)GlobalLock(desc_medium.hGlobal);
      
      desc->type = image;
      copy_string(desc->szMessage, sizeof(desc->szMessage) / sizeof(wchar_t), message);
      copy_string(desc->szInsert, sizeof(desc->szInsert) / sizeof(wchar_t), insertion);
      
      GlobalUnlock(desc_medium.hGlobal);
      HRreport(DataObject::make_ref_counted(&desc_medium));
      
      if(!HRbool(obj->SetData(&desc_format, &desc_medium, TRUE)))
        ReleaseStgMedium(&desc_medium);
    }
  }
}

HRESULT DataObject::get_global_data(IDataObject *obj, CLIPFORMAT format, FORMATETC *format_etc, STGMEDIUM *medium, size_t min_size) {
  COM_ASSERT(format_etc != nullptr);
  COM_ASSERT(medium != nullptr);
  
  if(!obj) 
    return E_INVALIDARG;
  
  format_etc->cfFormat = format;
  format_etc->ptd      = nullptr;
  format_etc->dwAspect = DVASPECT_CONTENT;
  format_etc->lindex   = -1;
  format_etc->tymed    = TYMED_HGLOBAL;
  
  HRquiet(obj->QueryGetData(format_etc));
  HR(obj->GetData(format_etc, medium));
  if(medium->tymed != TYMED_HGLOBAL) {
    ReleaseStgMedium(medium);
    ZeroMemory(medium, sizeof(STGMEDIUM));
    return DV_E_TYMED;
  }
  
  if(min_size && GlobalSize(medium->hGlobal) < min_size) {
    pmath_debug_print("[get_global_data: format 0x%x got %d bytes (flags 0x%x) but expected %d]\n", 
      format, GlobalSize(medium->hGlobal), GlobalFlags(medium->hGlobal), min_size);
    ReleaseStgMedium(medium);
    ZeroMemory(medium, sizeof(STGMEDIUM));
    return E_UNEXPECTED;
  }
  
  return S_OK;
}

DWORD DataObject::get_global_data_dword(IDataObject *obj, CLIPFORMAT format) {
  DWORD data = 0;
  FORMATETC format_etc;
  STGMEDIUM medium;
  
  if(SUCCEEDED(get_global_data(obj, format, &format_etc, &medium, sizeof(DWORD)))) {
    data = *(DWORD*)GlobalLock(medium.hGlobal);
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
  }
  return data;
}

HRESULT DataObject::set_global_data(IDataObject *obj, CLIPFORMAT format, void *data, size_t size) {
  FORMATETC format_etc;
  STGMEDIUM medium;
  
  format_etc.cfFormat = format;
  format_etc.ptd      = nullptr;
  format_etc.dwAspect = DVASPECT_CONTENT;
  format_etc.lindex   = -1;
  format_etc.tymed    = TYMED_HGLOBAL;
  
  medium.tymed = TYMED_HGLOBAL;
  medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
  medium.pUnkForRelease = nullptr;
  if(!medium.hGlobal) 
    return E_OUTOFMEMORY;
  
  void *dst = (DWORD*)GlobalLock(medium.hGlobal);
  memcpy(dst, data, size);
  
  GlobalUnlock(medium.hGlobal);
  HRESULT hr = HRreport(obj->SetData(&format_etc, &medium, TRUE));
  if(!SUCCEEDED(hr))
    ReleaseStgMedium(&medium);
  
  return hr;
}

HRESULT DataObject::set_global_data_dword(IDataObject *obj, CLIPFORMAT format, DWORD value) {
  return set_global_data(obj, format, &value, sizeof(value));
}

static String string_from_ansi(const char *s, int len) {
  static_assert(sizeof(uint16_t) == sizeof(wchar_t), "wchar_t must be 2 bytes on Win32");

  if(len == 0)
    return strings::EmptyString;
  
  int wlen = MultiByteToWideChar(CP_ACP, 0, s, len, nullptr, 0);
  pmath_string_t str = pmath_string_new_raw(wlen);
  uint16_t *buf;
  if(pmath_string_begin_write(&str, &buf, &wlen)) {
    wlen = MultiByteToWideChar(CP_ACP, 0, s, len, (wchar_t*)buf, wlen);
    pmath_string_end_write(&str, &buf);
  }
  return String(pmath_string_part(str, 0, wlen));
}

Expr DataObject::get_global_data_dropfiles(HGLOBAL hglb) {
  Expr list;
  
  if(auto data = (const DROPFILES*)GlobalLock(hglb)) {
    size_t size = GlobalSize(hglb);
    
    if(sizeof(DROPFILES) < size && data->pFiles < size) {
      list = MakeList(0);
      if(data->fWide) {
        const wchar_t *s = (const wchar_t*)(((const char *)data) + data->pFiles);
        const wchar_t *end = (const wchar_t*)(((const char *)data) + size);
        
        while(s < end && *s) {
          size_t len = wcsnlen(s, end - s - 1);
          if(len < INT_MAX) {
            list.append(String::FromUcs2((const uint16_t*)s, (int)len));
          }
          s+= len + 1;
        }
      }
      else {
        const char *s = ((const char *)data) + data->pFiles;
        const char *end = ((const char *)data) + size;
        
        while(s < end && *s) {
          size_t len = strnlen(s, end - s - 1);
          if(len < INT_MAX) {
            list.append(string_from_ansi(s, (int)len));
          }
          s+= len + 1;
        }
      }
    }
    
    GlobalUnlock(hglb);
  }
  return list;
}

Expr DataObject::get_global_data_dropfiles(IDataObject *obj) {
  Expr list;
  FORMATETC format_etc;
  STGMEDIUM medium;
  
  if(SUCCEEDED(get_global_data(obj, CF_HDROP, &format_etc, &medium, 0))) {
    list = get_global_data_dropfiles(medium.hGlobal);
    ReleaseStgMedium(&medium);
  }
  return list;
}

//} ... class DataObject

//{ class StgMediumObject ...

StgMediumObject::StgMediumObject(STGMEDIUM medium)
  : medium{medium},
    refcount{1}
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

StgMediumObject::~StgMediumObject() {
  pmath_debug_print("[StgMediumObject::~StgMediumObject]\n");
  ReleaseStgMedium(&medium);
}

HRESULT StgMediumObject::copy(STGMEDIUM *stgm_in, STGMEDIUM *stgm_out, bool copy_from_external) {
  STGMEDIUM result = *stgm_in;
  
  if(result.pUnkForRelease == nullptr && !(result.tymed & (TYMED_ISTREAM | TYMED_ISTREAM))) {
    if(copy_from_external) {
      if(result.tymed == TYMED_HGLOBAL) {
        result.hGlobal = GlobalClone(result.hGlobal);
        if(!result.hGlobal)
          return E_OUTOFMEMORY;
      }
      else
        return DV_E_TYMED;
    }
    else {
      result.pUnkForRelease = stgm_in->pUnkForRelease = new StgMediumObject(*stgm_in);
    }
  }
  
  switch(result.tymed) {
    case TYMED_ISTREAM:
      result.pstm->AddRef();
      break;
    case TYMED_ISTORAGE:
      result.pstg->AddRef();
      break;
  }
  
  if(result.pUnkForRelease)
    result.pUnkForRelease->AddRef();
  
  *stgm_out = result;
  return S_OK;
}

HRESULT StgMediumObject::make_ref_counted(STGMEDIUM *stgm) {
  if(!stgm)
    return E_INVALIDARG;
  
  if(stgm->pUnkForRelease)
    return S_OK;
  
  if(stgm->tymed & (TYMED_ISTREAM | TYMED_ISTREAM))
    return S_OK;
  
  stgm->pUnkForRelease = new StgMediumObject(*stgm);
  
  return S_OK;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) StgMediumObject::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) StgMediumObject::Release(void) {
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
STDMETHODIMP StgMediumObject::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IUnknown) {
    AddRef();
    *ppvObject = static_cast<IUnknown*>(this);
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//} ... class StgMediumObject
