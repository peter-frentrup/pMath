#ifndef __GUI__WIN32__OLE__DATAOBJECT_H__
#define __GUI__WIN32__OLE__DATAOBJECT_H__

#include <graphics/context.h>

#include <ole2.h>

namespace richmath{
  // see  http://www.catch22.net/tuts/dragdrop
  class DataObject : public IDataObject {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject);
      STDMETHODIMP_(ULONG) AddRef(        void);
      STDMETHODIMP_(ULONG) Release(       void);

      //
      // IDataObject members
      //
      STDMETHODIMP GetData(              FORMATETC *pFormatEtc, STGMEDIUM *pMedium);
      STDMETHODIMP GetDataHere(          FORMATETC *pFormatEtc, STGMEDIUM *pMedium);
      STDMETHODIMP QueryGetData(         FORMATETC *pFormatEtc);
      STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *pFormatEct,  FORMATETC *pFormatEtcOut);
      STDMETHODIMP SetData(              FORMATETC *pFormatEtc,  STGMEDIUM *pMedium, BOOL fRelease);
      STDMETHODIMP EnumFormatEtc(        DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc);
      STDMETHODIMP DAdvise(              FORMATETC *pFormatEtc,  DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
      STDMETHODIMP DUnadvise(            DWORD dwConnection);
      STDMETHODIMP EnumDAdvise(          IEnumSTATDATA **ppEnumAdvise);
      
    public:
      DataObject();
      ~DataObject();

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
