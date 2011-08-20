#include <gui/win32/ole/enumformatetc.h>


using namespace richmath;

static void deep_copy_format_etc(FORMATETC *dest, FORMATETC *source) {
  *dest = *source;
  
  if(source->ptd) {
    dest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
    
    *(dest->ptd) = *(source->ptd);
  }
}

//{ class EnumFormatEtc ...

EnumFormatEtc::EnumFormatEtc(DataObject *_src) {
  refcount = 1;
  index = 0;
  src = _src;
  src->AddRef();
}

EnumFormatEtc::~EnumFormatEtc() {
  src->Release();
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) EnumFormatEtc::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) EnumFormatEtc::Release(void) {
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
STDMETHODIMP EnumFormatEtc::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IEnumFORMATETC || iid == IID_IUnknown) {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  
  *ppvObject = 0;
  return E_NOINTERFACE;
}

//
//  IEnumFORMATETC::Next
//
//  If the returned FORMATETC structure contains a non-null "ptd" member, then
//  the caller must free this using CoTaskMemFree (stated in the COM
//  documentation)
//
STDMETHODIMP EnumFormatEtc::Next(ULONG celt, FORMATETC *pFormatEtc, ULONG *pceltFetched) {
  ULONG copied  = 0;
  
  if(celt == 0 || pFormatEtc == 0)
    return E_INVALIDARG;
    
  while(index < src->formats.length() && copied < celt) {
    deep_copy_format_etc(&pFormatEtc[copied], &src->formats[index]);
    ++copied;
    ++index;
  }
  
  if(pceltFetched != 0)
    *pceltFetched = copied;
    
  return (copied == celt) ? S_OK : S_FALSE;
}

//
//  IEnumFORMATETC::Skip
//
STDMETHODIMP EnumFormatEtc::Skip(ULONG celt) {
  index += celt;
  return (index <= src->formats.length()) ? S_OK : S_FALSE;
}

//
//  IEnumFORMATETC::Reset
//
STDMETHODIMP EnumFormatEtc::Reset(void) {
  index = 0;
  return S_OK;
}

//
//  IEnumFORMATETC::Clone
//
STDMETHODIMP EnumFormatEtc::Clone(IEnumFORMATETC **ppEnumFormatEtc) {
  if(!ppEnumFormatEtc)
    return E_INVALIDARG;
    
  *ppEnumFormatEtc = new EnumFormatEtc(src);
  if(!*ppEnumFormatEtc)
    return E_OUTOFMEMORY;
    
  ((EnumFormatEtc*) *ppEnumFormatEtc)->index = index;
  return S_OK;
}

//} ... class EnumFormatEtc
