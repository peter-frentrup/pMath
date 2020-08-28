#include <gui/gtk/mgtk-cursors.h>
#include <gtk/gtk.h>

#include <util/hashtable.h>

static const char *xpm_document[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                                ",  //  8
  "                                ",  //  9
  "                                ",  // 10
  "                                ",  // 11
  "        .                .      ",  // 12
  "       .+.              .+.     ",  // 13
  "       .+.              .+.     ",  // 14
  "        .+..............+.      ",  // 15
  "         .++++++++++++++.       ",  // 16
  "        .+..............+.      ",  // 17
  "       .+.              .+.     ",  // 18
  "       .+.              .+.     ",  // 19
  "        .                .      ",  // 20
  "                                ",  // 21
  "                                ",  // 22
  "                                ",  // 23
  "                                ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_no_select[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                                ",  //  8
  "              +++++             ",  //  9
  "            ++..+..++           ",  // 10
  "           +....+....+          ",  // 11
  "          +.....+.....+         ",  // 12
  "          +.....+.....+         ",  // 13
  "         +......+......+        ",  // 14
  "         +......+......+        ",  // 15
  "         +++++++++++++++        ",  // 16
  "         +......+......+        ",  // 17
  "         +......+......+        ",  // 18
  "          +.....+.....+         ",  // 19
  "          +.....+.....+         ",  // 20
  "           +....+....+          ",  // 21
  "            ++..+..++           ",  // 22
  "              +++++             ",  // 23
  "                                ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_section[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                                ",  //  8
  "                                ",  //  9
  "                                ",  // 10
  "               ...    ..        ",  // 11
  "               .+.   .+.        ",  // 12
  "               .+.  .+.         ",  // 13
  "               .+. .+.          ",  // 14
  "               .+..+.........   ",  // 15
  "               .++++++++++++.   ",  // 16
  "               .+..+.........   ",  // 17
  "               .+...+.          ",  // 18
  "               .+.  .+.         ",  // 19
  "               .+.   .+.        ",  // 20
  "               ...    ..        ",  // 21
  "                                ",  // 22
  "                                ",  // 23
  "                                ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_e[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                                ",  //  8
  "                                ",  //  9
  "                                ",  // 10
  "                                ",  // 11
  "         .                .     ",  // 12
  "        .+.      .       .+.    ",  // 13
  "        .+.     .+.      .+.    ",  // 14
  "         .+......+.......+.     ",  // 15
  "          .++++++++++++++.      ",  // 16
  "         .+......+.......+.     ",  // 17
  "        .+.     .+.      .+.    ",  // 18
  "        .+.      .       .+.    ",  // 19
  "         .                .     ",  // 20
  "                                ",  // 21
  "                                ",  // 22
  "                                ",  // 23
  "                                ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_n[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "              ..   ..           ",  //  6
  "             .++. .++.          ",  //  7
  "              ..+.+..           ",  //  8
  "                .+.             ",  //  9
  "                .+.             ",  // 10
  "                .+.             ",  // 11
  "                .+.             ",  // 12
  "                .+.             ",  // 13
  "                .+.             ",  // 14
  "               ..+..            ",  // 15
  "              .+++++.           ",  // 16
  "               ..+..            ",  // 17
  "                .+.             ",  // 18
  "                .+.             ",  // 19
  "                .+.             ",  // 20
  "                .+.             ",  // 21
  "                .+.             ",  // 22
  "              ..+.+..           ",  // 23
  "             .++. .++.          ",  // 24
  "              ..   ..           ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_ne[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                     .          ",  //  7
  "                    .+.         ",  //  8
  "                     .+.        ",  //  9
  "                     .+..       ",  // 10
  "                     .+++.      ",  // 11
  "                    .+...+.     ",  // 12
  "                   .+.   .      ",  // 13
  "                . .+.           ",  // 14
  "               .+.+.            ",  // 15
  "                .+.             ",  // 16
  "               .+.+.            ",  // 17
  "          .   .+. .             ",  // 18
  "         .+...+.                ",  // 19
  "          .+++.                 ",  // 20
  "           ..+.                 ",  // 21
  "            .+.                 ",  // 22
  "             .+.                ",  // 23
  "              .                 ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_nw[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "             .                  ",  //  7
  "            .+.                 ",  //  8
  "           .+.                  ",  //  9
  "          ..+.                  ",  // 10
  "         .+++.                  ",  // 11
  "        .+...+.                 ",  // 12
  "         .   .+.                ",  // 13
  "              .+. .             ",  // 14
  "               .+.+.            ",  // 15
  "                .+.             ",  // 16
  "               .+.+.            ",  // 17
  "                . .+.   .       ",  // 18
  "                   .+...+.      ",  // 19
  "                    .+++.       ",  // 20
  "                    .+..        ",  // 21
  "                    .+.         ",  // 22
  "                   .+.          ",  // 23
  "                    .           ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_s[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "              ..   ..           ",  //  7
  "             .++. .++.          ",  //  8
  "              ..+.+..           ",  //  9
  "                .+.             ",  // 10
  "                .+.             ",  // 11
  "                .+.             ",  // 12
  "                .+.             ",  // 13
  "                .+.             ",  // 14
  "               ..+..            ",  // 15
  "              .+++++.           ",  // 16
  "               ..+..            ",  // 17
  "                .+.             ",  // 18
  "                .+.             ",  // 19
  "                .+.             ",  // 20
  "                .+.             ",  // 21
  "                .+.             ",  // 22
  "                .+.             ",  // 23
  "              ..+.+..           ",  // 24
  "             .++. .++.          ",  // 25
  "              ..   ..           ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_se[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "              .                 ",  //  8
  "             .+.                ",  //  9
  "            .+.                 ",  // 10
  "           ..+.                 ",  // 11
  "          .+++.                 ",  // 12
  "         .+...+.                ",  // 13
  "          .   .+. .             ",  // 14
  "               .+.+.            ",  // 15
  "                .+.             ",  // 16
  "               .+.+.            ",  // 17
  "                . .+.           ",  // 18
  "                   .+.   .      ",  // 19
  "                    .+...+.     ",  // 20
  "                     .+++.      ",  // 21
  "                     .+..       ",  // 22
  "                     .+.        ",  // 23
  "                    .+.         ",  // 24
  "                     .          ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_sw[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                    .           ",  //  8
  "                   .+.          ",  //  9
  "                    .+.         ",  // 10
  "                    .+..        ",  // 11
  "                    .+++.       ",  // 12
  "                   .+...+.      ",  // 13
  "                . .+.   .       ",  // 14
  "               .+.+.            ",  // 15
  "                .+.             ",  // 16
  "               .+.+.            ",  // 17
  "              .+. .             ",  // 18
  "         .   .+.                ",  // 19
  "        .+...+.                 ",  // 20
  "         .+++.                  ",  // 21
  "          ..+.                  ",  // 22
  "           .+.                  ",  // 23
  "            .+.                 ",  // 24
  "             .                  ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

static const char *xpm_text_w[] = {
  "32 32 3 1",
  "  c None",
  "+ c #000000",
  ". c #ffffff",
  "                                ",  //  0
  "                                ",  //  1
  "                                ",  //  2
  "                                ",  //  3
  "                                ",  //  4
  "                                ",  //  5
  "                                ",  //  6
  "                                ",  //  7
  "                                ",  //  8
  "                                ",  //  9
  "                                ",  // 10
  "                                ",  // 11
  "        .                .      ",  // 12
  "       .+.       .      .+.     ",  // 13
  "       .+.      .+.     .+.     ",  // 14
  "        .+.......+......+.      ",  // 15
  "         .++++++++++++++.       ",  // 16
  "        .+.......+......+.      ",  // 17
  "       .+.      .+.     .+.     ",  // 18
  "       .+.       .      .+.     ",  // 19
  "        .                .      ",  // 20
  "                                ",  // 21
  "                                ",  // 22
  "                                ",  // 23
  "                                ",  // 24
  "                                ",  // 25
  "                                ",  // 26
  "                                ",  // 27
  "                                ",  // 28
  "                                ",  // 29
  "                                ",  // 30
  "                                "   // 31
};

using namespace richmath;

namespace {
  class CppGdkCursor {
    public:
      CppGdkCursor(GdkCursor *_cursor = 0)
        : cursor(_cursor)
      {
      }
      
      CppGdkCursor(GdkPixbuf *pixbuf) {
        cursor = gdk_cursor_new_from_pixbuf(
                   gdk_display_get_default(),
                   pixbuf,
                   gdk_pixbuf_get_width(pixbuf) / 2,
                   gdk_pixbuf_get_height(pixbuf) / 2);
      }
      
      CppGdkCursor(const CppGdkCursor &src)
        : cursor(src.cursor ? gdk_cursor_ref(src.cursor) : 0)
      {
      }
      
      CppGdkCursor &operator=(const CppGdkCursor &src) {
        GdkCursor *c = src.cursor ? gdk_cursor_ref(src.cursor) : 0;
        if(cursor)
          gdk_cursor_unref(cursor);
        cursor = c;
        return *this;
      }
      
      ~CppGdkCursor() {
        if(cursor)
          gdk_cursor_unref(cursor);
      }
      
    public:
      GdkCursor *cursor;
  };
}

static int num_refs = 0;

static Hashtable<CursorType, CppGdkCursor> all_cursors;

static GdkCursor *new_cursor_from_name_or_fallback(const char *name, GdkCursorType fallback) {
  GdkDisplay *display = gdk_display_get_default();
  GdkCursor *cursor = nullptr;
  
  char *theme_name;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-name", &theme_name, nullptr);
  if(theme_name) {
    cursor = gdk_cursor_new_from_name(display, name);
  }
  g_free(theme_name);
  
  if(!cursor)
    cursor = gdk_cursor_new_for_display(display, fallback);
  
  return cursor;
}

//{ class MathGtkCursors ...

MathGtkCursors::MathGtkCursors()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
// TODO: clear cache on cursor theme change
  
//  char *theme_name;
//  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-name", &theme_name, nullptr);
//  pmath_debug_print("[gtk-cursor-theme-name = %s]\n", theme_name);
//  g_free(theme_name);
  
  if(num_refs++ == 0) {
    all_cursors.set(FingerCursor,  new_cursor_from_name_or_fallback("help", GDK_HAND2));
    all_cursors.set(DefaultCursor, new_cursor_from_name_or_fallback("default", GDK_LEFT_PTR));
    
    all_cursors.set(DocumentCursor, gdk_pixbuf_new_from_xpm_data(xpm_document));
    all_cursors.set(NoSelectCursor, gdk_pixbuf_new_from_xpm_data(xpm_no_select));
    all_cursors.set(SectionCursor,  gdk_pixbuf_new_from_xpm_data(xpm_section));
    
    all_cursors.set(TextECursor,    gdk_pixbuf_new_from_xpm_data(xpm_text_e));
    all_cursors.set(TextNECursor,   gdk_pixbuf_new_from_xpm_data(xpm_text_ne));
    all_cursors.set(TextNCursor,    gdk_pixbuf_new_from_xpm_data(xpm_text_n));
    all_cursors.set(TextNWCursor,   gdk_pixbuf_new_from_xpm_data(xpm_text_nw));
    all_cursors.set(TextWCursor,    gdk_pixbuf_new_from_xpm_data(xpm_text_w));
    all_cursors.set(TextSWCursor,   gdk_pixbuf_new_from_xpm_data(xpm_text_sw));
    all_cursors.set(TextSCursor,    gdk_pixbuf_new_from_xpm_data(xpm_text_s));
    all_cursors.set(TextSECursor,   gdk_pixbuf_new_from_xpm_data(xpm_text_se));
    
    all_cursors.set(SizeECursor,    new_cursor_from_name_or_fallback("e-resize",  GDK_RIGHT_SIDE));
    all_cursors.set(SizeNECursor,   new_cursor_from_name_or_fallback("ne-resize", GDK_TOP_RIGHT_CORNER));
    all_cursors.set(SizeNCursor,    new_cursor_from_name_or_fallback("n-resize",  GDK_TOP_SIDE));
    all_cursors.set(SizeNWCursor,   new_cursor_from_name_or_fallback("nw-resize", GDK_TOP_LEFT_CORNER));
    all_cursors.set(SizeWCursor,    new_cursor_from_name_or_fallback("w-resize",  GDK_LEFT_SIDE));
    all_cursors.set(SizeSWCursor,   new_cursor_from_name_or_fallback("sw-resize", GDK_BOTTOM_LEFT_CORNER));
    all_cursors.set(SizeSCursor,    new_cursor_from_name_or_fallback("s-resize",  GDK_BOTTOM_SIDE));
    all_cursors.set(SizeSECursor,   new_cursor_from_name_or_fallback("se-resize", GDK_BOTTOM_RIGHT_CORNER));
    
  }
}

MathGtkCursors::~MathGtkCursors() {
  if(--num_refs == 0) {
    all_cursors.clear();
  }
}

GdkCursor *MathGtkCursors::get_gdk_cursor(CursorType type) {
  GdkCursor *c = all_cursors[type].cursor;
  if(c)
    gdk_cursor_ref(c);
    
  return c;
}

//} ... class MathGtkCursors
