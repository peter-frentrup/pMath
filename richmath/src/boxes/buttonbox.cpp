#include <boxes/buttonbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <eval/job.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;


namespace richmath { namespace strings {
  extern String Button;
  extern String Preemptive;
  extern String Queued;
}}

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_Method;

//{ class AbstractButtonBox ...

AbstractButtonBox::AbstractButtonBox(AbstractSequence *content, ContainerType _type)
  : ContainerWidgetBox(_type, content)
{
}

void AbstractButtonBox::resize_default_baseline(Context &context) {
  int bf = get_own_style(ButtonFrame, -1);
  if(bf >= 0)
    type = (ContainerType)bf;
  else
    type = default_container_type();
    
  float old_width = context.width;
  context.width = HUGE_VAL;
  
  ContainerWidgetBox::resize_default_baseline(context);
  
  context.width = old_width;
}

bool AbstractButtonBox::expand(Context &context, const BoxSize &size) {
  base::expand(context, size);
  _extents.merge(size);
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void AbstractButtonBox::on_mouse_down(MouseEvent &event) {
  animation = nullptr;
  
  base::on_mouse_down(event);
}

void AbstractButtonBox::on_mouse_move(MouseEvent &event) {
  if(mouse_inside() && enabled()) {
    if(Document *doc = find_parent<Document>(false)) {
      if(type == ContainerType::FramelessButton)
        doc->native()->set_cursor(CursorType::Finger);
      else
        doc->native()->set_cursor(CursorType::Default);
    }
  }
  
  base::on_mouse_move(event);
}

void AbstractButtonBox::on_mouse_up(MouseEvent &event) {
  if(event.left && enabled()) {
    request_repaint_all();
    
    if(mouse_inside() && mouse_left_down())
      click();
  }
  
  base::on_mouse_up(event);
}

//} ... class AbstractButtonBox

//{ class ButtonBox ...

ButtonBox::ButtonBox(AbstractSequence *content)
  : AbstractButtonBox(content)
{
}

bool ButtonBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_ButtonBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
  
  /* now success is guaranteed */
  
  content()->load_from_object(expr[1], opts);
  
  reset_style();
  style.add_pmath(options);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

Expr ButtonBox::to_pmath_symbol() { 
  return Symbol(richmath_System_ButtonBox); 
}

Expr ButtonBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::Button)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_ButtonBox));
  return e;
}

void ButtonBox::reset_style() {
  style.reset(strings::Button);
}

void ButtonBox::click() {
  if(!enabled())
    return;
  
  Expr fn = get_own_style(ButtonFunction);
  if(fn.is_null())
    return;
  
  fn = prepare_dynamic(PMATH_CPP_MOVE(fn));
  
  bool has_data;
  Expr data = get_own_style(ButtonData, Symbol(richmath_System_Inherited));
  if(data == richmath_System_Inherited) {
    has_data = false;
    data = Symbol(richmath_System_Automatic);
  }
  else {
    has_data = true;
    data = prepare_dynamic(PMATH_CPP_MOVE(data));
  }
  
  Expr arg1;
  int source = get_own_style(ButtonSource, ButtonSourceAutomatic);
  if(source == ButtonSourceAutomatic) {
    if(has_data)
      source = ButtonSourceButtonData;
    else
      source = ButtonSourceButtonContents;
  }
  
  switch(source) {
    case ButtonSourceButtonData:
      arg1 = data;
      break;
    
    case ButtonSourceButtonContents:
      arg1 = Call(
        Symbol(richmath_System_BoxData),
        _content->to_pmath(BoxOutputFlags::Default));
      break;
    
    case ButtonSourceButtonBox:
      arg1 = to_pmath(BoxOutputFlags::Default);
      break;
    
    case ButtonSourceFrontEndObject:
      arg1 = to_pmath_id();
      break;
    
    default:
      arg1 = Symbol(richmath_System_DollarFailed);
      break;
  }
  
  // Mathematica also gives a click repeat count as argument #3 and the currently 
  // pressed keyboard modifiers as argument #4
  // These should better be accessed via some CurrentValue() mechanism.
  fn = Call(PMATH_CPP_MOVE(fn), PMATH_CPP_MOVE(arg1), PMATH_CPP_MOVE(data));
  
  fn = EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(fn), this);
  
  String method = get_own_style(Method);
  if(!method) {
    method = String(pmath_option_value(richmath_System_ButtonBox, richmath_System_Method, PMATH_UNDEFINED));
  }
  
  if(method == strings::Preemptive) {
    Application::interrupt_wait_for_interactive(PMATH_CPP_MOVE(fn), this, Application::button_timeout);
  }
  else if(method == strings::Queued) {
    Application::add_job(new EvaluationJob(PMATH_CPP_MOVE(fn), this));
  }
  else if(auto doc = find_parent<Document>(false)) {
    doc->native()->beep();
  }
}

//} ... class ButtonBox
