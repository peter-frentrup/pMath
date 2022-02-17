#include <gui/gtk/mgtk-dragdrophandler.h>
#include <gui/gtk/mgtk-widget.h>

#include <gui/documents.h>


using namespace richmath;

//{ class MathGtkDragDropHandler ...

MathGtkDragDropHandler::MathGtkDragDropHandler(GdkDragContext *context)
: DragDropHandler(),
 _context(context)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  g_object_ref(context);
}

MathGtkDragDropHandler::~MathGtkDragDropHandler() {
  if(_context) {
    gtk_drag_finish(_context, FALSE, FALSE, 0);
    g_object_unref(_context);
    _context = nullptr;
  }
}

MenuCommandStatus MathGtkDragDropHandler::can_drop(DropAction action) {
  if(!_context)
    return false;
  
  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  auto *wid = dynamic_cast<MathGtkWidget*>(doc->native());
  if(!wid)
    return false;
  
  GdkDragAction allowed = gdk_drag_context_get_actions(_context);
  return 0 != (allowed & native_action(action));
}

bool MathGtkDragDropHandler::do_drop(DropAction action) {
  if(!_context)
    return false;

  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  auto *wid = dynamic_cast<MathGtkWidget*>(doc->native());
  if(!wid)
    return false;
  
  GdkAtom target = gtk_drag_dest_find_target(wid->widget(), _context, nullptr);
  if(target == GDK_NONE)
    return false;
  
  wid->do_drop_data(_context, native_action(action), target, 0);
  g_object_unref(_context);
  _context = nullptr;
  return true;
}

GdkDragAction MathGtkDragDropHandler::native_action(DropAction action) {
  switch(action) {
    case DropAction::Copy: return GDK_ACTION_COPY;
    case DropAction::Move: return GDK_ACTION_MOVE;
    case DropAction::Link: return GDK_ACTION_LINK;
  }
  return (GdkDragAction)0;
}

//} ... class MathGtkDragDropHandler
