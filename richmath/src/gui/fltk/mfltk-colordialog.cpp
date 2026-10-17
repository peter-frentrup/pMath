#include <gui/fltk/mfltk-colordialog.h>

#include <FL/Fl.H>
#include <FL/Fl_ask.H>
#include <FL/Fl_Color_Chooser.H>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

static Color last_color = Color::White;

//{ class MathFltkColorDialog ...

Expr MathFltkColorDialog::show_impl(Color initialcolor) {
  if(initialcolor == Color::None)
    initialcolor = last_color;
  
  double r = initialcolor.red();
  double g = initialcolor.green();
  double b = initialcolor.blue();
  
  // TODO: dynamically update set_color(...) while the dialog is open
  //        i.e. use custom dialog based on Fl_Color_Chooser widget
  int answer = fl_color_chooser("Richmath", r, g, b, 0);
  
  if(answer) {
    Color result = Color::from_rgb(r, g, b);
    
    last_color = result;
    
    return result.to_pmath();
  }
  
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class MathFltkColorDialog
