#include <gui/win32/win32-dragdrophandler.h>

#include <gui/win32/win32-widget.h>
#include <gui/documents.h>


using namespace richmath;

//{ class Win32DragDropHandler ...

Win32DragDropHandler::Win32DragDropHandler(
  ComBase<IDataObject> data, 
  DWORD allowed_effects) 
: DragDropHandler(),
  used_effect_ptr{nullptr},
  _data{data},
  _allowed_effects{allowed_effects}
{
}

MenuCommandStatus Win32DragDropHandler::can_drop(DropAction action) {
  if(!_data)
    return false;
  
  Document *doc = Documents::selected_document();
  if(!doc)
    return false;
  
  auto *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return false;
  
  return 0 != (_allowed_effects & effect_from_action(action));
}

bool Win32DragDropHandler::do_drop(DropAction action) {
  if(!_data)
    return false;
  
  Document *doc = Documents::selected_document();
  if(!doc)
    return false;
  
  auto *wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return false;
  
  DWORD effect = effect_from_action(action);
  wid->do_drop_data(_data.get(), effect);
  _data.reset();
  if(used_effect_ptr)
    *used_effect_ptr = effect;
  return true;
}

DWORD Win32DragDropHandler::effect_from_action(DropAction action) {
  switch(action) {
    case DropAction::Copy : return DROPEFFECT_COPY;
    case DropAction::Move : return DROPEFFECT_MOVE;
    case DropAction::Link : return DROPEFFECT_LINK;
  }
  return DROPEFFECT_NONE;
}

//} ... class Win32DragDropHandler
