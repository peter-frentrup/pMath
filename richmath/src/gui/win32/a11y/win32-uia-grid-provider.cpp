#include <gui/win32/a11y/win32-uia-grid-provider.h>

#include <boxes/gridbox.h>
#include <eval/application.h>

#include <gui/win32/ole/combase.h>
#include <gui/win32/a11y/win32-uia-box-provider.h>

#include <uiautomation.h>


using namespace richmath;

//{ class Win32UiaGridProvider ...

Win32UiaGridProvider::Win32UiaGridProvider(FrontEndReference obj_ref)
  : refcount(1),
    obj_ref(obj_ref)
{
}

Win32UiaGridProvider::~Win32UiaGridProvider() {
}

Win32UiaGridProvider *Win32UiaGridProvider::create(GridBox *box) {
  if(!box)
    return nullptr;
  
  return new Win32UiaGridProvider(box->id());
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP Win32UiaGridProvider::QueryInterface(REFIID iid, void **ppvObject) {
  if(!ppvObject)
    return HRreport(E_INVALIDARG);
  
  if(iid == IID_IUnknown || iid == IID_IGridProvider) {
    AddRef();
    *ppvObject = static_cast<IGridProvider*>(this);
    return S_OK;
  }
  if(iid == IID_ITableProvider) {
    AddRef();
    *ppvObject = static_cast<ITableProvider*>(this);
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) Win32UiaGridProvider::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) Win32UiaGridProvider::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
// IGridProvider::GetItem
//
STDMETHODIMP Win32UiaGridProvider::GetItem(int row, int column, IRawElementProviderSimple **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridBox *grid = FrontEndObject::find_cast<GridBox>(obj_ref);
  if(!grid)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  if(row < 0 || row >= grid->rows() || column < 0 || column >= grid->cols())
    return HRreport(E_INVALIDARG);
  
  *pRetVal = Win32UiaBoxProvider::create(grid->item(row, column));
  return S_OK;
}

//
// IGridProvider::get_RowCount
//
STDMETHODIMP Win32UiaGridProvider::get_RowCount(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridBox *grid = FrontEndObject::find_cast<GridBox>(obj_ref);
  if(!grid)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = grid->rows();
  return S_OK;
}

//
// IGridProvider::get_ColumnCount
//
STDMETHODIMP Win32UiaGridProvider::get_ColumnCount(int *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  if(!Application::is_running_on_gui_thread())
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  GridBox *grid = FrontEndObject::find_cast<GridBox>(obj_ref);
  if(!grid)
    return HRreport(UIA_E_ELEMENTNOTAVAILABLE);
  
  *pRetVal = grid->cols();
  return S_OK;
}

//
// ITableProvider::GetRowHeaders
//
STDMETHODIMP Win32UiaGridProvider::GetRowHeaders(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
  if(!*pRetVal)
    return E_OUTOFMEMORY;
  
  return S_OK;
}

//
// ITableProvider::GetColumnHeaders
//
STDMETHODIMP Win32UiaGridProvider::GetColumnHeaders(SAFEARRAY **pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
  if(!*pRetVal)
    return E_OUTOFMEMORY;
  
  return S_OK;
}

//
// ITableProvider::get_RowOrColumnMajor
//
STDMETHODIMP Win32UiaGridProvider::get_RowOrColumnMajor(enum RowOrColumnMajor *pRetVal) {
  if(!pRetVal)
    return HRreport(E_INVALIDARG);
  
  *pRetVal = RowOrColumnMajor_Indeterminate;
  return S_OK;
}

//} ... class Win32UiaGridProvider
