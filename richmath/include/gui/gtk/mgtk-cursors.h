#ifndef __GUI__GTK__MGTK_CURSORS_H__
#define __GUI__GTK__MGTK_CURSORS_H__

#ifndef RICHMATH_USE_GTK_GUI
#error this header is gtk specific
#endif

#include <gui/native-widget.h>

#include <gdk/gdk.h>


namespace richmath {
  class MathGtkCursors: public Base {
    public:
      MathGtkCursors();
      ~MathGtkCursors();
      
      GdkCursor *get_gdk_cursor(CursorType type); // caller must gdk_cursor_unref it
  };
}

#endif // __GUI__GTK__MGTK_CURSORS_H__
