#include <gui/fltk/mfltk-fontdialog.h>

#include <FL/Fl.H>
#include <FL/Fl_ask.H>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

//{ class MathFltkFontDialog ...

Expr MathFltkFontDialog::show(Style initial_style) {
  fl_alert("Font dialog not yet implemented for FLTK backend");
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class MathFltkFontDialog
