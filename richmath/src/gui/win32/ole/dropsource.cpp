#include <gui/win32/ole/dropsource.h>

using namespace richmath;

//{ class DropSource ...

DropSource::DropSource(){
  m_lRefCount = 1;
}

DropSource::~DropSource(){
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) DropSource::AddRef(void){
  return InterlockedIncrement(&m_lRefCount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) DropSource::Release(void){
  LONG count = InterlockedDecrement(&m_lRefCount);

  if(count == 0){
    delete this;
    return 0;
  }
  
  return count;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP DropSource::QueryInterface(REFIID iid, void **ppvObject){
  if(iid == IID_IDropSource || iid == IID_IUnknown){
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
STDMETHODIMP DropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState){
  if(fEscapePressed == TRUE)
    return DRAGDROP_S_CANCEL;  

  if((grfKeyState & MK_LBUTTON) == 0)
    return DRAGDROP_S_DROP;

  return S_OK;
}

//
//  IDropSource::GiveFeedback
//
STDMETHODIMP DropSource::GiveFeedback(DWORD dwEffect){
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

//} ... class DropSource
