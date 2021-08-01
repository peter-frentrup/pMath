#include <gui/dragdrophandler.h>


using namespace richmath;

namespace richmath {
  class DragDropHandler::Impl {
    public:
      Impl(DragDropHandler &self) : self(self) {}
      
      static void register_menu_commands();
      static MenuCommandStatus can_drop(DropAction action);
      static bool do_drop(DropAction action); 
      
    private:
      DragDropHandler &self;
    
    public:
      static DragDropHandler *current;
  };
}

namespace richmath { namespace strings {
  extern String DropCopyHere;
  extern String DropMoveHere;
  extern String DropLinkHere;
}}

//{ class DragDropHandler ...

DragDropHandler::DragDropHandler() {
  Impl::current = this;
  Impl::register_menu_commands();
}

DragDropHandler::~DragDropHandler() {
  if(Impl::current == this)
    Impl::current = nullptr;
}

//} ... class DragDropHandler

//{ class DragDropHandler::Impl ...

DragDropHandler *DragDropHandler::Impl::current = nullptr;

void DragDropHandler::Impl::register_menu_commands() {
  static bool initialized = false;
  if(initialized)
    return;
    
  initialized = true;
  Menus::register_command(
    strings::DropCopyHere, 
    [](Expr) { return do_drop(DropAction::Copy); }, 
    [](Expr) { return can_drop(DropAction::Copy); } );
  Menus::register_command(
    strings::DropMoveHere, 
    [](Expr) { return do_drop(DropAction::Move); }, 
    [](Expr) { return can_drop(DropAction::Move); } );
  Menus::register_command(
    strings::DropLinkHere, 
    [](Expr) { return do_drop(DropAction::Link); }, 
    [](Expr) { return can_drop(DropAction::Link); } );
}

MenuCommandStatus DragDropHandler::Impl::can_drop(DropAction action) {
  if(current) {
    current->ref();
    SharedPtr<DragDropHandler> tmp = current;
    return tmp->can_drop(action);
  }
  
  return false;
}

bool DragDropHandler::Impl::do_drop(DropAction action) {
  if(current) {
    current->ref();
    SharedPtr<DragDropHandler> tmp = current;
    return tmp->do_drop(action);
  }
  
  return false;
}

//} ... class DragDropHandler::Impl
