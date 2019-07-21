#include <gui/color-dialog.h>

#include <eval/application.h>

#include <util/style.h>


using namespace richmath;


Expr richmath_eval_FrontEnd_ColorDialog(Expr expr) {
  return ColorDialog::run(std::move(expr));
}

//{ class ColorDialog ...

Expr ColorDialog::run(Expr expr) {
  int initcolor = -1;
  
  if(expr.expr_length() >= 1)
    initcolor = pmath_to_color(expr[1]);
    
  AutoGuiWait timer;
  return show(initcolor);
}

//} ... class ColorDialog
