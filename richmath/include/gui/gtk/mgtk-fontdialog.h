#ifndef RICHMATH__GUI__GTK__MGTK_FONTDIALOG_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_FONTDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <util/style.h>


namespace richmath {
  class MathGtkFontDialog {
    public:
      MathGtkFontDialog() = delete;

      static pmath::Expr show(SharedPtr<Style> initial_style = nullptr);
  };
}


#endif // RICHMATH__GUI__GTK__MGTK_FONTDIALOG_H__INCLUDED
