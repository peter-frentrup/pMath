#ifndef RICHMATH__GUI__GTK__MGTK_CLIPBOARD_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_CLIPBOARD_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/clipboard.h>

#include <gtk/gtk.h>


namespace richmath {
  class MathGtkClipboard: public Clipboard {
    public:
      static MathGtkClipboard obj;
      
    public:
      MathGtkClipboard();
      virtual ~MathGtkClipboard();
      
      virtual bool has_format(String mimetype) override;
      
      virtual ReadableBinaryFile read_as_binary_file(String mimetype) override;
      virtual String             read_as_text(String mimetype) override;
      virtual Expr               read_as_filenames() override;
      
      virtual SharedPtr<OpenedClipboard> open_write() override;
      virtual cairo_surface_t *create_image(String mimetype, double width, double height) override;
      
      static GdkAtom mimetype_to_atom(String mimetype);
      static void add_to_target_list(GtkTargetList *targets, String mimetype, int info);
      
      GtkClipboard *clipboard();
      
    private:
      GtkClipboard *_clipboard;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_CLIPBOARD_H__INCLUDED
