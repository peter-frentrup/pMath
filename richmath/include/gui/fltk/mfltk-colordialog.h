#ifndef RICHMATH__GUI__FLTK__MFLTK_COLORDIALOG_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_COLORDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <graphics/color.h>


namespace richmath {
  class MathFltkColorDialog {
    protected:
      MathFltkColorDialog() {}
      Expr show_impl(Color initialcolor);
    
    public:
      virtual void set_color(Color current) = 0;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_COLORDIALOG_H__INCLUDED
