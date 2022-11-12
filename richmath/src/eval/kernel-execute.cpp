#include <eval/application.h>
#include <eval/job.h>
#include <boxes/box.h>
#include <gui/document.h>
#include <gui/documents.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String Button;
  extern String Preemptive;
  extern String Queued;
}}

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Method;
extern pmath_symbol_t richmath_FrontEnd_KernelExecute;

Expr richmath_eval_FrontEnd_KernelExecute(Expr expr) {
  /*  FrontEnd`KernelExecute(expr)
      FrontEnd`KernelExecute(expr, options)
      
    options:
      Method -> "Preemptive"  (default)
      Method -> "Queued"
   */
  size_t exprlen = expr.expr_length();
  if(exprlen < 1)
    return Symbol(richmath_System_DollarFailed);
  
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_FAIL));
  if(!options)
    return Symbol(richmath_System_DollarFailed);
  
  FrontEndObject *src = Application::get_evaluation_object();
  if(!src) {
    if(auto doc = Documents::selected_document()) {
      src = doc->selection_box();
      if(!src)
        src = doc;
    }
  }
  
  Expr meth(pmath_option_value(richmath_FrontEnd_KernelExecute, richmath_System_Method, options.get()));
  expr = expr[1];
  options = {};
  
  if(src)
    expr = src->prepare_dynamic(PMATH_CPP_MOVE(expr));
  
  if(meth == strings::Preemptive) {
    return Application::interrupt_wait_for_interactive(PMATH_CPP_MOVE(expr), src, Application::button_timeout);
  }
  else if(meth == strings::Queued) {
    Application::add_job(new EvaluationJob(PMATH_CPP_MOVE(expr), src));
    return {};
  }
  else {
    return Symbol(richmath_System_DollarFailed);
  }
}
