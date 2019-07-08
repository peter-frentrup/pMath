#include <gui/win32/win32-messagebox.h>

#include <gui/win32/win32-widget.h>


using namespace richmath;


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
  HWND owner_window = nullptr;
  
  if(doc) {
    Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
    if(wid) {
      owner_window = wid->hwnd();
      while(GetParent(owner_window) != nullptr)
        owner_window = GetParent(owner_window);
    }
  }
  
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
  TASKDIALOGCONFIG config = {0};
  
  config.cbSize = sizeof(config);
  config.hwndParent = owner_window;
  config.hInstance = GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszWindowTitle = title;
  config.pszMainInstruction = str;
  config.cButtons = sizeof(buttons) / sizeof(buttons[0]);
  config.pButtons = buttons;
  config.nDefaultButton = IDYES;
  
  int result = 0;
  if(!HRbool(try_TaskDialogIndirect(&config, &result, nullptr, nullptr))) {
    result = MessageBoxW(owner_window, str, title, MB_YESNOCANCEL | MB_TASKMODAL);
  }
  
  switch(result) {
    case IDYES: return YesNoCancel::Yes;
    case IDNO:  return YesNoCancel::No;
    default:    return YesNoCancel::Cancel;
  }
}
