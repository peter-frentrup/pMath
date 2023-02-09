#include <gui/win32/a11y/win32-uia-grid-item-provider.h>

#include <boxes/gridbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>
#include <gui/win32/a11y/win32-uia-box-provider.h>

#include <uiautomation.h>


using namespace richmath;

//{ class Win32UiaGridItemProvider ...

Win32UiaGridItemProvider::Win32UiaGridItemProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaGridItemProvider::~Win32UiaGridItemProvider() {
}

Win32UiaGridItemProvider *Win32UiaGridItemProvider::create(GridItem *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaGridItemProvider(box->id());
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaGridItemProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IGridItemProvider) {
    AddRef();
    *ppvObject = static_cast<IGridItemProvider*>(this);
    return S_OK;
  }
  if(iid == IID_ITableItemProvider) {
    AddRef();
    *ppvObject = static_cast<ITableItemProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaGridItemProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaGridItemProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// IGridItemProvider::get_Row
//
STDMETHODIMP Win32UiaGridItemProvider::get_Row(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridItem *item = FrontEndObject::find_cast<GridItem>(obj_ref);
  if(!item)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridYIndex row;
  GridXIndex col;
  item->grid()->index_to_yx(item->index(), &row, &col);
  
  *pRetVal = row.primary_value();
  return S_OK;
}

//
// IGridItemProvider::get_Column
//
STDMETHODIMP Win32UiaGridItemProvider::get_Column(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridItem *item = FrontEndObject::find_cast<GridItem>(obj_ref);
  if(!item)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridYIndex row;
  GridXIndex col;
  item->grid()->index_to_yx(item->index(), &row, &col);
  
  *pRetVal = col.primary_value();
  return S_OK;
}

//
// IGridItemProvider::get_RowSpan
//
STDMETHODIMP Win32UiaGridItemProvider::get_RowSpan(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridItem *item = FrontEndObject::find_cast<GridItem>(obj_ref);
  if(!item)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = 1 + item->span_down();
  return S_OK;
}

//
// IGridItemProvider::get_ColumnSpan
//
STDMETHODIMP Win32UiaGridItemProvider::get_ColumnSpan(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridItem *item = FrontEndObject::find_cast<GridItem>(obj_ref);
  if(!item)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = 1 + item->span_right();
  return S_OK;
}

//
// IGridItemProvider::get_ContainingGrid
//
STDMETHODIMP Win32UiaGridItemProvider::get_ContainingGrid(IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridItem *item = FrontEndObject::find_cast<GridItem>(obj_ref);
  if(!item)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = Win32UiaBoxProvider::create(item->grid());
  if(!*pRetVal)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  return S_OK;
}

//
// ITableItemProvider::GetRowHeaderItems
//
STDMETHODIMP Win32UiaGridItemProvider::GetRowHeaderItems(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
  if(!*pRetVal)
    return E_OUTOFMEMORY;
  
  return S_OK;
}

//
// ITableItemProvider::GetColumnHeaderItems
//
STDMETHODIMP Win32UiaGridItemProvider::GetColumnHeaderItems(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
  if(!*pRetVal)
    return E_OUTOFMEMORY;
  
  return S_OK;
}

//} ... class Win32UiaGridItemProvider
