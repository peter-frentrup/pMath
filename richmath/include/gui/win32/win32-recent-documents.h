#ifndef RICHMATH__GUI__WIN32__WIN32_RECENT_DOCUMENTS_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_RECENT_DOCUMENTS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <util/pmath-extra.h>


namespace richmath {
  struct Win32RecentDocuments {
    static void add(String path);
    static Expr as_menu_list();
    static void init();
    static void done();
  };
}


#endif // RICHMATH__GUI__WIN32__WIN32_RECENT_DOCUMENTS_H__INCLUDED
