#ifndef __GUI__GTK__MGTK_FILEDIALOG_H__
#define __GUI__GTK__MGTK_FILEDIALOG_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class MathGtkFileDialog: public Base {
    public:
      static pmath::Expr show(
        bool           save,
        pmath::String  initialfile,
        pmath::Expr    filter,
        pmath::String  title);
  };
}


#endif // __GUI__GTK__MGTK_FILEDIALOG_H__
