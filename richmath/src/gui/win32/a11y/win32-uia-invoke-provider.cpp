#include <gui/win32/a11y/win32-uia-invoke-provider.h>

#include <boxes/buttonbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>

#include <uiautomation.h>


using namespace richmath;

//{ class Win32UiaInvokeProvider ...

Win32UiaInvokeProvider::Win32UiaInvokeProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaInvokeProvider::~Win32UiaInvokeProvider() {
}

Win32UiaInvokeProvider *Win32UiaInvokeProvider::create(AbstractButtonBox *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaInvokeProvider(box->id());
}

Win32UiaInvokeProvider *Win32UiaInvokeProvider::try_create(FrontEndObject *obj) {
  if(!obj)
    return nullptr;
  
  //if(dynamic_cast<SetterBox*>(obj)) return nullptr; // TODO: SetterBox implements SelectionItem pattern instead
  if(auto button = dynamic_cast<AbstractButtonBox*>(obj)) return create(button);
  
  return nullptr;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaInvokeProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IInvokeProvider) {
    AddRef();
    *ppvObject = static_cast<IInvokeProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaInvokeProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaInvokeProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// IInvokeProvider::Invoke
//
STDMETHODIMP Win32UiaInvokeProvider::Invoke(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  FrontEndObject *obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
//  if(!dynamic_cast<AbstractButtonBox*>(obj))
//    return HRreport(UIA_E_INVALIDOPERATION);
  
  Application::click_button_async(obj_ref);
  return S_OK;
}

//} ... class Win32UiaInvokeProvider
