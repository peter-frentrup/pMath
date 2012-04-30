#ifndef __GUI__GTK__MGTK_COLORDIALOG_H__
#define __GUI__GTK__MGTK_COLORDIALOG_H__


#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class MathGtkColorDialog: public Base {
    public:
      static pmath::Expr show(int initialcolor = -1); // 0xrrggbb
  };
}


#endif // __GUI__GTK__MGTK_COLORDIALOG_H__
