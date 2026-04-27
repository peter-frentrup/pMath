#include <gui/win32/win32-colordialog.h>

#include <windows.h>

#include <eval/application.h>

#include <util/autovaluereset.h>

#include <gui/documents.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-widget.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

namespace {
  struct ColorDialogHook {
    static ColorDialogHook *current_hook;
    static UINT_PTR CALLBACK static_hook_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  public:
    bool dark_mode;
    
  private:
    UINT_PTR hook_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
  };
}

//{ class Win32ColorDialog ...

static COLORREF default_init_color = 0;
static COLORREF custom_colors[16] = {0};

Expr Win32ColorDialog::show(Color initialcolor) {
  CHOOSECOLORW data;
  ColorDialogHook hook {};
  
  AutoValueReset<ColorDialogHook*> auto_hook(ColorDialogHook::current_hook);
  ColorDialogHook::current_hook = &hook;
  
  memset(&data, 0, sizeof(data));
  data.lStructSize = sizeof(data);
  
  data.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN | CC_ENABLEHOOK;
  data.lpfnHook = ColorDialogHook::static_hook_proc;
  
  if(initialcolor.is_valid()) 
    data.rgbResult = initialcolor.to_bgr24();
  else
    data.rgbResult = default_init_color;
    
  data.lpCustColors = custom_colors; // todo: save these at application exit
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  if(!box)
    box = Documents::selected_document();
  
  if(box) {
    if(auto doc = box->find_parent<Document>(true)) {
      if(auto widget = dynamic_cast<Win32Widget*>(doc->native())) {
        data.hwndOwner = GetAncestor(widget->hwnd(), GA_ROOT);
      }
      hook.dark_mode = doc->native()->is_using_dark_mode();
    }
  }
  
  if(ChooseColorW(&data)) {
    default_init_color = data.rgbResult; // actually BBGGRR
    
    return Color::from_bgr24(default_init_color).to_pmath();
  }
  
  if(DWORD err = CommDlgExtendedError())
    pmath_debug_print("[CommDlgExtendedError %d]", err);
    
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class Win32ColorDialog

//{ class ColorDialogHook ...

ColorDialogHook *ColorDialogHook::current_hook = nullptr;

UINT_PTR CALLBACK ColorDialogHook::static_hook_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if(current_hook)
    return current_hook->hook_proc(hwnd, msg, wParam, lParam);
  return 0;
}

UINT_PTR ColorDialogHook::hook_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch(msg) {
    case WM_INITDIALOG: {
      Win32Themes::try_set_dark_mode_frame(hwnd, dark_mode);
      Win32Themes::try_set_dark_mode_theme_recursive(hwnd, dark_mode);
    } break;
    
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wParam;
      if(dark_mode) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(0, 0, 0));
        SetDCBrushColor(hdc, RGB(0, 0, 0));
        return (UINT_PTR)GetStockObject(DC_BRUSH);
      }
      else
        return 0;
    } break;
    
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: {
      HDC hdc = (HDC)wParam;
      COLORREF bg = Win32ControlPainter::win32_painter.win32_button_face_color(dark_mode).to_bgr24();
      SetTextColor(hdc, dark_mode ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));
      SetBkColor(hdc, bg);
      SetDCBrushColor(hdc, bg);
    } return (UINT_PTR)GetStockObject(DC_BRUSH);
    
//    default:
//      pmath_debug_print("[ColorDialog: %p 0x%04x %p %p]\n", hwnd, msg, (void*)wParam, (void*)lParam);
//      break;
  }
  
  return 0;
}

//} ... class ColorDialogHook
