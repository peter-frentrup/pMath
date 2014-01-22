#ifndef __GUI__WIN32__OLE__ENUMFORMATETC_H__
#define __GUI__WIN32__OLE__ENUMFORMATETC_H__

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
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject);
      STDMETHODIMP_(ULONG) AddRef(void);
      STDMETHODIMP_(ULONG) Release(void);
      
      //
      // IEnumFormatEtc members
      //
      STDMETHODIMP Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
      STDMETHODIMP Skip(ULONG celt);
      STDMETHODIMP Reset(void);
      STDMETHODIMP Clone(IEnumFORMATETC **ppEnumFormatEtc);
      
    public:
      EnumFormatEtc(DataObject *_src);
      virtual ~EnumFormatEtc();
      
    private:
      LONG        refcount;
      int         index;
      DataObject *src;
  };
}

#endif // __GUI__WIN32__OLE__ENUMFORMATETC_H__
