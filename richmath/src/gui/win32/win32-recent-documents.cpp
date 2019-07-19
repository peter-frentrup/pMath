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


void Win32RecentDocuments::add(String path) {
  if(path.length() == 0)
    return;
    
  path += String::FromChar(0);
  const wchar_t *str = (const wchar_t*)path.buffer();
  if(!str)
    return;
    
  SHAddToRecentDocs(SHARD_PATHW, str);
}

static HRESULT shell_item_to_menu_item(ComBase<IShellItem> shell_item, Expr *menu_item) {
  wchar_t *str = nullptr;
  
  HR(shell_item->GetDisplayName(SIGDN_FILESYSPATH, &str));
  String path = String::FromUcs2((const uint16_t*)str);
  CoTaskMemFree(str);
  
  HR(shell_item->GetDisplayName(SIGDN_NORMALDISPLAY, &str));
  String label = String::FromUcs2((const uint16_t*)str);
  CoTaskMemFree(str);
  
  *menu_item = RecentDocuments::open_document_menu_item(std::move(label), std::move(path));
  return S_OK;
}

static HRESULT shell_link_to_menu_item(ComBase<IShellLinkW> shell_link, Expr *menu_item) {
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
  
  *menu_item = RecentDocuments::open_document_menu_item(path.part(i), std::move(path)); 
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
      if(HRbool(shell_item_to_menu_item(std::move(shell_item), &menu_item))) {
        result->set(i + 1, std::move(menu_item));
        continue;
      }
    }
    
    if(auto shell_link = obj.as<IShellLinkW>()) {
      Expr menu_item;
      if(HRbool(shell_link_to_menu_item(std::move(shell_link), &menu_item))) {
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
    
