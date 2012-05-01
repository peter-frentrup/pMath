#ifndef __GUI__GTK__MGTK_FONTDIALOG_H__
#define __GUI__GTK__MGTK_FONTDIALOG_H__


#include <util/style.h>


namespace richmath {
  class MathGtkFontDialog: public Base {
    public:
      static pmath::Expr show(SharedPtr<Style> initial_style = 0);
  };
}


#endif // __GUI__GTK__MGTK_FONTDIALOG_H__
