#include <gui/fltk/mfltk-colordialog.h>

#include <FL/Fl.H>
#include <FL/Fl_ask.H>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

//{ class MathFltkColorDialog ...

Expr MathFltkColorDialog::show_impl(Color initialcolor) {
  fl_alert("Color dialog not yet implemented for FLTK backend");
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class MathFltkColorDialog
