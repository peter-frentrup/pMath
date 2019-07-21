#include <gui/font-dialog.h>
#include <eval/application.h>


using namespace richmath;


Expr richmath_eval_FrontEnd_FontDialog(Expr expr) {
  return FontDialog::run(std::move(expr));
}

//{ class FontDialog ...

Expr FontDialog::run(Expr style_expr) {
  SharedPtr<Style> initial_style;
  
  if(style_expr.expr_length() > 0)
    initial_style = new Style(std::move(style_expr));
    
  AutoGuiWait timer;
  return show(initial_style);
}

//} ... class FontDialog
