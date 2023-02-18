#include <gui/win32/a11y/win32-uia-range-value-provider.h>

#include <boxes/progressindicatorbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>

#include <uiautomation.h>
#include <limits>


using namespace richmath;

//{ class Win32UiaRangeValueProvider ...

Win32UiaRangeValueProvider::Win32UiaRangeValueProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaRangeValueProvider::~Win32UiaRangeValueProvider() {
}

Win32UiaRangeValueProvider *Win32UiaRangeValueProvider::create(ProgressIndicatorBox *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaRangeValueProvider(box->id());
}

Win32UiaRangeValueProvider *Win32UiaRangeValueProvider::try_create(FrontEndObject *obj) {
  if(!obj)
    return nullptr;
  
  if(auto progress = dynamic_cast<ProgressIndicatorBox*>(obj)) return create(progress);
  
  return nullptr;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaRangeValueProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IRangeValueProvider) {
    AddRef();
    *ppvObject = static_cast<IRangeValueProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaRangeValueProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaRangeValueProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// IRangeValueProvider::SetValue
//
STDMETHODIMP Win32UiaRangeValueProvider::SetValue(double val) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return UIA_E_INVALIDOPERATION;
}

//
// IRangeValueProvider::get_Value
//
STDMETHODIMP Win32UiaRangeValueProvider::get_Value(double *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  auto obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(auto progress = dynamic_cast<ProgressIndicatorBox*>(obj)) {
    *pRetVal = progress->range_value();
    return S_OK;
  }
  
  return E_NOTIMPL;
}

//
// IRangeValueProvider::get_IsReadOnly
//
STDMETHODIMP Win32UiaRangeValueProvider::get_IsReadOnly(BOOL *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = TRUE;
  return S_OK;
}

//
// IRangeValueProvider::get_Maximum
//
STDMETHODIMP Win32UiaRangeValueProvider::get_Maximum(double *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  auto obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(auto progress = dynamic_cast<ProgressIndicatorBox*>(obj)) {
    *pRetVal = progress->range_interval().to;
    return S_OK;
  }
  
  return E_NOTIMPL;
}

//
// IRangeValueProvider::get_Minimum
//
STDMETHODIMP Win32UiaRangeValueProvider::get_Minimum(double *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  auto obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(auto progress = dynamic_cast<ProgressIndicatorBox*>(obj)) {
    *pRetVal = progress->range_interval().from;
    return S_OK;
  }
  
  return E_NOTIMPL;
}

//
// IRangeValueProvider::get_LargeChange
//
STDMETHODIMP Win32UiaRangeValueProvider::get_LargeChange(double *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = std::numeric_limits<double>::quiet_NaN();
  return S_OK;
}

//
// IRangeValueProvider::get_SmallChange
//
STDMETHODIMP Win32UiaRangeValueProvider::get_SmallChange(double *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = std::numeric_limits<double>::quiet_NaN();
  return S_OK;
}

//} ... class Win32UiaRangeValueProvider
