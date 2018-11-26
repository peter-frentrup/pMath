#ifndef RICHMATH__GUI__WIN32__OLE__ENUMFORMATETC_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__ENUMFORMATETC_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/dataobject.h>


namespace richmath {
  // see  http://www.catch22.net/tuts/dragdrop
  class EnumFormatEtc : public IEnumFORMATETC {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IEnumFormatEtc members
      //
      STDMETHODIMP Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched) override;
      STDMETHODIMP Skip(ULONG celt) override;
      STDMETHODIMP Reset(void) override;
      STDMETHODIMP Clone(IEnumFORMATETC **ppEnumFormatEtc) override;
      
    public:
      EnumFormatEtc(DataObject *_src);
      virtual ~EnumFormatEtc();
      
    private:
      LONG        refcount;
      int         index;
      DataObject *src;
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__ENUMFORMATETC_H__INCLUDED
