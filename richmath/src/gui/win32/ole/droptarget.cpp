#include <gui/win32/ole/droptarget.h>

#include <gui/win32/win32-clipboard.h>
#include <gui/win32/ole/dataobject.h>


using namespace richmath;

//{ class DropTarget ...

DropTarget::DropTarget() 
  : _preferred_drop_effect(DROPEFFECT_NONE),
    _preferred_drop_format(0),
    _has_drag_image(false),
    _can_have_drop_descriptions(false),
    _did_set_drop_description(false),
    _right_mouse_drag(false)
{
  CoCreateInstance(
    CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, 
    _drop_target_helper.iid(),
    (void**)_drop_target_helper.get_address_of());
  
}

DropTarget::~DropTarget() {
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP DropTarget::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IDropTarget || iid == IID_IUnknown) {
    AddRef();
    *ppvObject = this;
    return S_OK;
  }
  
  *ppvObject = 0;
  return E_NOINTERFACE;
}

//
// IDropTarget::DragEnter
//
STDMETHODIMP DropTarget::DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
  _dragging.copy(data_object);
  
  _has_drag_image = 0 != DataObject::get_global_data_dword(data_object, Win32Clipboard::Formats::DragWindow);
  _can_have_drop_descriptions = false;
  _did_set_drop_description = false;
  
  DWORD flags = DataObject::get_global_data_dword(data_object, Win32Clipboard::Formats::DragSourceHelperFlags);
  _can_have_drop_descriptions = (flags & DSH_ALLOWDROPDESCRIPTIONTEXT) != 0;
  
  _preferred_drop_effect = preferred_drop_effect(data_object);
  _right_mouse_drag = (key_state & MK_RBUTTON) != 0;
  
  if(_preferred_drop_effect != DROPEFFECT_NONE) {
    *effect = drop_effect(key_state, pt, *effect);
    position_drop_cursor(pt);
    if(*effect != DROPEFFECT_NONE)
      apply_drop_description(*effect, key_state, pt);
  }
  else
    *effect = DROPEFFECT_NONE;
  
  if(*effect == DROPEFFECT_NONE || !_did_set_drop_description)
    clear_drop_description();
  
  if(_drop_target_helper) {
    POINT small_pt = {pt.x, pt.y};
    _drop_target_helper->DragEnter(hwnd(), data_object, &small_pt, *effect);
  }
  
  return S_OK;
}

//
// IDropTarget::DragOver
//
STDMETHODIMP DropTarget::DragOver(DWORD key_state, POINTL pt, DWORD *effect) {
  _did_set_drop_description = false;
  
  _right_mouse_drag = (key_state & MK_RBUTTON) != 0;
  if(_preferred_drop_effect != DROPEFFECT_NONE) {
    *effect = drop_effect(key_state, pt, *effect);
    position_drop_cursor(pt);
    if(*effect != DROPEFFECT_NONE)
      apply_drop_description(*effect, key_state, pt);
  }
  else
    *effect = DROPEFFECT_NONE;
  
  if(_can_have_drop_descriptions) {
    if(*effect == DROPEFFECT_NONE || !_did_set_drop_description)
      clear_drop_description();
  }
  
  if(_drop_target_helper) {
    POINT small_pt = {pt.x, pt.y};
    _drop_target_helper->DragOver(&small_pt, *effect);
  }
  
  return S_OK;
}

//
// IDropTarget::DragLeave
//
STDMETHODIMP DropTarget::DragLeave() {
  clear_drop_description();
  
  if(_drop_target_helper) 
    _drop_target_helper->DragLeave();
  
  _has_drag_image = false;
  _can_have_drop_descriptions = false;
  
  _dragging.reset();
  return S_OK;
}

//
// IDropTarget::Drop
//
STDMETHODIMP DropTarget::Drop(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
  DWORD allowed_effects = *effect;
  
  position_drop_cursor(pt);
  
  if(_preferred_drop_effect != DROPEFFECT_NONE) 
    *effect = drop_effect(key_state, pt, allowed_effects);
  else 
    *effect = _preferred_drop_effect;
  
  if(_drop_target_helper) {
    POINT small_pt = {pt.x, pt.y};
    _drop_target_helper->Drop(data_object, &small_pt, *effect);
  }
  
  if(_right_mouse_drag) { // (key_state & MK_RBUTTON) is already 0 (mouse was released)
    *effect = ask_drop_effect(data_object, pt, *effect, allowed_effects);
  }
  
  if(*effect != DROPEFFECT_NONE) {
    do_drop_data(data_object, *effect);
  }
    
  _dragging.reset();
  return S_OK;
}

DWORD DropTarget::preferred_drop_effect(IDataObject *data_object) {
  _preferred_drop_format = 0;
  return DROPEFFECT_NONE;
}

DWORD DropTarget::drop_effect(DWORD key_state, POINTL pt, DWORD allowed_effects) {
  DWORD effect = _preferred_drop_effect & allowed_effects;
  
  if(key_state & MK_CONTROL) {
    effect = allowed_effects & DROPEFFECT_COPY;
  }
  else if(key_state & MK_SHIFT) {
    effect = allowed_effects & DROPEFFECT_MOVE;
  }
  else if(key_state & MK_ALT) {
    effect = allowed_effects & DROPEFFECT_LINK;
  }
  
  if(effect == 0) {
    if(allowed_effects & DROPEFFECT_COPY) effect = DROPEFFECT_COPY;
    if(allowed_effects & DROPEFFECT_MOVE) effect = DROPEFFECT_MOVE;
  }
  
  return effect;
}

DWORD DropTarget::ask_drop_effect(IDataObject *data_object, POINTL pt, DWORD effect, DWORD allowed_effects) {
  return effect;
}

void DropTarget::apply_drop_description(DWORD effect, DWORD key_state, POINTL pt) {
}

void DropTarget::do_drop_data(IDataObject *data_object, DWORD effect) {
}

void DropTarget::position_drop_cursor(POINTL pt) {
}

void DropTarget::clear_drop_description() {
//  if(_did_set_drop_description) {
//    pmath_debug_print("[already _did_set_drop_description]");
//    return;
//  }
  _did_set_drop_description = true;
  DataObject::clear_drop_description(_dragging.get());
}

void DropTarget::set_drop_description(DROPIMAGETYPE image, const String &insert, const String &message) {
  if(_did_set_drop_description) {
    pmath_debug_print("[already _did_set_drop_description]");
    return;
  }
  _did_set_drop_description = true;
  if(_can_have_drop_descriptions)
    DataObject::set_drop_description(_dragging.get(), image, insert, message, true);
  else
    DataObject::set_drop_description(_dragging.get(), image, String(), String(), image > DROPIMAGE_LINK);
}

//} ... class DropTarget
