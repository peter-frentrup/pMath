#include <gui/win32/ole/dataobject.h>

#include <gui/document.h>
#include <gui/win32/ole/enumformatetc.h>


using namespace richmath;

//{ class DataObject ...

DataObject::DataObject(){
  refcount = 1;
}

DataObject::~DataObject(){
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) DataObject::AddRef(void){
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) DataObject::Release(void){
  LONG count = InterlockedDecrement(&refcount);

  if(count == 0){
    delete this;
    return 0;
  }
  
  return count;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP DataObject::QueryInterface(REFIID iid, void **ppvObject){
  if(iid == IID_IDataObject || iid == IID_IUnknown){
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  
  *ppvObject = 0;
  return E_NOINTERFACE;
}

int DataObject::lookup_format_etc(FORMATETC *pFormatEtc){
  for(int i = 0; i < formats.length();++i){
    if((pFormatEtc->tymed   &  formats[i].tymed)
    && pFormatEtc->cfFormat == formats[i].cfFormat
    && pFormatEtc->dwAspect == formats[i].dwAspect)
      return i;
  }

  return -1;
}

//
//  IDataObject::GetData
//
STDMETHODIMP DataObject::GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium){
  if(!pFormatEtc)
    return DV_E_FORMATETC;
  
  if(!pMedium)
    return STG_E_MEDIUMFULL;
  
  Box *srcbox = source.get();
  
  if(!srcbox)
    return OLE_E_NOTRUNNING;
    
  Document *doc = srcbox->find_parent<Document>(true);
  if(!doc)
    return OLE_E_NOTRUNNING;
  
  int i = lookup_format_etc(pFormatEtc);
  if(i < 0){
    return DV_E_FORMATETC;
  }
  
  switch(pFormatEtc->tymed){
    case TYMED_HGLOBAL: {
      int  old_s = doc->selection_start();
      int  old_e = doc->selection_end();
      Box *old_b = doc->selection_box();
      
      doc->select(srcbox, source.start, source.end);
      String data = doc->copy_to_text(mimetypes[i]);
      
      int len = data.length();
      
      pMedium->tymed = TYMED_HGLOBAL;
      pMedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(uint16_t)); 
      if(pMedium->hGlobal){
        uint16_t *dst = (uint16_t*)GlobalLock(pMedium->hGlobal); 
        memcpy(dst, data.buffer(), len * sizeof(uint16_t));
        for(int i = 0;i < len;++i){
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
STDMETHODIMP DataObject::GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium){
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
STDMETHODIMP DataObject::QueryGetData(FORMATETC *pFormatEtc){
  if(lookup_format_etc(pFormatEtc) == -1)
    return DV_E_FORMATETC;
  
  return S_OK;
}

//
//  IDataObject::GetCanonicalFormatEtc
//
STDMETHODIMP DataObject::GetCanonicalFormatEtc(FORMATETC *pFormatEct, FORMATETC *pFormatEtcOut){
  pFormatEtcOut->ptd = NULL;
  return E_NOTIMPL;
}

//
//  IDataObject::SetData
//
STDMETHODIMP DataObject::SetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium, BOOL fRelease){
  return E_NOTIMPL;
}

//
//  IDataObject::EnumFormatEtc
//
STDMETHODIMP DataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc){
  if(dwDirection == DATADIR_GET){
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
STDMETHODIMP DataObject::DAdvise(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection){
  return OLE_E_ADVISENOTSUPPORTED;
}

//
//  IDataObject::DUnadvise
//
STDMETHODIMP DataObject::DUnadvise (DWORD dwConnection){
  return OLE_E_ADVISENOTSUPPORTED;
}

//
//  IDataObject::EnumDAdvise
//
STDMETHODIMP DataObject::EnumDAdvise (IEnumSTATDATA **ppEnumAdvise){
  return OLE_E_ADVISENOTSUPPORTED;
}

//} ... class DataObject

