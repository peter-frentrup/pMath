#include <gui/win32/win32-filedialog.h>

#include <eval/application.h>
#include <gui/win32/win32-widget.h>

#include <windows.h>


using namespace richmath;
using namespace pmath;


//{ class Win32FileDialog ...

Expr Win32FileDialog::show(
  bool    save,
  String  initialfile,
  Expr    filter,
  String  title
) {
  OPENFILENAMEW data;
  String filterstring;
  String titlestring;
  
  static WCHAR filenamebuffer[4096];
  
  memset(&data, 0, sizeof(data));
  data.lStructSize = sizeof(data);
  
  if(save)
    data.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  else
    data.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
  if(filter.expr_length() > 0 && filter[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= filter.expr_length(); ++i) {
      Expr rule = filter[i];
      
      if( rule.expr_length() == 2 &&
          rule[0] == PMATH_SYMBOL_RULE)
      {
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
        if(!lhs.is_string())
          continue;
          
        if(rhs.is_string()) {
          filterstring += String(lhs);
          filterstring += String::FromChar(0);
          filterstring += String(rhs);
          filterstring += String::FromChar(0);
        }
        else if(rhs.expr_length() >= 1 && rhs[0] == PMATH_SYMBOL_LIST) {
          bool all_strings = true;
          
          for(size_t j = rhs.expr_length(); j > 0; --j) {
            if(!rhs[j].is_string()) {
              all_strings = false;
              break;
            }
          }
          
          if(all_strings) {
            filterstring += String(lhs);
            filterstring += String::FromChar(0);
            filterstring += String(rhs[1]);
            for(size_t j = 2; j <= rhs.expr_length(); ++j) {
              filterstring += ';';
              filterstring += String(rhs[j]);
            }
          }
        }
      }
    }
  }
  
  if(filterstring.length() > 0) {
    filterstring += String::FromChar(0);
    
    data.lpstrFilter  = (const WCHAR *)filterstring.buffer();
    data.nFilterIndex = 1;
  }
  
  if(initialfile.length() > 0) {
    size_t len = initialfile.length();
    
    if(len >= sizeof(filenamebuffer) / sizeof(WCHAR))
      len = sizeof(filenamebuffer) / sizeof(WCHAR) - 1;
      
    memcpy(filenamebuffer, initialfile.buffer(), sizeof(WCHAR) * len);
    filenamebuffer[len] = '\0';
  }
  else {
    filenamebuffer[0] = '\0';
  }
  
  data.lpstrFile = filenamebuffer;
  data.nMaxFile  = sizeof(filenamebuffer) / sizeof(WCHAR);
  
  if(title.is_valid()) {
    titlestring = title + String::FromChar(0);
    
    data.lpstrTitle = (const WCHAR *)titlestring.buffer();
  }
  
  Box *box = Application::get_evaluation_box();
  if(box) {
    Document *doc = box->find_parent<Document>(true);
    
    if(doc) {
      Win32Widget *widget = dynamic_cast<Win32Widget *>(doc->native());
      
      if(widget) {
        data.hwndOwner = GetAncestor(widget->hwnd(), GA_ROOT);
      }
    }
  }
  
  if( ( save && GetSaveFileNameW(&data)) ||
      (!save && GetOpenFileNameW(&data)))
  {
    if(data.nFileOffset > 0 && data.lpstrFile[data.nFileOffset - 1] == '\0') {
      String dir = String::FromUcs2((const uint16_t *)data.lpstrFile);
      
      Gather g;
      
      int i = data.nFileOffset;
      while(i < (int)data.nMaxFile && data.lpstrFile[i]) {
        String name = String::FromUcs2((const uint16_t *)data.lpstrFile + i);
        
        Gather::emit(dir + "\\" + name);
        
        i += name.length() + 1;
      }
      
      return g.end();
    }
    
    return String::FromUcs2((const uint16_t *)data.lpstrFile);
  }
  
  DWORD err = CommDlgExtendedError();
  if(err)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return Symbol(PMATH_SYMBOL_CANCELED);
}

//} ... class Win32FileDialog
