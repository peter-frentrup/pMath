#ifndef RICHMATH__GUI__WIN32__OLE__DROPTARGET_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__DROPTARGET_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/combase.h>

#include <shlobj.h>
#include <shobjidl.h>


namespace pmath {
  class String;
}

namespace richmath {
  class DropTarget: public IDropTarget {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override = 0;
      STDMETHODIMP_(ULONG) Release(void) override = 0;
      
      //
      // IDropTarget members
      //
      STDMETHODIMP DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragOver(DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragLeave() override;
      STDMETHODIMP Drop(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
    
    public:
      DropTarget();
      virtual ~DropTarget();
      
    protected:
      ComBase<IDropTargetHelper> _drop_target_helper;
      ComBase<IDataObject>       _dragging;
      DWORD                      _preferred_drop_effect;
      CLIPFORMAT                 _preferred_drop_format;
      bool                       _has_drag_image;
      bool                       _can_have_drop_descriptions;
      bool                       _did_set_drop_description;
    
    protected:
      virtual DWORD preferred_drop_effect(IDataObject *data_object);
      virtual DWORD drop_effect(DWORD key_state, POINTL pt, DWORD allowed_effects);
      virtual void apply_drop_description(DWORD effect, DWORD key_state, POINTL pt);
      virtual void do_drop_data(IDataObject *data_object, DWORD effect);
      virtual void position_drop_cursor(POINTL pt);
      void clear_drop_description();
      void set_drop_description(DROPIMAGETYPE image, const pmath::String &insert, const pmath::String &message);
      
      virtual HWND &hwnd() = 0;
      
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__DROPTARGET_H__INCLUDED
