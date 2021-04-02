#include <gui/win32/win32-messagebox.h>

#include <gui/documents.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/win32-widget.h>

#include <boxes/section.h>

#include <resources.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String Head;
  extern String Location;
}}

extern pmath_symbol_t richmath_System_Abort;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Dialog;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_Developer_SourceLocationOpenerFunction;
extern pmath_symbol_t richmath_FE_CallFrontEnd;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_SystemOpenDirectory;

namespace {
  class TaskDialogConfig: public TASKDIALOGCONFIG {
    public:
      bool dark_mode;
      
    public:
      TaskDialogConfig();
      String register_hyperlink_action(Expr action);
    
    protected:
      HRESULT callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
      void on_created(HWND hwnd);
      void on_dialog_created(HWND hwnd);
      void on_hyperlink_clicked(HWND hwnd, const wchar_t *url);
    
    private:
      Expr hyperlink_actions;
      
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
  const wchar_t *str = question.buffer_wchar();
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

YesNoCancel richmath::win32_ask_remove_private_style_definitions(Document *doc) {
  const wchar_t *str = L"This document has private style definitions.\nAre you sure you want to replace them with a shared stylesheet?";
  
  const wchar_t *title = L"Richmath";
  
  TASKDIALOG_BUTTON buttons[] = {
    { IDYES,    L"&Delete private style definitions\nUse shared stylesheet instead" },
    { IDNO,     L"&Keep private style definitions" },
    //{ IDCANCEL, L"&Cancel" }
  };
  TaskDialogConfig config;
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszMainIcon = MAKEINTRESOURCEW(ICO_APP_MAIN);
  config.pszWindowTitle = title;
  config.pszMainInstruction = L"Replace private style definitions?";
  config.pszContent = str;
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDNO;
  
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

bool richmath::win32_ask_open_suspicious_system_file(String path) {
  const wchar_t *title = L"Richmath";
  
  TASKDIALOG_BUTTON buttons[] = {
    { IDYES, L"&Open anyway" },
    { IDNO,  L"Do &not open suspicious file" }
  };
  
  TaskDialogConfig config;
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_ENABLE_HYPERLINKS;
  config.pszMainIcon = MAKEINTRESOURCEW(ICO_APP_MAIN);
  config.pszWindowTitle = title;
  config.pszMainInstruction = L"Open suspicous file?";
  //config.pszContent = str;
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDNO;
  
  String rich_question;
  rich_question = String("The file ") + String::FromChar(0x201C);
  rich_question+= "<a href=\"";
  rich_question+= config.register_hyperlink_action(
                    Call(
                      Symbol(richmath_FE_CallFrontEnd), 
                      Call(
                        Symbol(richmath_FrontEnd_SystemOpenDirectory), 
                        path)));
  rich_question+= "\">";
  rich_question+= path;
  rich_question+= "</a>";
  rich_question+= String::FromChar(0x201D) + " has an unrecognized file extension.\n";
  rich_question+= "Do you really want to open it?";
  rich_question+= String::FromChar(0);
  
  config.pszContent = rich_question.buffer_wchar();
  
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(!doc)
    doc = Documents::current();
  
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
    result = MessageBoxW(
               config.hwndParent, 
               config.pszContent, 
               title, 
               MB_YESNO | MB_TASKMODAL);
  }
  
  return result == IDYES;
}

Expr richmath::win32_ask_interrupt(Expr stack) {
  TaskDialogConfig config;
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    doc = Documents::current();
  
  if(Win32Widget *wid = doc ? dynamic_cast<Win32Widget*>(doc->native()) : nullptr) {
    config.dark_mode = wid->has_dark_background();
    config.hwndParent = wid->hwnd();
    while(auto parent = GetParent(config.hwndParent))
      config.hwndParent = parent;
  }
  
  String content;
  if(box) {
    if(Section *sect = box->find_parent<Section>(true)) {
      content = "During evaluation of <a href=\"";
      content+= config.register_hyperlink_action(
                  Call(Symbol(richmath_FE_CallFrontEnd),
                    Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
                    Symbol(richmath_System_Automatic),
                    sect->id().to_pmath())));
      content+= "\">";
      content+= sect->get_own_style(SectionLabel, "?");
      content+= "</a>";
    }
  }
  
  String details;
  if(stack[0] == richmath_System_List && stack.expr_length() > 1) {
    Expr default_name = String("?");
    
    details = "Stack trace:";
    for(size_t i = stack.expr_length() - 1; i > 0; --i) {
      Expr frame = stack[i];
      
      details+= "\n";
      String name = frame.lookup(strings::Head, default_name).to_string();
      bool have_link = false;
      Expr location {};
      if(frame.try_lookup(strings::Location, location)) {
        location = Application::interrupt_wait(Call(Symbol(richmath_Developer_SourceLocationOpenerFunction), std::move(location)));
        
        if(location[0] == richmath_System_Function) {
          if(location.expr_length() == 1)
            location = location[1];
          else
            location = Call(location);
          
          details+= "<a href=\"";
          details+= config.register_hyperlink_action(location);
          details+= "\">";
          have_link = true;
        }
      }
      details+= name;
      if(have_link)
        details+= "</a>";
    }
  }
  
  TASKDIALOG_BUTTON buttons[] = {
    { IDABORT,  L"&Abort current evaluation" },
    { IDRETRY,  L"&Enter subsession\n"
                L"You can exit the subsession with Return() to continue the current evaluation or Return(Abort()) to abort it."},
    { IDIGNORE, L"&Continue evaluation" }
  };
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS;
  config.pszMainIcon = MAKEINTRESOURCEW(ICO_APP_MAIN); // TD_WARNING_ICON
  config.pszWindowTitle = L"Richmath";
  config.pszMainInstruction = L"An interrupt occurred.";
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDIGNORE;
  
  if(content.is_string()) {
    content+= String::FromChar(0);
    config.pszContent = content.buffer_wchar();
  }
  
  if(details.is_string()) {
    details+= String::FromChar(0);
    config.pszExpandedInformation = details.buffer_wchar();
    config.pszExpandedControlText = L"&Details";
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
      return Call(Symbol(richmath_System_Abort));
    
    case IDRETRY:
    case IDTRYAGAIN:
      return Call(Symbol(richmath_System_Dialog));
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

String TaskDialogConfig::register_hyperlink_action(Expr action) {
  if(hyperlink_actions.is_null())
    hyperlink_actions = List(std::move(action));
  else
    hyperlink_actions.append(std::move(action));
  
  return Expr(hyperlink_actions.expr_length()).to_string();
}

HRESULT TaskDialogConfig::callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch(msg) {
    case TDN_DIALOG_CONSTRUCTED: on_dialog_created(hwnd); break;
    case TDN_CREATED:            on_created(hwnd);        break;
    case TDN_HYPERLINK_CLICKED:  on_hyperlink_clicked(hwnd, (const wchar_t*)lParam); break;
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

void TaskDialogConfig::on_hyperlink_clicked(HWND hwnd, const wchar_t *url) {
  size_t i = 0;
  const wchar_t *s = url;
  while(*s >= L'0' && *s <= L'9' && i < hyperlink_actions.expr_length()) {
    i = 10*i + (*s - L'0');
    ++s;
  }
  
  if(*s == L'\0' && i >= 1 && i <= hyperlink_actions.expr_length()) {
    Expr action = hyperlink_actions[i];
    Application::interrupt_wait(action);
  }
}

HRESULT CALLBACK TaskDialogConfig::static_callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
  TaskDialogConfig *config = (TaskDialogConfig*)lpRefData;
  return config->callback(hwnd, msg, wParam, lParam);
}

//} ... class TaskDialogConfig
