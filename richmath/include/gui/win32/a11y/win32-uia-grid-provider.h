#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class GridBox;
  
  class Win32UiaGridProvider:
      public IGridProvider,
      public ITableProvider
  {
      class Impl;
    protected:
      explicit Win32UiaGridProvider(FrontEndReference obj_ref);
      Win32UiaGridProvider(const Win32UiaGridProvider&) = delete;
      
      virtual ~Win32UiaGridProvider();
    
    public:
      static Win32UiaGridProvider *create(GridBox *box);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IGridProvider methods
      //
      STDMETHODIMP GetItem(int row, int column, IRawElementProviderSimple **pRetVal) override;
      STDMETHODIMP get_RowCount(int *pRetVal) override;
      STDMETHODIMP get_ColumnCount(int *pRetVal) override;
      
      //
      // ITableProvider methods
      //
      STDMETHODIMP GetRowHeaders(SAFEARRAY **pRetVal) override;
      STDMETHODIMP GetColumnHeaders(SAFEARRAY **pRetVal) override;
      STDMETHODIMP get_RowOrColumnMajor(enum RowOrColumnMajor *pRetVal) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_TEXT_PROVIDER_H__INCLUDED
