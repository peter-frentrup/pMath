#include <gui/gtk/mgtk-clipboard.h>

#include <util/array.h>

#include <assert.h>


using namespace richmath;

namespace{
  class ClipboardData: public Base{
    public:
      ClipboardData(): Base()
      {
      }
      
      unsigned add(const char *data, int size){
        all_data.add(size, data);
        end_pointers.add(all_data.length());
        return (unsigned)(end_pointers.length() - 1);
      }
      
      static void get_data_callback(
        GtkClipboard     *clipboard, 
        GtkSelectionData *selection_data,
        guint             info,
        gpointer          user_data
      ){
        ClipboardData *self = (ClipboardData*)user_data;
        
        assert(self != NULL);
        assert(info >= 0);
        assert(info < (unsigned)self->end_pointers.length());
        
        int end = self->end_pointers[info];
        int start = info > 0 ? self->end_pointers[info - 1] : 0;
        
        gtk_selection_data_set(
          selection_data, 
          selection_data->target, 
          8, 
          (const guchar*)(self->all_data.items() + start),
          end - start);
      }
      
      static void clear_callback(
        GtkClipboard *clipboard,
        gpointer      user_data
      ){
        ClipboardData *self = (ClipboardData*)user_data;
        delete self;
      }
      
    public:
      Array<int>  end_pointers;
      Array<char> all_data;
  };
  
  class OpenedGtkClipboard: public OpenedClipboard{
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
      
      virtual ~OpenedGtkClipboard(){
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
      
      virtual bool add_binary(String mimetype, void *data, size_t size){
        if(size >= INT_MAX / 2)
          return false;
        unsigned info = clipboard_data->add((const char*)data, (int)size);
        
        if(mimetype.equals(Clipboard::PlainText)){
          gtk_target_list_add_text_targets(targets, info);
        }
        else{
          gtk_target_list_add(targets, MathGtkClipboard::mimetype_to_atom(mimetype), 0, info);
        }
        
        return true;
      }
      
      virtual bool add_text(String mimetype, String data){
        int len;
        char *str = pmath_string_to_utf8(data.get(), &len);
        bool result = add_binary(mimetype, str, (size_t)len);
        pmath_mem_free(str);
        return result;
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

MathGtkClipboard::~MathGtkClipboard(){
  if(Clipboard::std == this)
    Clipboard::std = Clipboard::dummy;
}

GtkClipboard *MathGtkClipboard::clipboard(){
  if(!_clipboard)
    _clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  
  return _clipboard;
}

bool MathGtkClipboard::has_format(String mimetype){
  bool result = 0 != gtk_clipboard_wait_is_target_available(
    clipboard(), 
    MathGtkClipboard::mimetype_to_atom(mimetype));
  
  if(result)
    return true;
  
  if(mimetype.equals(Clipboard::PlainText)){
    return 0 != gtk_clipboard_wait_is_text_available(clipboard());
  }
  
  return false;
}

Expr MathGtkClipboard::read_as_binary_file(String mimetype){
  GtkSelectionData *data = gtk_clipboard_wait_for_contents(
    clipboard(),
    MathGtkClipboard::mimetype_to_atom(mimetype));
  
  if(!data)
    return Expr();
  
  size_t size = (size_t)gtk_selection_data_get_length(data);
  
  Expr result(pmath_file_create_binary_buffer(size));
  pmath_file_write(result.get(), gtk_selection_data_get_data(data), size);
  
  gtk_selection_data_free(data);
  return result;
}

String MathGtkClipboard::read_as_text(String mimetype){
  if(mimetype.equals(Clipboard::PlainText)){
    char *str = gtk_clipboard_wait_for_text(clipboard());
    
    if(str){
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
  const char *str = (const char*)gtk_selection_data_get_data(  data);
  
  if(str[length-1] == '\0')
    --length;
  
  String result = String::FromUtf8(str, length);
  
  gtk_selection_data_free(data);
  return result;
}

SharedPtr<OpenedClipboard> MathGtkClipboard::open_write(){
  return new OpenedGtkClipboard(clipboard());
}

GdkAtom MathGtkClipboard::mimetype_to_atom(String mimetype){
  GdkAtom atom;
  
  char *str = pmath_string_to_utf8(mimetype.get(), NULL);
  atom = gdk_atom_intern(str, TRUE);
  pmath_mem_free(str);
  
  return atom;
}

//} ... class MathGtkClipboard
