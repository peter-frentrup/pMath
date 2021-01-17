#ifndef RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/combase.h>

#include <shobjidl.h>

namespace richmath {
  class DropSource: public IDropSource {
      class Impl;
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IDropSource members
      //
      STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
      STDMETHODIMP GiveFeedback(DWORD dwEffect) override;
      
    public:
      DropSource();
      virtual ~DropSource();
      
      HRESULT set_flags(DWORD flags);
      HRESULT set_drag_image_from_window(HWND hwnd, const POINT *point = nullptr); // hwnd = NULL is allowed
      HRESULT set_drag_image_from_window_part(HWND hwnd, const RECT *rect, const POINT *point = nullptr); // hwnd = NULL means screen
      HRESULT set_drag_image_from_document(const Point &mouse, SelectionReference source);
            
      ComBase<IDataObject>  description_data;
      
    private:
      LONG                       refcount;
      ComBase<IDragSourceHelper> helper;
      bool                       must_set_cursor;
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED
