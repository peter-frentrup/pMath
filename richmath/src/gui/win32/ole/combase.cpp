#include <gui/win32/ole/combase.h>

using namespace richmath;

IUnknown *richmath::get_canonical_iunknown(IUnknown *punk) {
  IUnknown *punkCanonical;
  if(punk && SUCCEEDED(punk->QueryInterface(IID_IUnknown, (void**)&punkCanonical))) 
    punkCanonical->Release();
  else 
    punkCanonical = punk;
  
  return punkCanonical;
}
