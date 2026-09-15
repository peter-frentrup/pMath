#ifndef RICHMATH__GUI__FLTK__MFLTK_RECENT_DOCUMENTS_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_RECENT_DOCUMENTS_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <util/pmath-extra.h>

namespace richmath {
  struct MathFltkRecentDocuments {
    static void add(String path);
    static Expr as_menu_list();
    static bool remove(String path);
    static void init();
    static void done();
    
    private: 
      class Impl;
  };
}


#endif // RICHMATH__GUI__FLTK__MFLTK_RECENT_DOCUMENTS_H__INCLUDED
