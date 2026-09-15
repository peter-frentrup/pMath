#include <gui/fltk/mfltk-filedialog.h>

#include <FL/Fl.H>
#include <FL/Fl_ask.H>
#include <FL/Fl_Native_File_Chooser.H>


using namespace pmath;
using namespace richmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

//{ class MathFltkFileDialog ...

MathFltkFileDialog::MathFltkFileDialog(bool to_save)
: _to_save{to_save}
{
}

MathFltkFileDialog::~MathFltkFileDialog() {
}
      
void MathFltkFileDialog::set_title(pmath::String title) {
}

void MathFltkFileDialog::set_filter(pmath::Expr filter) {
}

void MathFltkFileDialog::set_initial_file(pmath::String initialfile) {
}

Expr MathFltkFileDialog::show_dialog() {
  fl_alert("File dialog not yet implemented for FLTK backend");
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class MathFltkFileDialog
