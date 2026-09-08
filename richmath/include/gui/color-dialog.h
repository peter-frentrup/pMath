#ifndef RICHMATH__GUI__COLOR_DIALOG_H__INCLUDED
#define RICHMATH__GUI__COLOR_DIALOG_H__INCLUDED


#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-colordialog.h>
#endif

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-colordialog.h>
#endif

#include <util/pmath-extra.h>


namespace richmath {
  class ColorDialog :
  #if RICHMATH_USE_GTK_GUI
    private MathGtkColorDialog
  #elif RICHMATH_USE_WIN32_GUI
    private Win32ColorDialog
  #endif
  {
      class Impl;
      Impl &impl;
      ColorDialog(Impl &impl) : impl{impl} {}
    protected:
      virtual void set_color(Color current) override;
    public:
      static Expr run(Expr expr);
  };
}

#endif // RICHMATH__GUI__COLOR_DIALOG_H__INCLUDED
