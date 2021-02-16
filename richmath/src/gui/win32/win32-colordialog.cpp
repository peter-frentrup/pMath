#include <gui/win32/win32-colordialog.h>

#include <windows.h>

#include <eval/application.h>

#include <gui/documents.h>
#include <gui/win32/win32-widget.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

//{ class Win32ColorDialog ...

static COLORREF default_init_color = 0;
static COLORREF custom_colors[16] = {0};

Expr Win32ColorDialog::show(Color initialcolor) {
  CHOOSECOLORW data;
  
  memset(&data, 0, sizeof(data));
  data.lStructSize = sizeof(data);
  
  data.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;
  
  if(initialcolor.is_valid()) 
    data.rgbResult = initialcolor.to_bgr24();
  else
    data.rgbResult = default_init_color;
    
  data.lpCustColors = custom_colors; // todo: save these at application exit
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  if(!box)
    box = Documents::current();
  
  if(box) {
    if(auto doc = box->find_parent<Document>(true)) {
      if(auto widget = dynamic_cast<Win32Widget*>(doc->native())) {
        data.hwndOwner = GetAncestor(widget->hwnd(), GA_ROOT);
      }
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
