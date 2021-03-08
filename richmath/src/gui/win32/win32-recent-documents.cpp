#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601

#include <gui/win32/win32-recent-documents.h>
#include <gui/recent-documents.h>

#include <gui/win32/ole/combase.h>
#include <gui/win32/ole/propvar.h>
#include <propkey.h>
#include <shlwapi.h>
#include <shlobj.h>
//#include <shlobjidl.h>


//http://www.catch22.net/tuts/undocumented-createprocess
#define STARTF_TITLESHORTCUT  0x800

  
using namespace pmath;
using namespace richmath;


extern pmath_symbol_t richmath_System_DollarApplicationFileName;
extern pmath_symbol_t richmath_System_None;


namespace {
  class FileAssociationRegistry {
      static const wchar_t *document_prog_id;
      
    public:
      static const wchar_t *app_user_model_id;
    
    public:
      static void init_app_user_model_id();
      static String find_startup_shortcut_path();
      static HRESULT set_shortcut_app_user_model_id(String shortcut_path);
      
      static bool are_file_associations_registered();
      static HRESULT register_application();
    
    private:
      static HRESULT register_program_id();
      static HRESULT register_known_file_extensions();
  };
  
  class RegistryKey {
    public:
      RegistryKey() : _key(nullptr), _owned(false) {}
      RegistryKey(HKEY key, bool owned) : _key(key), _owned(owned) {}
      
      ~RegistryKey() {
        if(_owned && _key)
          RegCloseKey(_key);
      }
      
      RegistryKey(const RegistryKey &other) = delete;
      RegistryKey(RegistryKey &&other)
        : _key(other._key),
          _owned(other._owned)
      {
        other._key = nullptr;
        other._owned = false;
      }
      
      explicit operator bool() const { return _key != nullptr; }
      
      HKEY *pointer() { return &_key; }
      
      RegistryKey open(const wchar_t *name) const;
      RegistryKey create(const wchar_t *name, REGSAM desiredAccess, DWORD options = REG_OPTION_NON_VOLATILE);
      
      HRESULT set_string_value(const wchar_t *name, const wchar_t *str);
      HRESULT set_string_value(const wchar_t *name, String s);
      
      static RegistryKey CurrentUser;
      
    private:
      HKEY _key;
      bool _owned;
  };
}


void Win32RecentDocuments::add(String path) {
  if(path.length() == 0)
    return;
    
  path += String::FromChar(0);
  const wchar_t *str = path.buffer_wchar();
  if(!str)
    return;
    
  SHAddToRecentDocs(SHARD_PATHW, str);
}

static Expr indexed_open_document_menu_item(int index, String label, String path) {
  label = String("&") + Expr(index + 1).to_string() + " " + label;
  
  return RecentDocuments::open_document_menu_item(std::move(label), std::move(path));
}

static HRESULT shell_item_to_menu_item(ComBase<IShellItem> shell_item, Expr *menu_item, int index) {
  wchar_t *str = nullptr;
  
  HR(shell_item->GetDisplayName(SIGDN_FILESYSPATH, &str));
  if(!PathFileExistsW(str)) {
    CoTaskMemFree(str);
    return E_FAIL;
  }
  String path = String::FromUcs2((const uint16_t*)str);
  CoTaskMemFree(str);
  
  HR(shell_item->GetDisplayName(SIGDN_NORMALDISPLAY, &str));
  String label = String::FromUcs2((const uint16_t*)str);
  CoTaskMemFree(str);
  
  *menu_item = indexed_open_document_menu_item(index, std::move(label), std::move(path));
  return S_OK;
}

static HRESULT shell_link_to_menu_item(ComBase<IShellLinkW> shell_link, Expr *menu_item, int index) {
  wchar_t target[MAX_PATH];
  
  HR(shell_link->GetPath(target, ARRAYSIZE(target), nullptr, 0));
  target[ARRAYSIZE(target) - 1] = L'\0';
  
  if(!PathFileExistsW(target))
    return E_FAIL;
  
  String path = String::FromUcs2((const uint16_t*)target);
  if(path.length() == 0)
    return E_FAIL;
  
  const uint16_t *buf = path.buffer();
  int i = path.length();
  while(i > 0 && buf[i - 1] != '\\')
    --i;
  
  *menu_item = indexed_open_document_menu_item(index, path.part(i), std::move(path));
  return S_OK;
}

// From Microsoft's AutomaticJumpList example:
// > For a document to appear in Jump Lists, the associated application must be registered to
// > handle the document's file type (extension).
static HRESULT jump_list_to_menu_list(Expr *result) {
  ComBase<IApplicationDocumentLists> app_doc_lists;
  
  HR(CoCreateInstance(
       CLSID_ApplicationDocumentLists, nullptr, CLSCTX_INPROC_SERVER,
       app_doc_lists.iid(),
       (void**)app_doc_lists.get_address_of()));
  
  HRreport(app_doc_lists->SetAppID(FileAssociationRegistry::app_user_model_id));
  
  ComBase<IObjectArray> items;
  HR(app_doc_lists->GetList(ADLT_RECENT, 0, items.iid(), (void**)items.get_address_of()));
  
  unsigned count;
  HR(items->GetCount(&count));
  
  Gather g;
  for(size_t i = 0, found = 0; i < count && found < 9; ++i) {
    ComBase<IUnknown> obj;
    HR(items->GetAt(i, obj.iid(), (void**)obj.get_address_of()));
    
    if(auto shell_item = obj.as<IShellItem>()) {
      Expr menu_item;
      if(HRbool(shell_item_to_menu_item(std::move(shell_item), &menu_item, i))) {
        ++found;
        Gather::emit(std::move(menu_item));
        continue;
      }
    }
    
    if(auto shell_link = obj.as<IShellLinkW>()) {
      Expr menu_item;
      if(HRbool(shell_link_to_menu_item(std::move(shell_link), &menu_item, i))) {
        ++found;
        Gather::emit(std::move(menu_item));
        continue;
      }
    }
  }
  *result = g.end();
  
  return S_OK;
}

Expr Win32RecentDocuments::as_menu_list() {
  Expr result;
  
  if(SUCCEEDED(jump_list_to_menu_list(&result)))
    return result;
  
  return List();
}

static HRESULT jump_list_remove(String path) {
  //pmath_debug_print_object("[remove recent ", path.get(), "]\n");
  
  ComBase<IApplicationDestinations> app_dest;
  HR(CoCreateInstance(
       CLSID_ApplicationDestinations, nullptr, CLSCTX_INPROC_SERVER,
       app_dest.iid(),
       (void**)app_dest.get_address_of()));
  
  path += String::FromChar(0);
  const wchar_t *buf = path.buffer_wchar();
  if(!buf)
    return E_OUTOFMEMORY;
  
  ComBase<IShellItem> item;
  HR(SHCreateItemFromParsingName(buf, nullptr, item.iid(), (void**)item.get_address_of()));
  
  HR(app_dest->SetAppID(FileAssociationRegistry::app_user_model_id));
  
  HR(app_dest->RemoveDestination(item.get()));
  
  return S_OK;
}

bool Win32RecentDocuments::remove(String path) {
  HRESULT hr = jump_list_remove(std::move(path));
  if(HRbool(hr)) {
    if(hr == S_FALSE)
      return false;
    else
      return true;
  }
  return false;
}

void Win32RecentDocuments::init() {
  if(String shortcut_path = FileAssociationRegistry::find_startup_shortcut_path()) {
    //Evaluate(Parse("FE`Private`StartupShortcutPath:= `1`", shortcut_path));
    HRreport(FileAssociationRegistry::set_shortcut_app_user_model_id(std::move(shortcut_path)));
  }
  
  // Note: SetCurrentProcessExplicitAppUserModelID() will overwrite the STARTUPINFO block.
  FileAssociationRegistry::init_app_user_model_id();
  
  if(!FileAssociationRegistry::are_file_associations_registered())
    HRreport(FileAssociationRegistry::register_application());
}

void Win32RecentDocuments::done() {
}

void Win32RecentDocuments::set_window_app_user_model_id(HWND hwnd) {
  if(!hwnd)
    return;
  
  HMODULE shell32 = LoadLibrary("shell32.dll");
  if(shell32) {
    HRESULT (WINAPI * p_SHGetPropertyStoreForWindow)(HWND, REFIID, void **);
    p_SHGetPropertyStoreForWindow = (HRESULT (WINAPI*)(HWND, REFIID, void **))
      GetProcAddress(shell32, "SHGetPropertyStoreForWindow");
    
    if(p_SHGetPropertyStoreForWindow) {
      ComBase<IPropertyStore> props;
      HRreport(p_SHGetPropertyStoreForWindow(hwnd, props.iid(), (void**)props.get_address_of()));
      if(props) {
        PropertyVariant appIdPropVar = PropertyVariant(FileAssociationRegistry::app_user_model_id);
        HRreport(props->SetValue(PKEY_AppUserModel_ID, appIdPropVar.value));
        //HRreport(props->Commit());
      }
    }
    
    FreeLibrary(shell32);
  }
}

//{ class FileAssociationRegistry ...

const wchar_t *FileAssociationRegistry::app_user_model_id = L"Frentrup.pMath.RichMath";
const wchar_t *FileAssociationRegistry::document_prog_id = L"Frentrup.pMath.RichMath.Document";

void FileAssociationRegistry::init_app_user_model_id() {
  HMODULE shell32 = LoadLibrary("shell32.dll");
  if(shell32) {
    HRESULT (WINAPI * p_SetCurrentProcessExplicitAppUserModelID)(PCWSTR);
    p_SetCurrentProcessExplicitAppUserModelID = (HRESULT (WINAPI *)(PCWSTR))
        GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    
    if(p_SetCurrentProcessExplicitAppUserModelID) {
      HRreport(p_SetCurrentProcessExplicitAppUserModelID(app_user_model_id));
    }
    
    FreeLibrary(shell32);
  }
}

String FileAssociationRegistry::find_startup_shortcut_path() {
  STARTUPINFOW si = {sizeof(si)};
  
  GetStartupInfoW(&si);
  
  Evaluate(ParseArgs(
    "FE`Private`StartupInfo:= {"
      "\"Desktop\" -> `1`,"
      "\"Title\" -> `2`,"
      "\"Reserved\" -> `3`,"
      "\"Flags\" -> `4`,"
      "\"Position\" -> {`5`, `6`},"
      "\"Size\" -> {`7`, `8`},"
      "\"SizeInChars\" -> {`9`, `10`},"
      "\"FillAttribute\" -> `11`,"
      "\"ShowWindowSetting\" -> `12`}", // "\"ReservedBytes\" -> `13`
    List(
      si.lpDesktop  ? String::FromUcs2((const uint16_t*)si.lpDesktop)  : Symbol(richmath_System_None),
      si.lpTitle    ? String::FromUcs2((const uint16_t*)si.lpTitle)    : Symbol(richmath_System_None),
      si.lpReserved ? String::FromUcs2((const uint16_t*)si.lpReserved) : Symbol(richmath_System_None),
      Expr((uintptr_t)si.dwFlags),
      Expr((uintptr_t)si.dwX),
      Expr((uintptr_t)si.dwY),
      Expr((uintptr_t)si.dwXSize),
      Expr((uintptr_t)si.dwYSize),
      Expr((uintptr_t)si.dwXCountChars),
      Expr((uintptr_t)si.dwYCountChars),
      Expr((uintptr_t)si.dwFillAttribute),
      Expr((uintptr_t)si.wShowWindow)/*,
      Expr((uintptr_t)si.cbReserved2)*/)));
  
  if(si.dwFlags & STARTF_TITLESHORTCUT) 
    return String::FromUcs2((const uint16_t*)si.lpTitle);
  
  return String();
}

HRESULT FileAssociationRegistry::set_shortcut_app_user_model_id(String shortcut_path) {
  if(shortcut_path.is_null())
    return E_INVALIDARG;
  
  shortcut_path+= String::FromChar(0);
  if(shortcut_path.is_null())
    return E_OUTOFMEMORY;
  
  ComBase<IShellLinkW> link;
  HR(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, link.iid(), (void**)link.get_address_of()));
  
  ComBase<IPersistFile> file = link.as<IPersistFile>();
  if(!file)
    return E_NOINTERFACE;
  
  static_assert(sizeof(OLECHAR) == sizeof(wchar_t), "OLE2ANSI must not be defined");
  
  HR(file->Load(shortcut_path.buffer_wchar(), STGM_READWRITE));
  
  ComBase<IPropertyStore> props = link.as<IPropertyStore>();
  if(!props)
    return E_NOINTERFACE;
  
  PropertyVariant appIdPropVar;
  HR(props->GetValue(PKEY_AppUserModel_ID, &appIdPropVar.value));
  if(appIdPropVar) {
    pmath_debug_print_object("[shortcut already has an AppUserModelID: ", appIdPropVar.to_string_or_null().get(), "]\n");
    return S_FALSE;
  }
  
  pmath_debug_print("[setting AppUserModelID ...]\n");
  
  appIdPropVar = PropertyVariant(app_user_model_id);
  HR(props->SetValue(PKEY_AppUserModel_ID, appIdPropVar.value));
  HR(props->Commit());
  
  HR(file->Save(nullptr, TRUE));
  
  return S_OK;
}

bool FileAssociationRegistry::are_file_associations_registered() {
  HKEY prog_id_key;
  
  if(SUCCEEDED(HRESULT_FROM_WIN32(RegOpenKeyW(HKEY_CLASSES_ROOT, document_prog_id, &prog_id_key)))) {
    RegCloseKey(prog_id_key);
    return true;
  }
  
  return false;
}

HRESULT FileAssociationRegistry::register_application() {
  HR(register_program_id());
  HR(register_known_file_extensions());
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return S_OK;
}

HRESULT FileAssociationRegistry::register_program_id() {
  RegistryKey hkcu_software_classes = RegistryKey::CurrentUser.open(L"Software\\Classes");
  
  RegistryKey progid = hkcu_software_classes.create(document_prog_id, KEY_SET_VALUE | KEY_CREATE_SUB_KEY);
  if(!progid)
    return E_FAIL;
  
  HR(progid.set_string_value(L"",                 L"pMath Document"));
  HR(progid.set_string_value(L"FriendlyTypeName", L"pMath Document"));
  HR(progid.set_string_value(L"AppUserModelID", app_user_model_id));
  
  
  String exe = Evaluate(Symbol(richmath_System_DollarApplicationFileName));
  if(exe.length() == 0)
    return E_FAIL;
  
  progid.create(L"DefaultIcon", KEY_SET_VALUE | KEY_CREATE_SUB_KEY).set_string_value(L"", exe + ",1");
  
  RegistryKey progid_shell = progid.create(L"Shell", KEY_SET_VALUE | KEY_CREATE_SUB_KEY);
  HR(progid_shell.set_string_value(L"", L"Open"));
  
  RegistryKey progid_shell_open_command = progid_shell.create(L"Open\\Command", KEY_SET_VALUE);
  
  HR(progid_shell_open_command.set_string_value(L"", String("\"") + exe + "\" \"%1\""));
  
  return S_OK;
}

HRESULT FileAssociationRegistry::register_known_file_extensions() {
  RegistryKey hkcu_software_classes = RegistryKey::CurrentUser.open(L"Software\\Classes");
  
  RegistryKey key = hkcu_software_classes.create(L".pmathdoc\\OpenWithProgIDs", KEY_SET_VALUE);
  HR(key.set_string_value(document_prog_id, L""));
  
  return S_OK;
}

//} ... class FileAssociationRegistry


//{ class RegistryKey ...

RegistryKey RegistryKey::CurrentUser { HKEY_CURRENT_USER, false };

RegistryKey RegistryKey::open(const wchar_t *name) const {
  RegistryKey result;
  
  HRreport(HRESULT_FROM_WIN32(RegOpenKeyW(_key, name, result.pointer())));
  
  return result;
}

RegistryKey RegistryKey::create(const wchar_t *name, REGSAM desiredAccess, DWORD options) {
  RegistryKey result;
  
  HRreport(HRESULT_FROM_WIN32(RegCreateKeyExW(_key, name, 0, nullptr, options, desiredAccess, nullptr, result.pointer(), nullptr)));
  
  return result;
}

HRESULT RegistryKey::set_string_value(const wchar_t *name, const wchar_t *str) {
  return HRESULT_FROM_WIN32(RegSetValueExW(_key, name, 0, REG_SZ, (const BYTE*)str, (wcslen(str) + 1) * sizeof(wchar_t)));
}

HRESULT RegistryKey::set_string_value(const wchar_t *name, String s) {
  s += String::FromChar(0);
  
  size_t len = (size_t)s.length();
  if(!len)
    return E_OUTOFMEMORY;
  
  return HRESULT_FROM_WIN32(RegSetValueExW(_key, name, 0, REG_SZ, (const BYTE*)s.buffer(), len * sizeof(wchar_t)));
}

//} ... class RegistryKey

