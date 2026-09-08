#include <gui/color-dialog.h>

#include <eval/application.h>
#include <eval/dynamic.h>

#include <util/style.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Dynamic;
extern pmath_symbol_t richmath_System_DollarCanceled;


namespace richmath {
  struct ColorDialog::Impl {
    Dynamic current_color;
    Color   initcolor;
    bool    initialized;
    
    void assign(Expr col, bool post) {
      current_color.assign(col, !initialized, true, post);
      initialized = true;
    }
  };
}


Expr richmath_eval_FrontEnd_ColorDialog(Expr expr) {
  return ColorDialog::run(PMATH_CPP_MOVE(expr));
}

//{ class ColorDialog ...

void ColorDialog::set_color(Color current) {
  impl.assign(current.to_pmath(), false);
}

Expr ColorDialog::run(Expr expr) {
  ColorDialog::Impl impl;
  impl.current_color.init(Application::front_end_session, Expr());
  impl.initcolor = Color::None;
  impl.initialized = false;
  
  ColorDialog dialog(impl);
  
  if(expr.expr_length() >= 1) {
    impl.current_color = expr[1];
    impl.initcolor = Color::from_pmath(impl.current_color.get_value_now());
  }
  
  AutoGuiWait timer;
  Expr result = dialog.show_impl(impl.initcolor);
  
  if(result == richmath_System_DollarCanceled) {
    if(impl.initialized)
      impl.assign(impl.initcolor.to_pmath(), true);
  } else {
    impl.assign(result, true);
  }
  
  return PMATH_CPP_MOVE(result);
}

//} ... class ColorDialog
