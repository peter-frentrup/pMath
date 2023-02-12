#include <gui/win32/a11y/win32-uia-selection-item-provider.h>

#include <boxes/radiobuttonbox.h>
#include <boxes/setterbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>

#include <uiautomation.h>


using namespace richmath;

namespace richmath {
  class Win32UiaSelectionItemProvider::Impl {
    public:
      explicit Impl(Win32UiaSelectionItemProvider &self);
      
      bool is_selected();
      
    private:
      Win32UiaSelectionItemProvider &self;
  };
}

//{ class Win32UiaSelectionItemProvider ...

Win32UiaSelectionItemProvider::Win32UiaSelectionItemProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaSelectionItemProvider::~Win32UiaSelectionItemProvider() {
}

Win32UiaSelectionItemProvider *Win32UiaSelectionItemProvider::create(SetterBox *setter) {
  if(!setter)
    return nullptr;
  
  return new Win32UiaSelectionItemProvider(setter->id());
}

Win32UiaSelectionItemProvider *Win32UiaSelectionItemProvider::create(RadioButtonBox *radio) {
  if(!radio)
    return nullptr;
  
  return new Win32UiaSelectionItemProvider(radio->id());
}

Win32UiaSelectionItemProvider *Win32UiaSelectionItemProvider::try_create(FrontEndObject *obj) {
  if(!obj)
    return nullptr;
  
  if(auto setter = dynamic_cast<SetterBox*>(     obj)) return create(setter);
  if(auto radio  = dynamic_cast<RadioButtonBox*>(obj)) return create(radio);
  
  return nullptr;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaSelectionItemProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_ISelectionItemProvider) {
    AddRef();
    *ppvObject = static_cast<ISelectionItemProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaSelectionItemProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaSelectionItemProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// ISelectionItemProvider::Select
//
STDMETHODIMP Win32UiaSelectionItemProvider::Select(void) {
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  FrontEndObject *obj = FrontEndObject::find(obj_ref);
  if(!obj)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
//  if(!dynamic_cast<SetterBox*>(obj) || !dynamic_cast<RadioButtonBox*>(obj)
//    return HRreport(UIA_E_INVALIDOPERATION);
  
  Application::click_button_async(obj_ref);
  return S_OK;
}

//
// ISelectionItemProvider::AddToSelection
//
STDMETHODIMP Win32UiaSelectionItemProvider::AddToSelection(void) {
  return HRreport(UIA_E_INVALIDOPERATION);
}

//
// ISelectionItemProvider::RemoveFromSelection
//
STDMETHODIMP Win32UiaSelectionItemProvider::RemoveFromSelection(void) {
  return HRreport(UIA_E_INVALIDOPERATION);
}

//
// ISelectionItemProvider::get_IsSelected
//
STDMETHODIMP Win32UiaSelectionItemProvider::get_IsSelected(BOOL *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = Impl(*this).is_selected();
  
  return S_OK;
}

//
// ISelectionItemProvider::get_SelectionContainer
//
STDMETHODIMP Win32UiaSelectionItemProvider::get_SelectionContainer(IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = nullptr;
  return S_OK;
}

//} ... class Win32UiaSelectionItemProvider

//{ class Win32UiaSelectionItemProvider::Impl ...

inline Win32UiaSelectionItemProvider::Impl::Impl(Win32UiaSelectionItemProvider &self)
: self{self}
{
}

bool Win32UiaSelectionItemProvider::Impl::is_selected() {
  FrontEndObject *obj = FrontEndObject::find(self.obj_ref);
  if(!obj)
    return false;
  
  if(auto simple = dynamic_cast<EmptyWidgetBox*>(obj)) {
    switch(simple->control_type()) {
      case ContainerType::OpenerTriangleOpened: return true;  // not really important, because OpenerBox does not implement SelectionItem pattern
      case ContainerType::OpenerTriangleClosed: return false; // dito.
      
      case ContainerType::CheckboxChecked:       return true;  // not really important, because CheckboxBox does not implement SelectionItem pattern
      case ContainerType::CheckboxUnchecked:     return false; // dito.
      case ContainerType::CheckboxIndeterminate: return false; // dito.
      
      case ContainerType::RadioButtonChecked:   return true;
      case ContainerType::RadioButtonUnchecked: return false; // dito.
      
  //    case ContainerType::ToggleSwitchChannelChecked:   return true;
  //    case ContainerType::ToggleSwitchThumbChecked:     return true;
  //    case ContainerType::ToggleSwitchChannelUnchecked: return false;
  //    case ContainerType::ToggleSwitchThumbUnchecked:   return false;
      
      default: return false;
    }
  }
  
  if(auto setter = dynamic_cast<SetterBox*>(obj))
    return setter->is_down();
  
  return false;
}

//} ... class Win32UiaSelectionItemProvider::Impl
