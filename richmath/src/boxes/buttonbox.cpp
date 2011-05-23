#include <boxes/buttonbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class ButtonBox ...

ButtonBox::ButtonBox(MathSequence *content)
: ContainerWidgetBox(PushButton, content)
{
}

ButtonBox *ButtonBox::create(Expr expr, int opts){
  if(expr.expr_length() < 1)
    return 0;
  
  Expr options(pmath_options_extract(expr.get(), 1));
  
  if(options.is_null())
    return 0;
    
  ButtonBox *box = new ButtonBox(new MathSequence);
  box->content()->load_from_object(expr[1], opts);
  
  if(options != PMATH_UNDEFINED){
    if(box->style)
      box->style->add_pmath(options);
    else
      box->style = new Style(options);
  }
  
  return box;
}

bool ButtonBox::expand(const BoxSize &size){
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void ButtonBox::resize(Context *context){
  int bf = get_style(ButtonFrame, -1);
  if(bf >= 0)
    type = (ContainerType)bf;
  
  float old_width = context->width;
  context->width = HUGE_VAL;
  
  ContainerWidgetBox::resize(context);
  
  context->width = old_width;
}

Expr ButtonBox::to_pmath(bool parseable){
  Gather g;
  
  g.emit(_content->to_pmath(parseable));
  
  if(style)
    style->emit_to_pmath(false, false);
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_BUTTONBOX));
  return e;
}

void ButtonBox::on_mouse_down(MouseEvent &event){
  animation = 0;
  
  ContainerWidgetBox::on_mouse_down(event);
}

void ButtonBox::on_mouse_move(MouseEvent &event){
  Document *doc = find_parent<Document>(false);
  
  if(mouse_inside && doc){
    if(type == FramelessButton)
      doc->native()->set_cursor(FingerCursor);
    else
      doc->native()->set_cursor(DefaultCursor);
  }
  
  ContainerWidgetBox::on_mouse_move(event);
}

void ButtonBox::on_mouse_up(MouseEvent &event){
  if(event.left){
    request_repaint_all();
    
    if(mouse_inside && mouse_left_down)
      click();
  }
  
  ContainerWidgetBox::on_mouse_up(event);
}

void ButtonBox::click(){
  Expr fn = get_style(ButtonFunction);
  
  if(!fn.is_null()){
    String method = get_style(Method);
    
    fn = Call(
      fn,
      Call(
        Symbol(PMATH_SYMBOL_BOXDATA),
        _content->to_pmath(false)));
    
    if(method.equals("Preemptive")){
      Application::execute_for(fn, this, Application::button_timeout);
    }
    else
      Application::add_job(new EvaluationJob(fn, this));
  }
}

//} ... class ButtonBox
