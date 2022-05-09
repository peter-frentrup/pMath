#include <gui/win32/win32-fontdialog.h>

#include <windows.h>

#include <eval/application.h>
#include <gui/documents.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/win32-widget.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;
extern pmath_symbol_t richmath_System_List;

//{ class Win32FontDialog ...

Expr Win32FontDialog::show(SharedPtr<Style> initial_style) {

  LOGFONTW logfontw;
  memset(&logfontw, 0, sizeof(LOGFONTW));
  logfontw.lfCharSet = DEFAULT_CHARSET;
  
  CHOOSEFONTW data;
  memset(&data, 0, sizeof(data));
  data.lStructSize = sizeof(data);
  data.lpLogFont   = &logfontw;
  data.Flags       = CF_SCREENFONTS | CF_NOSCRIPTSEL /*| CF_EFFECTS*/;
   
  if(initial_style) {
    data.Flags |= CF_INITTOLOGFONTSTRUCT;
    
    Expr families;
    if(initial_style->get(FontFamilies, &families)) {
      String family(families);
      
      if(families[0] == richmath_System_List){
        for(size_t i = 1;i <= families.expr_length();++i){
          family = String(families[i]);
          
          if(FontInfo::font_exists_similar(family))
            break;
        }
      }
      
      int len = family.length();
      if(len >= LF_FACESIZE)
        len = LF_FACESIZE - 1;
      memcpy(logfontw.lfFaceName, family.buffer(), len * sizeof(WCHAR));
      logfontw.lfFaceName[len] = 0;
    }
    else
      data.Flags |= CF_NOFACESEL;
      
    Length size = SymbolicSize::Invalid;
    if(initial_style->get(FontSize, &size) && size.is_explicit_abs() && size.explicit_abs_value() >= 1) {
      data.iPointSize = (int)(10 * size.explicit_abs_value() + 0.5);
      
      // ChooseFontW() is not per-monitor-DPI-aware:
      logfontw.lfHeight = -(int)(size.explicit_abs_value() * Win32HighDpi::get_dpi_for_system() / 72.0 + 0.5);
    }
    else
      data.Flags |= CF_NOSIZESEL;
      
    int weight = FontWeightPlain;
    int slant  = FontSlantPlain;
    
    bool has_weight = initial_style->get(FontWeight, &weight);
    bool has_slant  = initial_style->get(FontSlant, &slant);
    if(has_weight || has_slant) {
      logfontw.lfWeight = (weight == FontWeightBold) ? FW_BOLD : FW_NORMAL;
      logfontw.lfItalic = (slant  == FontSlantItalic);
    }
    else
      data.Flags |= CF_NOSTYLESEL;
      
  }
  else {
    data.Flags |= CF_NOFACESEL;
    data.Flags |= CF_NOSIZESEL;
    data.Flags |= CF_NOSTYLESEL;
  }
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  if(!box)
    box = Documents::selected_document();
    
  if(box) {
    if(auto doc = box->find_parent<Document>(true)) {
      if(auto widget = dynamic_cast<Win32Widget *>(doc->native())) {
        data.hwndOwner = GetAncestor(widget->hwnd(), GA_ROOT);
      }
    }
  }
  
  if(ChooseFontW(&data)) {
    SharedPtr<Style> result_style = new Style();
    
    if(!(data.Flags & CF_NOFACESEL)){
      result_style->set(FontFamilies, String::FromUcs2((const uint16_t*)logfontw.lfFaceName));
    }
    
    if(!(data.Flags & CF_NOSTYLESEL)) {
      result_style->set(FontWeight, logfontw.lfWeight >= FW_BOLD ? FontWeightBold  : FontWeightPlain);
      result_style->set(FontSlant,  logfontw.lfItalic            ? FontSlantItalic : FontSlantPlain);
    }
    
    if(!(data.Flags & CF_NOSIZESEL) && data.iPointSize > 0){
      result_style->set(FontSize, Length(data.iPointSize / 10.0));
    }
    
    Gather g;
    result_style->emit_to_pmath(false);
    return g.end();
  }
  
  if(DWORD err = CommDlgExtendedError())
    pmath_debug_print("[CommDlgExtendedError %d]", err);
    
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class Win32FontDialog
