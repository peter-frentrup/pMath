#ifndef __GUI__GTK__MGTK_FILEDIALOG_H__
#define __GUI__GTK__MGTK_FILEDIALOG_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>
//#include <gtk/gtk.h>

typedef struct _GtkFileChooserDialog  GtkFileChooserDialog;
typedef struct _GtkFileChooser        GtkFileChooser;

namespace richmath {
  class MathGtkFileDialog: public Base {
    friend class MathGtkFileDialogImpl;
    public:
      MathGtkFileDialog(bool to_save);
      ~MathGtkFileDialog();
      
      void set_title(pmath::String title);
      void set_filter(pmath::Expr filter);
      void set_initial_file(pmath::String initialfile);
      
      pmath::Expr show_dialog();
      
    private:
      GtkFileChooserDialog *_dialog;
      GtkFileChooser       *_chooser;
      
    public:
      static pmath::Expr show(
        bool           save,
        pmath::String  initialfile,
        pmath::Expr    filter,
        pmath::String  title);
  };
}


#endif // __GUI__GTK__MGTK_FILEDIALOG_H__
