#include <gui/win32/a11y/win32-uia-toggle-provider.h>

#include <boxes/checkboxbox.h>
#include <boxes/openerbox.h>
#include <boxes/setterbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>

#include <uiautomation.h>


using namespace richmath;

namespace richmath {
  class Win32UiaToggleProvider::Impl {
    public:
      explicit Impl(Win32UiaToggleProvider &self);
      
      static ToggleState toggle_state_from_control(ContainerType type);
      
    private:
      Win32UiaToggleProvider &self;
  };
}

//{ class Win32UiaToggleProvider ...

Win32UiaToggleProvider::Win32UiaToggleProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaToggleProvider::~Win32UiaToggleProvider() {
}

Win32UiaToggleProvider *Win32UiaToggleProvider::create(CheckboxBox *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaToggleProvider(box->id());
}

Win32UiaToggleProvider *Win32UiaToggleProvider::create(OpenerBox *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaToggleProvider(box->id());
}

Win32UiaToggleProvider *Win32UiaToggleProvider::try_create(FrontEndObject *obj) {
  if(!obj)
    return nullptr;
  
  if(auto checkbox = dynamic_cast<CheckboxBox*>(obj))  return create(checkbox);
  if(auto opener   = dynamic_cast<OpenerBox*>(obj))    return create(opener);// TODO: should support ExpandCollapse pattern instead (or in addition?)
  
  return nullptr;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaToggleProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IToggleProvider) {
    AddRef();
    *ppvObject = static_cast<IToggleProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaToggleProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaToggleProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// IToggleProvider::Toggle
//
STDMETHODIMP Win32UiaToggleProvider::Toggle(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  FrontEndObject *obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
//  if(!dynamic_cast<SetterBox*>(obj) || !dynamic_cast<CheckboxBox*>(obj) || !dynamic_cast<OpenerBox*>(obj))
//    return HRreport(UIA_E_INVALIDOPERATION);
  
  Application::click_button_async(obj_ref);
  return S_OK;
}

//
// IToggleProvider::get_ToggleState
//
STDMETHODIMP Win32UiaToggleProvider::get_ToggleState(enum ToggleState *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  FrontEndObject *obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(auto simple = dynamic_cast<EmptyWidgetBox*>(obj)) {
    *pRetVal = Impl::toggle_state_from_control(simple->control_type());
    return S_OK;
  }
  else if(auto setter = dynamic_cast<SetterBox*>(obj)) {
    *pRetVal = setter->is_down() ? ToggleState_On : ToggleState_Off;
    return S_OK;
  }
//  else if(auto widget = dynamic_cast<ContainerWidgetBox*>(obj)) {
//    *pRetVal = Impl::toggle_state_from_control(widget->control_type());
//    return S_OK;
//  }
  
  *pRetVal = ToggleState_Indeterminate;
  return S_OK;
}

//} ... class Win32UiaToggleProvider

//{ class Win32UiaToggleProvider::Impl ...

inline Win32UiaToggleProvider::Impl::Impl(Win32UiaToggleProvider &self)
: self{self}
{
}

ToggleState Win32UiaToggleProvider::Impl::toggle_state_from_control(ContainerType type) {
  switch(type) {
    case ContainerType::OpenerTriangleOpened: return ToggleState_On;
    case ContainerType::OpenerTriangleClosed: return ToggleState_Off;
    
    case ContainerType::CheckboxChecked:       return ToggleState_On;
    case ContainerType::CheckboxUnchecked:     return ToggleState_Off;
    case ContainerType::CheckboxIndeterminate: return ToggleState_Indeterminate;
    
    case ContainerType::RadioButtonChecked:   return ToggleState_On;  // not really important, because RadioButtonBox does not implement Toggle pattern
    case ContainerType::RadioButtonUnchecked: return ToggleState_Off; // dito.
    
//    case ContainerType::ToggleSwitchChannelChecked:   return ToggleState_On;
//    case ContainerType::ToggleSwitchThumbChecked:     return ToggleState_On;
//    case ContainerType::ToggleSwitchChannelUnchecked: return ToggleState_Off;
//    case ContainerType::ToggleSwitchThumbUnchecked:   return ToggleState_Off;
    
    default: return ToggleState_Indeterminate;
  }
}

//} ... class Win32UiaToggleProvider::Impl
