#include <gui/gtk/mgtk-recent-documents.h>
#include <gui/recent-documents.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>


using namespace pmath;
using namespace richmath;


void MathGtkRecentDocuments::add(String path) {
  GtkRecentManager *manager = gtk_recent_manager_get_default();
  
  if(path.length() == 0)
    return;
    
  int len = 0;
  char *str =
#ifdef WIN32
    pmath_string_to_utf8(path.get(), &len);
#else
    pmath_string_to_native(path.get(), &len);
#endif
  if(str && len > 0) {
    char *uri = g_filename_to_uri(str, nullptr, nullptr);
    if(uri) {
      gtk_recent_manager_add_item(manager, uri);
      g_free(uri);
    }
  }
  pmath_mem_free(str);
}

static Expr recent_info_to_menu_item(GtkRecentInfo *info) {
  const char *uri = gtk_recent_info_get_uri(info);
  char *str = g_filename_from_uri(uri, nullptr, nullptr);
  if(!str)
    return Expr();
    
  String path =
#ifdef WIN32
    String(pmath_string_from_utf8(str, -1));
#else
    String(pmath_string_from_native(str, -1));
#endif
    
  g_free(str);
  
  String label = String::FromUtf8(gtk_recent_info_get_display_name(info));
  
  return RecentDocuments::open_document_menu_item(std::move(label), std::move(path));
}

Expr MathGtkRecentDocuments::as_menu_list() {
  GtkRecentManager *manager = gtk_recent_manager_get_default();
  
  Gather g;
  
  GList *items = gtk_recent_manager_get_items(manager);
  
  int count = 0;
  for(GList *cur = items; cur != nullptr && count < 9; cur = cur->next) {
    Expr menu_item = recent_info_to_menu_item((GtkRecentInfo*)cur->data);
    if(menu_item.is_expr()) {
      Gather::emit(std::move(menu_item));
      ++count;
    }
  }
  
  g_list_free_full(items, [](void *p) { gtk_recent_info_unref((GtkRecentInfo*)p); });
  
  return g.end();
}

void MathGtkRecentDocuments::init() {
}

void MathGtkRecentDocuments::done() {
}
