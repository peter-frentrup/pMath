#include <gui/win32/win32-fontdialog.h>

#include <windows.h>

#include <eval/application.h>
#include <gui/documents.h>
#include <gui/win32/win32-widget.h>


using namespace richmath;
using namespace pmath;


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
      
      if(families[0] == PMATH_SYMBOL_LIST){
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
      
    float size = 0;
    if(initial_style->get(FontSize, &size) && size >= 1) {
      data.iPointSize = (int)(10 * size + 0.5);
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
  
  Box *box = Application::get_evaluation_box();
  if(!box)
    box = Documents::current();
    
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
      result_style->set(FontSize, data.iPointSize / 10.0);
    }
    
    Gather g;
    result_style->emit_to_pmath(false);
    return g.end();
  }
  
  DWORD err = CommDlgExtendedError();
  if(err)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return Symbol(PMATH_SYMBOL_CANCELED);
}

//} ... class Win32FontDialog
