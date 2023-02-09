#ifndef RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_GRID_ITEM_PROVIDER_H__INCLUDED
#define RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_GRID_ITEM_PROVIDER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/frontendobject.h>
#include <uiautomationcore.h>

namespace richmath {
  class GridItem;
  
  class Win32UiaGridItemProvider:
      public IGridItemProvider,
      public ITableItemProvider
  {
      class Impl;
    protected:
      explicit Win32UiaGridItemProvider(FrontEndReference obj_ref);
      Win32UiaGridItemProvider(const Win32UiaGridItemProvider&) = delete;
      
      virtual ~Win32UiaGridItemProvider();
    
    public:
      static Win32UiaGridItemProvider *create(GridItem *box);
      
      //
      // IUnknown methods
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IGridItemProvider methods
      //
      STDMETHODIMP get_Row(int *pRetVal) override;
      STDMETHODIMP get_Column(int *pRetVal) override;
      STDMETHODIMP get_RowSpan(int *pRetVal) override;
      STDMETHODIMP get_ColumnSpan(int *pRetVal) override;
      STDMETHODIMP get_ContainingGrid(IRawElementProviderSimple **pRetVal) override;
      
      //
      // ITableItemProvider methods
      //
      STDMETHODIMP GetRowHeaderItems(SAFEARRAY **pRetVal) override;
      STDMETHODIMP GetColumnHeaderItems(SAFEARRAY **pRetVal) override;
      
    private:
      LONG refcount;
      FrontEndReference obj_ref;
  };
}

#endif // RICHMATH__GUI__WIN32__A11Y__WIN32_UIA_GRID_ITEM_PROVIDER_H__INCLUDED
