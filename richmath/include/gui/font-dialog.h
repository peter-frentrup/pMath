#ifndef RICHMATH__GUI__FONT_DIALOG_H__INCLUDED
#define RICHMATH__GUI__FONT_DIALOG_H__INCLUDED


#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-fontdialog.h>
#endif

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-fontdialog.h>
#endif


namespace richmath {
  struct FontDialog :
  #if RICHMATH_USE_GTK_GUI
    private MathGtkFontDialog
  #elif RICHMATH_USE_WIN32_GUI
    private Win32FontDialog
  #endif
  {
    static Expr run(Expr style_expr);
  };
}

#endif // RICHMATH__GUI__FONT_DIALOG_H__INCLUDED
