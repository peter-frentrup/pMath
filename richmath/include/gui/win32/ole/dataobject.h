#ifndef RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <graphics/context.h>

#include <ole2.h>


namespace richmath {
  /* see  http://www.catch22.net/tuts/dragdrop
     and "The Shell Drag/Drop Helper Object Part 2: IDropSourceHelper -- Preparing Your IDataObject" 
      (https://msdn.microsoft.com/en-us/library/ms997502.aspx#ddhelp_pt2_topic2)
  */
  class DataObject : public IDataObject {
      friend class EnumFormatEtc;
      
      struct SavedData {
        FORMATETC  format_etc;
        STGMEDIUM  stg_medium;
      };
      
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
      
      void add_source_format(CLIPFORMAT cfFormat);
      
    private:
      HRESULT find_data_format_etc(const FORMATETC *format, struct SavedData **entry, bool add_missing);
      HRESULT add_ref_std_medium(const STGMEDIUM *stgm_in, STGMEDIUM *stgm_out, bool copy_from_external);
      
      bool has_source_format(const FORMATETC *pFormatEtc);
    
    public:
      SelectionReference  source;
      
    private:
      LONG refcount;
      
      Array<FORMATETC>    source_formats;
      SavedData          *data;
      int                 data_count;
    
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED
