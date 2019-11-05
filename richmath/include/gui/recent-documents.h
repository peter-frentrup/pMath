#ifndef RICHMATH__GUI__RECENT_DOCUMENTS_H__INCLUDED
#define RICHMATH__GUI__RECENT_DOCUMENTS_H__INCLUDED


#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-recent-documents.h>
#endif

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-recent-documents.h>
#endif


namespace richmath {
  struct RecentDocuments :
  #if defined(RICHMATH_USE_GTK_GUI)
    public MathGtkRecentDocuments
  #elif defined(RICHMATH_USE_WIN32_GUI)
    public Win32RecentDocuments
  #endif
  {
    static Expr open_document_menu_item(Expr label, Expr path);
    static Expr open_document_menu_item(Expr label, Expr path, bool add_to_recent);
  };
}

#endif // RICHMATH__GUI__RECENT_DOCUMENTS_H__INCLUDED
