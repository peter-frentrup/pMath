#include <gui/font-dialog.h>
#include <eval/application.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_List;


Expr richmath_eval_FrontEnd_FontDialog(Expr expr) {
  return FontDialog::run(PMATH_CPP_MOVE(expr));
}

//{ class FontDialog ...

Expr FontDialog::run(Expr style_expr) {
  SharedPtr<Style> initial_style;
  
  if(style_expr.expr_length() > 0) {
    style_expr.set(0, Symbol(richmath_System_List));
    initial_style = new Style(PMATH_CPP_MOVE(style_expr));
  }
  
  AutoGuiWait timer;
  return show(initial_style);
}

//} ... class FontDialog
