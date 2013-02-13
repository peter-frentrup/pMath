#include <gui/gtk/mgtk-clipboard.h>

#include <util/array.h>

#include <assert.h>
#include <cairo.h>
#include <cmath>


using namespace richmath;

namespace {
  class ClipboardData: public Base {
    public:
      ClipboardData(): Base()
      {
        pixbuf = 0;
      }
      
      virtual ~ClipboardData() {
        if(pixbuf)
          gdk_pixbuf_unref(pixbuf);
      }
      
      unsigned add(const char *data, int size) {
        all_data.add(size, data);
        end_pointers.add(all_data.length());
        return (unsigned)(end_pointers.length() - 1);
      }
      
      static void get_data_callback(
        GtkClipboard     *clipboard,
        GtkSelectionData *selection_data,
        guint             info,
        gpointer          user_data
      ) {
        ClipboardData *self = (ClipboardData *)user_data;
        
        assert(self != NULL);
        
        if(info == PixbufInfoIndex) {
          gtk_selection_data_set_pixbuf(selection_data, self->pixbuf);
          return;
        }
        
        assert(info >= 0);
        assert(info < (unsigned)self->end_pointers.length());
        
        int end = self->end_pointers[info];
        int start = info > 0 ? self->end_pointers[info - 1] : 0;
        
        gtk_selection_data_set(
          selection_data,
          gtk_selection_data_get_data_type(selection_data),
          8,
          (const guchar *)(self->all_data.items() + start),
          end - start);
        
        //gtk_selection_data_set_pixbuf()
      }
      
      static void clear_callback(
        GtkClipboard *clipboard,
        gpointer      user_data
      ) {
        ClipboardData *self = (ClipboardData *)user_data;
        delete self;
      }
      
    public:
      Array<int>   end_pointers;
      Array<char>  all_data;
      GdkPixbuf   *pixbuf;
      
      static const guint PixbufInfoIndex = (guint)-1;
  };
  
  class OpenedGtkClipboard: public OpenedClipboard {
    protected:
      GtkClipboard  *clipboard;
      ClipboardData *clipboard_data;
      GtkTargetList *targets;
      
    public:
      OpenedGtkClipboard(GtkClipboard *_clipboard)
        : OpenedClipboard(),
          clipboard(_clipboard),
          clipboard_data(new ClipboardData),
          targets(gtk_target_list_new(NULL, 0))
      {
      }
      
      virtual ~OpenedGtkClipboard() {
        gint num_targets = 0;
        GtkTargetEntry *target_array = gtk_target_table_new_from_list(targets, &num_targets);
        
        bool success = 0 != gtk_clipboard_set_with_data(
                         clipboard,
                         target_array,
                         num_targets,
                         ClipboardData::get_data_callback,
                         ClipboardData::clear_callback,
                         clipboard_data);
                         
        gtk_clipboard_set_can_store(clipboard, target_array, num_targets);
        gtk_target_table_free(target_array, num_targets);
        gtk_target_list_unref(targets);
        
        if(!success)
          delete clipboard_data;
      }
      
      virtual bool add_binary(String mimetype, void *data, size_t size) {
        if(size >= INT_MAX / 2)
          return false;
        unsigned info = clipboard_data->add((const char *)data, (int)size);
        
        MathGtkClipboard::add_to_target_list(targets, mimetype, info);
        
        return true;
      }
      
      virtual bool add_text(String mimetype, String data) {
        int len;
        char *str = pmath_string_to_utf8(data.get(), &len);
        bool result = add_binary(mimetype, str, (size_t)len);
        pmath_mem_free(str);
        return result;
      }
      
      virtual bool add_image(cairo_surface_t *image) {
        if(cairo_surface_get_type(image) == CAIRO_SURFACE_TYPE_IMAGE) {
          int width  = cairo_image_surface_get_width( image);
          int height = cairo_image_surface_get_height(image);
          
          GdkPixbuf *pixbuf;
          
#if GTK_MAJOR_VERSION >= 3
          {
            pixbuf = gdk_pixbuf_get_from_surface(image, 0, 0, width, height);
          }
#else
          {
            GdkPixmap *pixmap = gdk_pixmap_new(NULL, width, height,
                                               gdk_visual_get_best_depth());
          
            cairo_t *cr = gdk_cairo_create(pixmap);
            cairo_set_source_surface(cr, image, 0, 0);
            cairo_paint(cr);
            cairo_destroy(cr);
          
            pixbuf = gdk_pixbuf_get_from_drawable(
                       NULL,
                       GDK_PIXMAP(pixmap),
                       gdk_colormap_get_system(),
                       0, 0,
                       0, 0,
                       width,
                       height);
          
            gdk_pixmap_unref(pixmap);
          }
#endif
          
          MathGtkClipboard::add_to_target_list(targets, Clipboard::BitmapImage, ClipboardData::PixbufInfoIndex);
          
          if(clipboard_data->pixbuf)
            gdk_pixbuf_unref(clipboard_data->pixbuf);
          
          clipboard_data->pixbuf = pixbuf;
          return true;
        }
        
        return OpenedClipboard::add_image(image);
      }
  };
};

//{ class MathGtkClipboard ...

MathGtkClipboard MathGtkClipboard::obj;

MathGtkClipboard::MathGtkClipboard()
  : Clipboard(),
    _clipboard(0)
{
}

MathGtkClipboard::~MathGtkClipboard() {
  if(Clipboard::std == this)
    Clipboard::std = Clipboard::dummy;
}

GtkClipboard *MathGtkClipboard::clipboard() {
  if(!_clipboard)
    _clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    
  return _clipboard;
}

bool MathGtkClipboard::has_format(String mimetype) {
  bool result = 0 != gtk_clipboard_wait_is_target_available(
                  clipboard(),
                  MathGtkClipboard::mimetype_to_atom(mimetype));
                  
  if(result)
    return true;
    
  if(mimetype.equals(Clipboard::PlainText)) {
    return 0 != gtk_clipboard_wait_is_text_available(clipboard());
  }
  
  return false;
}

ReadableBinaryFile MathGtkClipboard::read_as_binary_file(String mimetype) {
  GtkSelectionData *data = gtk_clipboard_wait_for_contents(
                             clipboard(),
                             MathGtkClipboard::mimetype_to_atom(mimetype));
                             
  if(!data)
    return ReadableBinaryFile();
    
  int size = gtk_selection_data_get_length(data);
  if(size < 0)
    return ReadableBinaryFile();
    
  Expr result(pmath_file_create_binary_buffer((size_t)size));
  pmath_file_write(result.get(), gtk_selection_data_get_data(data), (size_t)size);
  
  gtk_selection_data_free(data);
  return ReadableBinaryFile(result);
}

String MathGtkClipboard::read_as_text(String mimetype) {
  if(mimetype.equals(Clipboard::PlainText)) {
    char *str = gtk_clipboard_wait_for_text(clipboard());
    
    if(str) {
      String result = String::FromUtf8(str);
      g_free(str);
      return result;
    }
  }
  
  GtkSelectionData *data = gtk_clipboard_wait_for_contents(
                             clipboard(),
                             MathGtkClipboard::mimetype_to_atom(mimetype));
                             
  if(!data)
    return String();
    
  int length      =              gtk_selection_data_get_length(data);
  const char *str = (const char *)gtk_selection_data_get_data(data);
  
  if(str[length - 1] == '\0')
    --length;
    
  String result = String::FromUtf8(str, length);
  
  gtk_selection_data_free(data);
  return result;
}

SharedPtr<OpenedClipboard> MathGtkClipboard::open_write() {
  return new OpenedGtkClipboard(clipboard());
}

cairo_surface_t *MathGtkClipboard::create_image(String mimetype, double width, double height) {
  if(mimetype.equals(Clipboard::BitmapImage)) {
    int w = (int)ceil(width);
    int h = (int)ceil(height);
    
    if(w < 1)
      w = 1;
    if(h < 1)
      h = 1;
      
    return cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  }
  
  return Clipboard::create_image(mimetype, width, height);
}

GdkAtom MathGtkClipboard::mimetype_to_atom(String mimetype) {
  GdkAtom atom;
  
  char *str = pmath_string_to_utf8(mimetype.get(), NULL);
  atom = gdk_atom_intern(str, TRUE);
  pmath_mem_free(str);
  
  return atom;
}

void MathGtkClipboard::add_to_target_list(GtkTargetList *targets, String mimetype, int info) {
  if(mimetype.equals(Clipboard::PlainText)) {
    gtk_target_list_add_text_targets(targets, info);
  }
  else if(mimetype.equals(Clipboard::BitmapImage)) {
    gtk_target_list_add_image_targets(targets, info, TRUE);
  }
  else{
    gtk_target_list_add(targets, mimetype_to_atom(mimetype), 0, info);
  }
}

//} ... class MathGtkClipboard
