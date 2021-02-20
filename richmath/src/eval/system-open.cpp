#include <util/filesystem.h>

#include <gui/document.h>
#include <gui/documents.h>
#include <gui/messagebox.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/ole/combase.h>
#  include <gui/win32/win32-widget.h>
#  include <shellapi.h>
#  include <shlwapi.h>
#  include <shtypes.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-widget.h>
#endif


using namespace richmath;


extern pmath_symbol_t richmath_System_DollarFailed;

static bool contains_character_in_range(String s, uint16_t ch_min, uint16_t ch_max);

static bool system_open(String uriOrPath);
static bool system_open_file_path(String path);
static bool system_open_non_file_uri(String uri);
static bool allow_file_extension_of(String path);

#ifdef RICHMATH_USE_WIN32_GUI
static bool win32_shell_execute_ex(String uri);
#endif

#ifdef RICHMATH_USE_GTK_GUI
static bool mgtk_system_open_file_path(String path);
static bool mgtk_system_open_non_file_uri(String uri);
static bool mgtk_system_open_uri_raw(const char *uri);
#endif

Expr richmath_eval_FrontEnd_SystemOpen(Expr expr) {
  /*  FrontEnd`SystemOpen(uriOrPath)
   */
   
  if(expr.expr_length() != 1) 
    return Symbol(richmath_System_DollarFailed);
  
  String s = expr[1];
  expr = {};
  
  return system_open(std::move(s)) ? Expr() : Symbol(richmath_System_DollarFailed);
}

static bool system_open(String uriOrPath) {
  if(!uriOrPath || uriOrPath.length() == 0)
    return false;
    
  if(contains_character_in_range(uriOrPath, 0, 0x1F))
    return false;
  
  if(String scheme = FileSystem::get_uri_scheme(uriOrPath)) {
    if(scheme.length() <= 1)
      return system_open_file_path(FileSystem::to_existing_absolute_file_name(std::move(uriOrPath)));
    
    if(contains_character_in_range(scheme, 'A', 'Z'))
      return false;
    
    if(scheme.equals("file")) {
      return system_open_file_path(
               FileSystem::to_existing_absolute_file_name(
                 FileSystem::get_local_path_from_uri(std::move(uriOrPath))));
    }
    
    return system_open_non_file_uri(uriOrPath);
  }
  
  return system_open_file_path(FileSystem::to_existing_absolute_file_name(std::move(uriOrPath)));
}

static bool system_open_file_path(String path) {
  if(!allow_file_extension_of(path))
    return false;
  
  // TODO: open *.pmathdoc files locally ?
  
#if defined(RICHMATH_USE_WIN32_GUI)
  return win32_shell_execute_ex(std::move(path));
#elif defined(RICHMATH_USE_GTK_GUI)
  return mgtk_system_open_file_path(std::move(path));
#else
  return false;
#endif
}

static bool system_open_non_file_uri(String uri) {
#if defined(RICHMATH_USE_WIN32_GUI)
  return win32_shell_execute_ex(std::move(uri));
#elif defined(RICHMATH_USE_GTK_GUI)
  return mgtk_system_open_non_file_uri(std::move(uri));
#else
  return false;
#endif
}

static bool allow_file_extension_of(String path) {
  const uint16_t *path_buf = path.buffer();
  if(!path_buf)
    return false;
    
  int len = path.length();
  
  int ext_start = len;
  while(ext_start > 0) {
    if(path_buf[ext_start-1] == '.') {
      --ext_start;
      break;
    }
    
    if(path_buf[ext_start-1] == '/' || path_buf[ext_start-1] == '\\')
      break;
  
    --ext_start;
  }
  
  if(ext_start < len && path_buf[ext_start] != '.')
    ext_start = len;
  
  String ext = path.part(ext_start);
  bool must_ask = false;
  if(contains_character_in_range(ext, 0, ' ') || contains_character_in_range(ext, 0x7F, 0xFFFFu)) {
    must_ask = true; // non-ascii extension is suspicious
  }
  else {
    // TODO: use a (configurable?) blocklist and/or allowlist
#ifdef RICHMATH_USE_WIN32_GUI
    {
      ext+= String::FromChar(0);
      const wchar_t *ext_buf = (const wchar_t*)ext.buffer();
      if(!ext_buf)
        return false;
        
      PERCEIVED type;
      PERCEIVEDFLAG flag;
      if(HRbool(AssocGetPerceivedType(ext_buf, &type, &flag, nullptr))) {
        switch(type) { 
          case PERCEIVED_TYPE_AUDIO:
          case PERCEIVED_TYPE_CONTACTS:
          case PERCEIVED_TYPE_IMAGE:
          case PERCEIVED_TYPE_DOCUMENT:
          case PERCEIVED_TYPE_TEXT:
          case PERCEIVED_TYPE_VIDEO:
            must_ask = false; 
            break;
          
          default:
            must_ask = true; 
            break;
        }
      }
      else
        must_ask = true;
    }
#endif
#ifdef RICHMATH_USE_GTK_GUI
    {
      int len;
      if(char *str = pmath_string_to_native(path.get(), &len)) {
        gboolean uncertain = TRUE;
        if(char *type = g_content_type_guess(str, nullptr, 0, &uncertain)) {
          if(g_content_type_can_be_executable(type)) {
            must_ask = true;
          }
          g_free(type);
        }
        else
          must_ask = true;
        
        pmath_mem_free(str);
      }
      else
        return false;
    }
#endif
  }
  
  if(must_ask) 
    return ask_open_suspicious_system_file(path);
  
  return true;
}

#ifdef RICHMATH_USE_WIN32_GUI

static bool win32_shell_execute_ex(String uri) {
  SHELLEXECUTEINFOW info;
  
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(!doc)
    doc = Documents::current();
  
  if(doc) {
    if(auto widget = dynamic_cast<Win32Widget *>(doc->native())) {
      info.hwnd = GetAncestor(widget->hwnd(), GA_ROOT);
    }
  }
  
  uri+= String::FromChar(0);
  info.lpFile = (const wchar_t*)uri.buffer();
  if(!info.lpFile)
    return false;
  
  return !!ShellExecuteExW(&info);
}

#endif // RICHMATH_USE_WIN32_GUI

#ifdef RICHMATH_USE_GTK_GUI

static bool mgtk_system_open_file_path(String path) {
  int len = 0;
  char *str =
#ifdef WIN32
    pmath_string_to_utf8(path.get(), &len);
#else
    pmath_string_to_native(path.get(), &len);
#endif
  
  bool result = false;
  if(str) {
    if(char *uri = g_filename_to_uri(str, nullptr, nullptr)) {
      result = mgtk_system_open_uri_raw(uri);
      g_free(uri);
    }
    pmath_mem_free(str);
  }
  
  return result;
}

static bool mgtk_system_open_non_file_uri(String uri) {
  bool result = false;
  int len;
  if(char *utf8 = pmath_string_to_utf8(uri.get(), &len)) {
    result = mgtk_system_open_uri_raw(utf8);
    pmath_mem_free(utf8);
  }
  
  return result;
}

static bool mgtk_system_open_uri_raw(const char *uri) {
#ifdef PMATH_OS_WIN32
  // gtk_show_uri() takes forever on Win32 ...
  return false;
#endif
  if(!uri)
    return false;
  
  GtkWindow *owner_window = nullptr;
  if(Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object())) {
    if(auto mwid = dynamic_cast<MathGtkWidget*>(doc->native())) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(mwid->widget());
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
#if GTK_MAJOR_VERSION >= 3
  return gtk_show_uri_on_window(
           owner_window,
           uri,
           GDK_CURRENT_TIME,
           nullptr);
#else
  return gtk_show_uri(
           owner_window ? gtk_window_get_screen(owner_window) : nullptr,
           uri,
           GDK_CURRENT_TIME,
           nullptr);
#endif
}

#endif

static bool contains_character_in_range(String s, uint16_t ch_min, uint16_t ch_max) {
  int len;
  const uint16_t *buf = s.buffer();
  for(int i = 0; i < len; ++i) {
    if(buf[i] >= ch_min && buf[i] <= ch_max)
      return true;
  }
  return false;
}
