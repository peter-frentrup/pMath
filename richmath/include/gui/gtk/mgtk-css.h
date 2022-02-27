#ifndef RICHMATH__GUI__GTK__MGTK_CSS_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_CSS_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <util/pmath-extra.h>

#include <gtk/gtk.h>


namespace richmath {
  class MathGtkCss {
    public:
      static void init();
      static void done();
      
#   if GTK_MAJOR_VERSION >= 3
      static Expr parse_text_shadow(const char *css);
      static Expr parse_text_shadow(GtkStyleContext *ctx);
#   endif
  };
  
#if GTK_MAJOR_VERSION < 3
  inline void MathGtkCss::init() {}
  inline void MathGtkCss::done() {}
#endif
}

#endif // RICHMATH__GUI__GTK__MGTK_CSS_H__INCLUDED
