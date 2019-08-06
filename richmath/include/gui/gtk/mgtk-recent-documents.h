#ifndef RICHMATH__GUI__GTK__MGTK_RECENT_DOCUMENTS_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_RECENT_DOCUMENTS_H__INCLUDED


#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <util/pmath-extra.h>

namespace richmath {
  struct MathGtkRecentDocuments {
    static void add(String path);
    static Expr as_menu_list();
    static bool remove(String path);
    static void init();
    static void done();
  };
}


#endif // RICHMATH__GUI__GTK__MGTK_RECENT_DOCUMENTS_H__INCLUDED
