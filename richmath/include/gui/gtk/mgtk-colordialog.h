#ifndef __GUI__GTK__MGTK_COLORDIALOG_H__
#define __GUI__GTK__MGTK_COLORDIALOG_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class MathGtkColorDialog: public Base {
    public:
      static pmath::Expr show(int initialcolor = -1); // 0xrrggbb
  };
}


#endif // __GUI__GTK__MGTK_COLORDIALOG_H__
