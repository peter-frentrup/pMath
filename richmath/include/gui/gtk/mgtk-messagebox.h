#ifndef RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/messagebox.h>
#include <gtk/gtk.h>

namespace richmath {
  class ControlContext;

  YesNoCancel mgtk_ask_save(Document *doc, String question);
  YesNoCancel mgtk_ask_remove_private_style_definitions(Document *doc);
  bool mgtk_ask_open_suspicious_system_file(String path);
  Expr mgtk_ask_interrupt(Expr stack);
  
  int mgtk_themed_dialog_run(ControlContext &ctx, GtkDialog *dialog);
  int mgtk_themed_dialog_run(GtkDialog *dialog);
}

#endif // RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED
