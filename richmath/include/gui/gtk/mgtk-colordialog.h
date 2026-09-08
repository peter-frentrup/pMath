#ifndef RICHMATH__GUI__GTK__MGTK_COLORDIALOG_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_COLORDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <graphics/color.h>


namespace richmath {
  class MathGtkColorDialog {
    protected:
      MathGtkColorDialog() {}
      Expr show_impl(Color initialcolor);
    
    public:
      virtual void set_color(Color current) = 0;
  };
}


#endif // RICHMATH__GUI__GTK__MGTK_COLORDIALOG_H__INCLUDED
