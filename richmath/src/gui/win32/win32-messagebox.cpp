#include <gui/win32/win32-messagebox.h>

#include <gui/win32/win32-themes.h>
#include <gui/win32/win32-widget.h>

#include <eval/binding.h>
#include <resources.h>


using namespace richmath;


namespace {
  class TaskDialogConfig: public TASKDIALOGCONFIG {
    public:
      bool dark_mode;
      
    public:
      TaskDialogConfig();
    
    protected:
      HRESULT callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
      void on_created(HWND hwnd);
      void on_dialog_created(HWND hwnd);
      
    private:
      static HRESULT CALLBACK static_callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData);
  };
}


HRESULT try_TaskDialogIndirect(const TASKDIALOGCONFIG *pTaskConfig, int *pnButton, int *pnRadioButton, BOOL *pfVerificationFlagChecked) {
  // MINGW's libcomctl32.a does not provide TaskDialogIndirect
  
  HRESULT result = E_FAIL;
  HMODULE comctl32 = LoadLibrary("comctl32.dll");
  if(comctl32) {
    HRESULT (WINAPI *p_TaskDialogIndirect)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
    p_TaskDialogIndirect = (HRESULT (WINAPI *)(const TASKDIALOGCONFIG *, int *, int *, BOOL *))
                           GetProcAddress(comctl32, "TaskDialogIndirect");
    
    if(p_TaskDialogIndirect) {
      result = p_TaskDialogIndirect(pTaskConfig, pnButton, pnRadioButton, pfVerificationFlagChecked);
    }
    
    FreeLibrary(comctl32);
  }
  return result;
}

YesNoCancel richmath::win32_ask_save(Document *doc, String question) {
  question+= String::FromChar(0);
  const wchar_t *str = (const wchar_t*)question.buffer();
  if(!str)
    str = L"Save changes?";
  
  const wchar_t *title = L"Richmath";
  
  TASKDIALOG_BUTTON buttons[] = {
    { IDYES,    L"&Save" },
    { IDNO,     L"Do&n't save" },
    { IDCANCEL, L"&Cancel" }
  };
  TaskDialogConfig config;
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszMainIcon = MAKEINTRESOURCEW(ICO_APP_MAIN);
  config.pszWindowTitle = title;
  config.pszMainInstruction = str;
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDYES;
  
  if(doc) {
    if(Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native())) {
      config.dark_mode = wid->has_dark_background();
      config.hwndParent = wid->hwnd();
      while(auto parent = GetParent(config.hwndParent))
        config.hwndParent = parent;
    }
  }
  
  
  int result = 0;
  if(!HRbool(try_TaskDialogIndirect(&config, &result, nullptr, nullptr))) {
    result = MessageBoxW(config.hwndParent, str, title, MB_YESNOCANCEL | MB_TASKMODAL);
  }
  
  switch(result) {
    case IDYES: return YesNoCancel::Yes;
    case IDNO:  return YesNoCancel::No;
    default:    return YesNoCancel::Cancel;
  }
}

Expr richmath::win32_ask_interrupt() {
  
  TASKDIALOG_BUTTON buttons[] = {
    { IDABORT,  L"&Abort current evaluation" },
    { IDRETRY,  L"&Enter subsession\n"
                L"You can exit the subsession with Return() to continue the current evaluation or Return(Abort()) to abort it."},
    { IDIGNORE, L"&Continue evaluation" }
  };
  TaskDialogConfig config;
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_USE_COMMAND_LINKS;
  config.pszMainIcon = MAKEINTRESOURCEW(ICO_APP_MAIN); // TD_WARNING_ICON
  config.pszWindowTitle = L"Richmath";
  config.pszMainInstruction = L"An interrupt occurred.";
  //config.pszContent = L"...";
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDIGNORE;
  
  Document *doc = nullptr;
  Box *box = Application::find_current_job();
  if(box)
    doc = box->find_parent<Document>(true);
  
  if(!doc)
    doc = get_current_document();
  
  if(Win32Widget *wid = doc ? dynamic_cast<Win32Widget*>(doc->native()) : nullptr) {
    config.dark_mode = wid->has_dark_background();
    config.hwndParent = wid->hwnd();
    while(auto parent = GetParent(config.hwndParent))
      config.hwndParent = parent;
  }
  
  int result = 0;
  if(!HRbool(try_TaskDialogIndirect(&config, &result, nullptr, nullptr))) {
    result = MessageBoxW(
      config.hwndParent, 
      L"An interrupt occurred. Choose:\r\n"
      L"Abort \t to abort the evaluation,\r\n"
      L"Retry \t to enter an interactive dialog,\r\n"
      L"Ignore \t to continue normally.", 
      config.pszWindowTitle, 
      MB_ABORTRETRYIGNORE | MB_DEFBUTTON3 | MB_ICONWARNING);
  }
  
  switch(result) {
    case IDIGNORE:
    case IDCANCEL:
    case IDCONTINUE:
      return Expr();
    
    case IDABORT:
      return Call(Symbol(PMATH_SYMBOL_ABORT));
    
    case IDRETRY:
    case IDTRYAGAIN:
      return Call(Symbol(PMATH_SYMBOL_DIALOG));
  }
  
  return Expr();
}

//{ class TaskDialogConfig ...

TaskDialogConfig::TaskDialogConfig()
: TASKDIALOGCONFIG{0},
  dark_mode{false}
{
  cbSize = sizeof(TASKDIALOGCONFIG);
  pfCallback = static_callback;
  lpCallbackData = (LONG_PTR)this;
}

HRESULT TaskDialogConfig::callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch(msg) {
    case TDN_DIALOG_CONSTRUCTED: on_dialog_created(hwnd); break;
    case TDN_CREATED:            on_created(hwnd);        break;
    default: break;
  }
  return S_OK;
}

void TaskDialogConfig::on_created(HWND hwnd) {
  Win32Themes::try_set_dark_mode_frame(hwnd, dark_mode);
  
  // does not work (there is only one (light) TaskDialog theme in Windows 10, 1903):
//  if(Win32Themes::SetWindowTheme) {
//    if(dark_mode) {
//      Win32Themes::SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
//      //Win32Themes::SetWindowTheme(hwnd, L"DarkMode", nullptr);
//    }
//    else {
//      Win32Themes::SetWindowTheme(hwnd, L"Explorer", nullptr);
//    }
//    pmath_debug_print("[enum for %p]\n", hwnd);
//    struct Data {
//      bool dark_mode;
//    } data { dark_mode };
//    EnumChildWindows(
//      hwnd, 
//      [](HWND wnd, LPARAM lParam) -> BOOL {
//        Data *data = (Data*)lParam;
//        pmath_debug_print("[  child %p]\n", wnd);
//        if(data->dark_mode) {
//          Win32Themes::SetWindowTheme(wnd, L"DarkMode_Explorer", nullptr);
//        }
//        else{
//          Win32Themes::SetWindowTheme(wnd, L"Explorer", nullptr);
//        }
//        return TRUE; 
//      }, 
//      (LPARAM)&data);
//  }
}

void TaskDialogConfig::on_dialog_created(HWND hwnd) {
}

HRESULT CALLBACK TaskDialogConfig::static_callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
  TaskDialogConfig *config = (TaskDialogConfig*)lpRefData;
  return config->callback(hwnd, msg, wParam, lParam);
}

//} ... class TaskDialogConfig
