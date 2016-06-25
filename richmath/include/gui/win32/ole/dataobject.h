#ifndef __GUI__WIN32__OLE__DATAOBJECT_H__
#define __GUI__WIN32__OLE__DATAOBJECT_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <graphics/context.h>

#include <ole2.h>


namespace richmath {
  // see  http://www.catch22.net/tuts/dragdrop
  class DataObject : public IDataObject {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IDataObject members
      //
      STDMETHODIMP GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) override;
      STDMETHODIMP GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium) override;
      STDMETHODIMP QueryGetData(FORMATETC *pFormatEtc) override;
      STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *pFormatEct,  FORMATETC *pFormatEtcOut) override;
      STDMETHODIMP SetData(FORMATETC *pFormatEtc,  STGMEDIUM *pMedium, BOOL fRelease) override;
      STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc) override;
      STDMETHODIMP DAdvise(FORMATETC *pFormatEtc,  DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) override;
      STDMETHODIMP DUnadvise(DWORD dwConnection) override;
      STDMETHODIMP EnumDAdvise(IEnumSTATDATA **ppEnumAdvise) override;
      
    public:
      DataObject();
      virtual ~DataObject();
      
    public:
      int lookup_format_etc(FORMATETC *pFormatEtc);
      
      Array<FORMATETC>    formats;   // all for HGLOBAL
      Array<String>       mimetypes; // same length as formats!
      SelectionReference  source;
      
    private:
      LONG refcount;
  };
}

#endif // __GUI__WIN32__OLE__DATAOBJECT_H__
