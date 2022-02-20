#ifndef RICHMATH__GUI__GTK__MGTK_ICONS_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_ICONS_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <util/base.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>


namespace richmath {
  class MathGtkIcons: public Base {
    public:
      enum Index {
        AppIcon16Index,
        AppIcon24Index,
        AppIcon32Index,
        AppIcon48Index,
        
        IconsCount
      };
      
    public:
      MathGtkIcons();
      ~MathGtkIcons();
      
      GdkPixbuf *get_icon(Index idx); // you must free the result withn g_object_unref()
      
      GdkPixbuf *get_app_icon(GtkIconSize size); // you must free the result withn g_object_unref()
      
      static GdkPixbuf *new_pixbuf_from_image(cairo_surface_t *image);
      
      GList *get_app_icon_list(); // list of GdkPixbuf, you must free it
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_ICONS_H__INCLUDED
