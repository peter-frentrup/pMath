#ifndef RICHMATH__GUI__FLTK__MFLTK_FONTDIALOG_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_FONTDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <util/style.h>


namespace richmath {
  class MathFltkFontDialog {
    public:
      MathFltkFontDialog() = delete;

      static pmath::Expr show(Style initial_style = nullptr);
  };
}


#endif // RICHMATH__GUI__FLTK__MFLTK_FONTDIALOG_H__INCLUDED
