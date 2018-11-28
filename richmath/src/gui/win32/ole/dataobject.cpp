#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/combase.h>

#include <gui/document.h>
#include <gui/win32/ole/enumformatetc.h>
#include <gui/win32/win32-clipboard.h>


using namespace richmath;

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
  for(int i = 0; i < data_count; ++i) {
    CoTaskMemFree(data[i].format_etc.ptd);
    ReleaseStgMedium(&data[i].stg_medium);
  }
  CoTaskMemFree(data);
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
  
  *ppvObject = 0;
  return E_NOINTERFACE;
}

void DataObject::add_source_format(CLIPFORMAT cfFormat) {
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.tymed    = TYMED_HGLOBAL;
  fmt.cfFormat = cfFormat;
  source_formats.add(fmt);
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

HRESULT DataObject::add_ref_std_medium(const STGMEDIUM *stgm_in, STGMEDIUM *stgm_out, bool copy_from_external) {
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
    else
      result.pUnkForRelease = static_cast<IDataObject*>(this);
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
    HR(add_ref_std_medium(&entry->stg_medium, pMedium, false));
  }
    
  Box *srcbox = source.get();
  if(!srcbox)
    return OLE_E_NOTRUNNING;
    
  Document *doc = srcbox->find_parent<Document>(true);
  if(!doc)
    return OLE_E_NOTRUNNING;
    
  if(!has_source_format(pFormatEtc)) 
    return DV_E_FORMATETC;
  
  switch(pFormatEtc->tymed) {
    case TYMED_HGLOBAL: {
        int  old_s = doc->selection_start();
        int  old_e = doc->selection_end();
        Box *old_b = doc->selection_box();
        
        doc->select(srcbox, source.start, source.end);
        String data = doc->copy_to_text(Win32Clipboard::win32cbformat_to_mime[pFormatEtc->cfFormat]);
        
        int len = data.length();
        
        pMedium->tymed = TYMED_HGLOBAL;
        pMedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(uint16_t));
        if(pMedium->hGlobal) {
          uint16_t *dst = (uint16_t*)GlobalLock(pMedium->hGlobal);
          memcpy(dst, data.buffer(), len * sizeof(uint16_t));
          for(int i = 0; i < len; ++i) {
            if(dst[i] == '\0')
              dst[i] = '?';
          }
          dst[len] = '\0';
          GlobalUnlock(pMedium->hGlobal);
        }
        
        doc->select(old_b, old_s, old_e);
        
        if(pMedium->hGlobal)
          return S_OK;
        return E_OUTOFMEMORY;
      }
      
    default: break;
  }
  
  return DV_E_TYMED;
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
    hr = add_ref_std_medium(pMedium, &entry->stg_medium, true);
  
  // break circular reference loop:
  if(get_canonical_iunknown(entry->stg_medium.pUnkForRelease) == get_canonical_iunknown(static_cast<IDataObject*>(this))) {
    entry->stg_medium.pUnkForRelease->Release();
    entry->stg_medium.pUnkForRelease = nullptr;
  }
  
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

//} ... class DataObject

