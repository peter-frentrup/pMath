#include <gui/win32/win32-colordialog.h>

#include <windows.h>

#include <eval/application.h>
#include <gui/win32/win32-widget.h>


using namespace richmath;
using namespace pmath;


//{ class Win32ColorDialog ...

static COLORREF default_init_color = 0;
static COLORREF custom_colors[16] = {0};

Expr Win32ColorDialog::show(int intialcolor) {
  CHOOSECOLORW data;
  
  memset(&data, 0, sizeof(data));
  data.lStructSize = sizeof(data);
  
  data.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;
  
  if(intialcolor >= 0) {
    data.rgbResult = ((intialcolor & 0xFF0000) >> 16) |
                     ( intialcolor & 0x00FF00)        |
                     ((intialcolor & 0x0000FF) << 16);
  }
  else
    data.rgbResult = default_init_color;
    
  data.lpCustColors = custom_colors; // todo: save these at application exit
  
  Box *box = Application::get_evaluation_box();
  if(box){
    Document *doc = box->find_parent<Document>(true);
    
    if(doc){
      Win32Widget *widget = dynamic_cast<Win32Widget*>(doc->native());
      
      if(widget){
        data.hwndOwner = GetAncestor(widget->hwnd(), GA_ROOT);
      }
    }
  }
  
  if(ChooseColorW(&data)) {
    default_init_color = data.rgbResult;
    
    int col = ((default_init_color & 0xFF0000) >> 16) |
              ( default_init_color & 0x00FF00)        |
              ((default_init_color & 0x0000FF) << 16);
              
    return color_to_pmath(col);
  }
  
  DWORD err = CommDlgExtendedError();
  if(err)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return Symbol(PMATH_SYMBOL_CANCELED);
}

//} ... class Win32ColorDialog
