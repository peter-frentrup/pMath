#include <util/filesystem.h>
#include <util/array.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/ole/combase.h>
#  include <gui/win32/ole/filesystembinddata.h>
#  include <shlobj.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gio/gio.h>
#endif

using namespace richmath;

static bool system_open_directory(String dir, Expr items);

#ifdef RICHMATH_USE_WIN32_GUI
static HRESULT win32_system_open_directory(String dir, Expr items);
static HRESULT CreateSimplePidl(const WIN32_FIND_DATAW &find_data, const wchar_t *path, PIDLIST_ABSOLUTE *ppidl);
#endif

#ifdef RICHMATH_USE_GTK_GUI
static bool mgtk_system_open_directory_via_dbus(String dir, Expr items);
static bool mgtk_system_open_directory(String dir);
static char *filename_to_g_uri(String filename);
#endif

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_List;

Expr richmath_eval_FrontEnd_SystemOpenDirectory(Expr expr) {
  /** FrontEnd`SystemOpenDirectory(filename)
      FrontEnd`SystemOpenDirectory(dir, {file1, file2, ...})
   */
  if(expr.expr_length() < 1 || expr.expr_length() > 2) 
    return Symbol(richmath_System_DollarFailed);
  
  String dir = expr[1];
  if(!dir)
    return Symbol(richmath_System_DollarFailed);
  
  if(dir.length() > 0)
    dir = FileSystem::to_existing_absolute_file_name(std::move(dir));
  
  Expr items;
  if(expr.expr_length() == 2) {
    items = expr[2];
    if(items.is_string())
      items = List(std::move(items));
  }
  else {
    String filename = dir;
    dir = FileSystem::extract_directory_path(&filename);
    if(filename.length() > 0)
      items = List(std::move(filename));
    else
      items = List();
  }
  
  expr = {};
  if(system_open_directory(std::move(dir), std::move(items)))
    return {};
  else
    return Symbol(richmath_System_DollarFailed);
}

static bool system_open_directory(String dir, Expr items) {
  if(!dir)
    return false;
  
  if(items[0] != richmath_System_List)
    return false;
  
  if(dir.length() == 0) {
    // allow e.g. FrontEnd`SystemOpenDirectory("", "C:\\")
    for(auto filename : items.items()) 
      if(String(std::move(filename)).length() == 0)
        return false;
  }
  else {
    for(auto filename : items.items()) 
      if(!FileSystem::is_filename_without_directory(String(std::move(filename))))
        return false;
  }
  
#if defined(RICHMATH_USE_WIN32_GUI)
  return HRbool(win32_system_open_directory(std::move(dir), std::move(items)));
#elif defined(RICHMATH_USE_GTK_GUI)
  if(mgtk_system_open_directory_via_dbus(dir, items))
    return true;
  return mgtk_system_open_directory(std::move(dir));
#endif
  return false;
}

#ifdef RICHMATH_USE_WIN32_GUI
static HRESULT win32_system_open_directory(String dir, Expr items) {
  WIN32_FIND_DATAW find_data = {};
  find_data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
  
  dir+= String::FromChar(0);
  if(!dir)
    return E_OUTOFMEMORY;
  
  ComHashPtr<ITEMIDLIST> folder_pidl;
  HR(CreateSimplePidl(find_data, dir.buffer_wchar(), folder_pidl.get_address_of()));
  
  ComBase<IShellFolder> folder;
  HR(SHBindToObject(nullptr, folder_pidl.get(), nullptr, IID_PPV_ARGS(folder.get_address_of())));
  
  Array<ComHashPtr<ITEMIDLIST>> item_idls(items.expr_length());
  item_idls.length(0);
  for(String name : items.items()) {
    ComHashPtr<ITEMIDLIST> idl;
    name+= String::FromChar(0);
    if(name) {
      if(HRbool(folder->ParseDisplayName(nullptr, nullptr, const_cast<wchar_t*>(name.buffer_wchar()), nullptr, idl.get_address_of(), nullptr))) {
        item_idls.add(std::move(idl));
      }
    }
  }
  
  HR(SHOpenFolderAndSelectItems(folder_pidl.get(), item_idls.length(), (const ITEMIDLIST**)item_idls.items(), 0));
  return S_OK;
}

static HRESULT CreateSimplePidl(const WIN32_FIND_DATAW &find_data, const wchar_t *path, PIDLIST_ABSOLUTE *ppidl) {
  *ppidl = nullptr;
  ComBase<IBindCtx> bind_ctx;
  HR(FileSystemBindData::CreateFileSystemBindCtx(find_data, bind_ctx.get_address_of()));
  HR(SHParseDisplayName(path, bind_ctx.get(), ppidl, 0, nullptr));
  return S_OK;
}
#endif

#ifdef RICHMATH_USE_GTK_GUI
static bool mgtk_system_open_directory_via_dbus(String dir, Expr items) {
  bool success = false;
  
  GError* error = nullptr;
  GDBusConnection *conn = nullptr;
  GDBusProxy *proxy = nullptr;
  GVariantBuilder *uris_builder = nullptr;
  const char *method = nullptr;
  
  conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
  if(!conn) {
    pmath_debug_print("[g_bus_get_sync(G_BUS_TYPE_SESSION) failed]");
    goto FAILED_CONN;
  }
  
  g_dbus_connection_set_exit_on_close(conn, false);
  
  proxy = g_dbus_proxy_new_sync(
            conn, 
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            "org.freedesktop.FileManager1",
            "/org/freedesktop/FileManager1",
            "org.freedesktop.FileManager1",
            nullptr,
            &error);
  if(!proxy) {
    pmath_debug_print("[org.freedesktop.FileManager1 dbus object not found]");
    goto FAILED_PROXY;
  }
  
  uris_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
  if(!uris_builder) {
    pmath_debug_print("[g_variant_builder_new failed]");
    goto FAILED_URIS_BUILDER;
  }
  
  if(items.expr_length() == 0) {
    method = "ShowFolders";
    if(char *uri = filename_to_g_uri(dir)) {
      g_variant_builder_add_value(uris_builder, g_variant_new_take_string(uri));
    }
    else
      goto FAILED_URIS;
  }
  else {
    method = "ShowItems";
    bool any = false;
    for(String name : items.items()) {
      if(char *uri = filename_to_g_uri(FileSystem::file_name_join(dir, name))) {
        g_variant_builder_add_value(uris_builder, g_variant_new_take_string(uri));
        any = true;
      }
    }
    if(!any)
      goto FAILED_URIS;
  }
  
  g_dbus_proxy_call(
    proxy, 
    method, 
    g_variant_new("(ass)", uris_builder, ""),
    G_DBUS_CALL_FLAGS_NONE,
    3000,
    nullptr,
    nullptr,
    nullptr);
  
  success = true;                          FAILED_URIS:
  g_variant_builder_unref(uris_builder);   FAILED_URIS_BUILDER:
  g_object_unref(proxy);                   FAILED_PROXY:
  g_object_unref(conn);                    FAILED_CONN:
  if(error) {
    pmath_debug_print("[mgtk_system_open_directory_via_dbus failed: %s]\n", error->message);
    g_error_free(error);
  }
  return success;
}

static bool mgtk_system_open_directory(String dir) {
#ifdef PMATH_OS_WIN32
  // g_app_info_get_default_for_type() takes forever on Win32 ...
  return false;
#endif
  GAppInfo *app_info = nullptr;
  GList    *uris = nullptr;
  GError   *error = nullptr;
  bool success = false;
  
  app_info = g_app_info_get_default_for_type("inode/directory", true);
  if(!app_info) {
    pmath_debug_print("[g_app_info_get_default_for_type(\"inode/directory\") failed]\n");
    goto FAILED_APP_INFO;
  }
  
  uris = g_list_alloc();
  if(!uris) {
    pmath_debug_print("[g_list_alloc failed]\n");
    goto FAILED_ALLOC_URIS;
  }
  
  if(char *uri = filename_to_g_uri(std::move(dir))) {
    uris = g_list_append(uris, uri);
  }
  else {
    pmath_debug_print("[filename_to_g_uri failed]\n");
    goto FAILED_URI;
  }
  
  success = g_app_info_launch_uris(app_info, uris, nullptr, &error);
  
  success = true;                  FAILED_URI:
  g_list_free_full(uris, g_free);  FAILED_ALLOC_URIS:
  g_object_unref(app_info);        FAILED_APP_INFO:
  if(error) {
    pmath_debug_print("[mgtk_system_open_directory_via_dbus failed: %s]\n", error->message);
    g_error_free(error);
  }
  return success;
}

static char *filename_to_g_uri(String filename) {
  int len = 0;
  char *str = 
#ifdef WIN32
    pmath_string_to_utf8(filename.get(), &len);
#else
    pmath_string_to_native(filename.get(), &len);
#endif

  char *uri = nullptr;
  
  if(str) {
    uri = g_filename_to_uri(str, nullptr, nullptr);
  }
  
  pmath_mem_free(str);
  return uri;
}
#endif
