#include <gui/color-dialog.h>

#include <eval/application.h>

#include <util/style.h>


using namespace richmath;


Expr richmath_eval_FrontEnd_ColorDialog(Expr expr) {
  return ColorDialog::run(PMATH_CPP_MOVE(expr));
}

//{ class ColorDialog ...

Expr ColorDialog::run(Expr expr) {
  Color initcolor = Color::None;
  
  if(expr.expr_length() >= 1)
    initcolor = Color::from_pmath(expr[1]);
    
  AutoGuiWait timer;
  return show(initcolor);
}

//} ... class ColorDialog
