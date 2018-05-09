#ifndef __GUI__GTK__MGTK_COLORDIALOG_H__
#define __GUI__GTK__MGTK_COLORDIALOG_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>


namespace richmath {
  class MathGtkColorDialog {
    public:
      MathGtkColorDialog() = delete;

      static pmath::Expr show(int initialcolor = -1); // 0xrrggbb
  };
}


#endif // __GUI__GTK__MGTK_COLORDIALOG_H__
