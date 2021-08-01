#ifndef RICHMATH__GUI__WIN32__WIN32_DRAGDROPHANDLER_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_DRAGDROPHANDLER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/dragdrophandler.h>
#include <gui/win32/ole/combase.h>
#include <gui/win32/ole/dataobject.h>

namespace richmath {
  class Win32DragDropHandler final : public DragDropHandler {
    public:
      explicit Win32DragDropHandler(ComBase<IDataObject> data, DWORD allowed_effects);
    
      virtual MenuCommandStatus can_drop(DropAction action) override;
      virtual bool do_drop(DropAction action) override;
      
      static DWORD effect_from_action(DropAction action);
      
    public:
      DWORD *used_effect_ptr;
    private:
      ComBase<IDataObject> _data;
      DWORD                _allowed_effects;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_DRAGDROPHANDLER_H__INCLUDED
