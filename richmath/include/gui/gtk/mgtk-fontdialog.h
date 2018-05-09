#ifndef __GUI__GTK__MGTK_FONTDIALOG_H__
#define __GUI__GTK__MGTK_FONTDIALOG_H__

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


#endif // __GUI__GTK__MGTK_FONTDIALOG_H__
