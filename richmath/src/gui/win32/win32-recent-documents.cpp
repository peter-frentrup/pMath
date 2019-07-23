#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601

#include <gui/win32/win32-recent-documents.h>
#include <gui/recent-documents.h>

#include <gui/win32/ole/combase.h>
#include <shlobj.h>
//#include <shlobjidl.h>


using namespace pmath;
using namespace richmath;


extern pmath_symbol_t richmath_System_DollarApplicationFileName;


namespace {
  class FileAssociationRegistry {
      static const wchar_t *app_user_model_id;
      static const wchar_t *document_prog_id;
    public:
      static void init_app_user_model_id();
      
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
  const wchar_t *str = (const wchar_t*)path.buffer();
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
  
  String path = String::FromUcs2((const uint16_t*)target);
  if(path.length() == 0)
    return E_FAIL;
  
  const uint16_t *buf = path.buffer();
  int i = path.length();
  while(i > 0 && buf[i-1] != '\\')
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
  
  ComBase<IObjectArray> items;
  HR(app_doc_lists->GetList(ADLT_RECENT, 9, items.iid(), (void**)items.get_address_of()));
  
  unsigned count;
  HR(items->GetCount(&count));
  
  *result = MakeList(count);
  for(size_t i = 0; i < count; ++i) {
    ComBase<IUnknown> obj;
    HR(items->GetAt(i, obj.iid(), (void**)obj.get_address_of()));
    
    if(auto shell_item = obj.as<IShellItem>()) {
      Expr menu_item;
      if(HRbool(shell_item_to_menu_item(std::move(shell_item), &menu_item, i))) {
        result->set(i + 1, std::move(menu_item));
        continue;
      }
    }
    
    if(auto shell_link = obj.as<IShellLinkW>()) {
      Expr menu_item;
      if(HRbool(shell_link_to_menu_item(std::move(shell_link), &menu_item, i))) {
        result->set(i + 1, std::move(menu_item));
        continue;
      }
    }
  }
  
  return S_OK;
}

Expr Win32RecentDocuments::as_menu_list() {
  Expr result;
  
  if(SUCCEEDED(jump_list_to_menu_list(&result)))
    return result;
  
  return List();
}

void Win32RecentDocuments::init() {
  FileAssociationRegistry::init_app_user_model_id();
  
  if(!FileAssociationRegistry::are_file_associations_registered())
    HRreport(FileAssociationRegistry::register_application());
}

void Win32RecentDocuments::done() {
}

// class FileAssociationRegistry ...

const wchar_t *FileAssociationRegistry::app_user_model_id = L"Frentrup.pMath.RichMath";
const wchar_t *FileAssociationRegistry::document_prog_id = L"Frentrup.pMath.RichMath.Document";

void FileAssociationRegistry::init_app_user_model_id() {
  HMODULE shell32 = LoadLibrary("shell32.dll");
  if(shell32) {
    HRESULT (WINAPI *p_SetCurrentProcessExplicitAppUserModelID)(PCWSTR);
    p_SetCurrentProcessExplicitAppUserModelID = (HRESULT (WINAPI *)(PCWSTR))
                                                GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    
    if(p_SetCurrentProcessExplicitAppUserModelID) {
      HRreport(p_SetCurrentProcessExplicitAppUserModelID(app_user_model_id));
    }
    
    FreeLibrary(shell32);
  }
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
  
  progid.create(L"CurVer", KEY_SET_VALUE | KEY_CREATE_SUB_KEY).set_string_value(L"", document_prog_id);
  
  String exe = Evaluate(Symbol(richmath_System_DollarApplicationFileName));
  if(exe.length() == 0)
    return E_FAIL;
  
  //progid.create(L"DefaultIcon", KEY_SET_VALUE | KEY_CREATE_SUB_KEY).set_string_value(L"", exe + ",1");
  
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

// ... class FileAssociationRegistry


//{ class RegistryKey ...

RegistryKey RegistryKey::CurrentUser { HKEY_CURRENT_USER, false };

RegistryKey RegistryKey::open(const wchar_t *name) const {
  RegistryKey result;
  
  HRreport(HRESULT_FROM_WIN32(RegOpenKeyW(_key, name, result.pointer())));
  
  return std::move(result);
}

RegistryKey RegistryKey::create(const wchar_t *name, REGSAM desiredAccess, DWORD options) {
  RegistryKey result;
  
  HRreport(HRESULT_FROM_WIN32(RegCreateKeyExW(_key, name, 0, nullptr, options, desiredAccess, nullptr, result.pointer(), nullptr)));
  
  return std::move(result);
}

HRESULT RegistryKey::set_string_value(const wchar_t *name, const wchar_t *str) {
  return HRESULT_FROM_WIN32(RegSetValueExW(_key, name, 0, REG_SZ, (const BYTE*)str, (wcslen(str) + 1) * sizeof(wchar_t)));
}

HRESULT RegistryKey::set_string_value(const wchar_t *name, String s) {
  s+= String::FromChar(0);
  
  size_t len = (size_t)s.length();
  if(!len)
    return E_OUTOFMEMORY;
  
  return HRESULT_FROM_WIN32(RegSetValueExW(_key, name, 0, REG_SZ, (const BYTE*)s.buffer(), len * sizeof(wchar_t)));
}

//} ... class RegistryKey
