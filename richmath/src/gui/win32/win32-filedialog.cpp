#include <gui/win32/win32-filedialog.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <gui/win32/win32-widget.h>

#include <windows.h>


using namespace richmath;
using namespace pmath;


namespace richmath {
  class Win32FileDialogImpl {
    private:
      Win32FileDialog &self;
    public:
      Win32FileDialogImpl(Win32FileDialog &_self): self(_self) {}
      
    public:
      void try_set_default_ext(String ext) {
        if(self._default_extension_z.length() > 0 || ext.length() == 0)
          return;
          
        const uint16_t *buf = ext.buffer();
        for(int i = ext.length() - 1; i >= 0; --i) {
          if(buf[i] == '*' || buf[i] == '?')
            return;
        }
        
        self._default_extension_z = ext + String::FromChar(0);
      }
      
    public:
      void add_filter(Expr caption, Expr extensions) {
        if(!caption.is_string())
          return;
        
        if(self._filters_z.length() > 0) {
          assert_double_zero_terminated(self._filters_z);
          self._filters_z = self._filters_z.part(0, self._filters_z.length() - 1);
        }
        
        if(extensions.is_string()) {
          String ext(extensions);
          
          self._filters_z += String(caption);
          self._filters_z += String::FromChar(0);
          self._filters_z += ext;
          self._filters_z += String::FromChar(0);
          
          if(ext.starts_with("*."))
            try_set_default_ext(ext.part(2));
        }
        else if(extensions.expr_length() >= 1 && extensions[0] == PMATH_SYMBOL_LIST) {
          bool all_strings = true;
          
          for(size_t j = extensions.expr_length(); j > 0; --j) {
            if(!extensions[j].is_string()) {
              all_strings = false;
              break;
            }
          }
          
          if(all_strings) {
            String ext1(extensions[1]);
            self._filters_z += String(caption);
            self._filters_z += String::FromChar(0);
            self._filters_z += ext1;
            
            for(size_t j = 2; j <= extensions.expr_length(); ++j) {
              self._filters_z += ';';
              self._filters_z += String(extensions[j]);
            }
            self._filters_z += String::FromChar(0);
            
            if(ext1.starts_with("*."))
              try_set_default_ext(ext1.part(2));
          }
        }
        
        if(self._filters_z.length() > 0)
          self._filters_z += String::FromChar(0);
      }
    
    private:
      static void assert_double_zero_terminated(String s) {
        assert(s.length() >= 2);
        assert(s.buffer()[s.length() - 1] == 0);
        assert(s.buffer()[s.length() - 2] == 0);
      }
      
      static void assert_zero_terminated(String s) {
        assert(s.length() >= 1);
        assert(s.buffer()[s.length() - 1] == 0);
      }
      
    public:
      Expr show_dialog() {
        OPENFILENAMEW data;
        static WCHAR filenamebuffer[4096];
        
        memset(&data, 0, sizeof(data));
        data.lStructSize = sizeof(data);
        
        if(self._to_save)
          data.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        else
          data.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
        if(self._filters_z.length() > 0) {
          assert_double_zero_terminated(self._filters_z);
          data.lpstrFilter  = (const WCHAR *)self._filters_z.buffer();
          data.nFilterIndex = 1;
        }
        
        if(self._default_extension_z.length() > 0) {
          assert_zero_terminated(self._default_extension_z);
          data.lpstrDefExt = (const WCHAR *)self._default_extension_z.buffer();
        }
        
        if(self._initialfile.length() > 0) {
          size_t len = self._initialfile.length();
          
          if(len >= sizeof(filenamebuffer) / sizeof(WCHAR))
            len = sizeof(filenamebuffer) / sizeof(WCHAR) - 1;
            
          memcpy(filenamebuffer, self._initialfile.buffer(), sizeof(WCHAR) * len);
          filenamebuffer[len] = '\0';
        }
        else {
          filenamebuffer[0] = '\0';
        }
        
        data.lpstrFile = filenamebuffer;
        data.nMaxFile  = sizeof(filenamebuffer) / sizeof(WCHAR);
        
        if(self._title_z.is_valid()) {
          assert_zero_terminated(self._title_z);
          data.lpstrTitle = (const WCHAR *)self._title_z.buffer();
        }
        
        data.hwndOwner = get_dialog_owner();
        
        if( ( self._to_save && GetSaveFileNameW(&data)) ||
            (!self._to_save && GetOpenFileNameW(&data)))
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
  
    private:
      HWND get_dialog_owner() {
        Box *box = Application::get_evaluation_box();
        if(!box)
          box = get_current_document();
          
        if(!box) 
          return nullptr;
        
        Document *doc = box->find_parent<Document>(true);
        if(!doc) 
          return nullptr;
        
        Win32Widget *widget = dynamic_cast<Win32Widget *>(doc->native());
        if(widget) 
          return GetAncestor(widget->hwnd(), GA_ROOT);
        
        return nullptr;
      }
  };
}

//{ class Win32FileDialog ...

Win32FileDialog::Win32FileDialog(bool to_save): _to_save(to_save) {
}

Win32FileDialog::~Win32FileDialog() {
}

void Win32FileDialog::set_title(String title) {
  if(title.is_valid())
    _title_z = title + String::FromChar(0);
}

void Win32FileDialog::set_filter(Expr filter) {
  if(filter.expr_length() == 0 || filter[0] != PMATH_SYMBOL_LIST)
    return;
  
  for(size_t i = 1; i <= filter.expr_length(); ++i) {
    Expr rule = filter[i];
    
    if(rule.is_rule()) 
      Win32FileDialogImpl(*this).add_filter(rule[1], rule[2]);
  }
}

void Win32FileDialog::set_initial_file(String initialfile) {
  if(initialfile.length() > 0)
    _initialfile = initialfile;
}

Expr Win32FileDialog::show_dialog() {
  return Win32FileDialogImpl(*this).show_dialog();
}

//} ... class Win32FileDialog
