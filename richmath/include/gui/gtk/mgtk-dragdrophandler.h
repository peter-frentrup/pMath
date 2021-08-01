#ifndef RICHMATH__GUI__GTK__MGTK_DRAGDROPHANDLER_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_DRAGDROPHANDLER_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/dragdrophandler.h>
#include <gtk/gtk.h>


namespace richmath {
  class MathGtkDragDropHandler final : public DragDropHandler {
    public:
      explicit MathGtkDragDropHandler(GdkDragContext *context);
      virtual ~MathGtkDragDropHandler();
    
      virtual MenuCommandStatus can_drop(DropAction action) override;
      virtual bool do_drop(DropAction action) override;
      
      static GdkDragAction native_action(DropAction action);
      
    private:
      GdkDragContext *_context;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_DRAGDROPHANDLER_H__INCLUDED
