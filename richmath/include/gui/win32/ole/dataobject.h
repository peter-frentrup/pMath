#ifndef RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600 /* DROPDESCRIPTION */
#endif

#include <graphics/context.h>
#include <util/pmath-extra.h>

#include <cairo-win32.h>
#include <ole2.h>
#include <shlobj.h>


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
      
      HRESULT do_drag_drop(IDropSource *pDropSource, DWORD dwOKEffects, DWORD *pdwEffect);

      void add_source_format(CLIPFORMAT cfFormat, TYMED tymed = TYMED_HGLOBAL);
      void add_source_format(const FORMATETC &formatEtc) { source_formats.add(formatEtc); }
      
      static HRESULT buffer_to_medium(const FORMATETC *pFormatEtc, STGMEDIUM *pMedium, const uint8_t *data, size_t len);
      static HRESULT buffer_to_medium(const FORMATETC *pFormatEtc, STGMEDIUM *pMedium, pmath::BinaryFile binbuffer);
      static HRESULT image_to_medium(const FORMATETC *pFormatEtc, STGMEDIUM *pMedium, cairo_surface_t *image);
      
      static cairo_surface_t *try_create_image(const FORMATETC *pFormatEtc, cairo_format_t img_format, double width, double height);
      
      static void clear_drop_description(IDataObject *obj);
      static bool clear_drop_description(DROPDESCRIPTION *desc);
      
      // e.g. message = "Move to %1", insertion = "Documents"
      static void set_drop_description(IDataObject *obj, DROPIMAGETYPE image, const String &message, const String &insertion, bool create);
      
      static HRESULT make_ref_counted(STGMEDIUM *pMedium);
      
    private:
      HRESULT find_data_format_etc(const FORMATETC *format, struct SavedData **entry, bool add_missing);
      
      bool has_source_format(const FORMATETC *pFormatEtc);
    
    public:
      SelectionReference  source;
      Expr                source_content;
      
      static DataObject *as_current_data_object(IDataObject *obj);
      
    private:
      LONG refcount;
      
      Array<FORMATETC>    source_formats;
      SavedData          *data;
      int                 data_count;
      
    public:
      static HRESULT get_global_data(IDataObject *obj, CLIPFORMAT format, FORMATETC *formatEtc, STGMEDIUM *medium, size_t min_size);
      static DWORD   get_global_data_dword(IDataObject *obj, CLIPFORMAT format);
      static HRESULT set_global_data(IDataObject *obj, CLIPFORMAT format, void *data, size_t size);
      static HRESULT set_global_data_dword(IDataObject *obj, CLIPFORMAT format, DWORD value);
      static Expr    get_global_data_dropfiles(HGLOBAL hglb);
      static Expr    get_global_data_dropfiles(IDataObject *obj);
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__DATAOBJECT_H__INCLUDED
