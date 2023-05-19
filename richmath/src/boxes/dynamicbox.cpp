#include <boxes/dynamicbox.h>

#include <boxes/dynamiclocalbox.h>
#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <eval/job.h>
#include <eval/observable.h>
#include <graphics/context.h>
#include <gui/document.h>

#include <cstdio>

using namespace richmath;

namespace richmath { namespace strings {
  extern String DollarContext_namespace;
}}

extern pmath_symbol_t richmath_System_DynamicBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_Internal_DynamicRemove;

//{ class AbstractDynamicBox ...

AbstractDynamicBox::AbstractDynamicBox(AbstractSequence *content)
  : OwnerBox(content)
{
  if(!style) style = new StyleData();
}

AbstractDynamicBox::~AbstractDynamicBox() {
  if(Expr deinit = get_own_style(InternalDeinitialization)) {
    if(deinit != richmath_System_None)
      Application::interrupt_wait(PMATH_CPP_MOVE(deinit), Application::dynamic_timeout);
  }
}

VolatileSelection AbstractDynamicBox::dynamic_to_literal(int start, int end) {
  if(start > 0 || end < 1)
    return {this, start, end};
  
  content()->all_dynamic_to_literal();
  
  auto seq = dynamic_cast<AbstractSequence*>(parent());
  if(!seq)
    return {this, start, end};
    
  start = index();
  end = seq->insert(index(), content(), 0, content()->length());
  seq->remove(end, end + 1); // remove this
  return {seq, start, end};
}

void AbstractDynamicBox::reset_style() {
  style->clear();
}

void AbstractDynamicBox::before_paint_inline(Context &context) {
  ensure_init();
  base::before_paint_inline(context);
}

void AbstractDynamicBox::paint(Context &context) {
  ensure_init();
  base::paint(context);
}

void AbstractDynamicBox::ensure_init() {
  if(get_own_style(InternalDeinitialization))
    return;
    
  String ctx = EvaluationContexts::resolve_context(this);
  
  Expr deinit = get_own_style(Deinitialization);
  deinit = EvaluationContexts::make_context_block(
                  EvaluationContexts::replace_symbol_namespace(
                    prepare_dynamic(PMATH_CPP_MOVE(deinit)),
                    strings::DollarContext_namespace, 
                    ctx),
                  ctx);
  if(deinit)
    style->set(InternalDeinitialization, PMATH_CPP_MOVE(deinit));
  else
    style->set(InternalDeinitialization, Symbol(richmath_System_None));
  
  Expr init_call = get_own_style(Initialization);
  if(init_call == richmath_System_None)
    init_call = Expr();
  
  if(Expr vars = get_own_style(DynamicLocalValues))
    init_call = List(PMATH_CPP_MOVE(vars), PMATH_CPP_MOVE(init_call));
  
  if(init_call) {
    init_call = EvaluationContexts::make_context_block(
                  EvaluationContexts::replace_symbol_namespace(
                    prepare_dynamic(
                      PMATH_CPP_MOVE(init_call)), 
                    strings::DollarContext_namespace, 
                    ctx),
                  ctx);
    
    Application::interrupt_wait_for(PMATH_CPP_MOVE(init_call), this, Application::dynamic_timeout);
  }
}

//} ... class AbstractDynamicBox

//{ class DynamicBox ...

DynamicBox::DynamicBox(AbstractSequence *content)
  : base(content)
{
  must_update(true);
  dynamic.init(this, Expr());
}

DynamicBox::~DynamicBox() {
  Application::interrupt_wait_for(
    Call(Symbol(richmath_Internal_DynamicRemove), id().to_pmath_raw()), 
    0,
    Application::interrupt_timeout);
  Observable::unregister_oberserver(id());
}

bool DynamicBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_DynamicBox)
    return false;
    
  if(expr.expr_length() < 1) 
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  reset_style();
  if(options[0] == richmath_System_List) {
    style->add_pmath(options);
  }
  
  expr.set(0, Symbol(richmath_System_Dynamic)); // TODO: update the Dynamic expr when a style changes
  
  if(dynamic.expr() != expr || has(opts, BoxInputFlags::ForceResetDynamic)){
    dynamic = expr;
    must_update(true);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

MathSequence *DynamicBox::as_inline_span() { 
  return dynamic_cast<MathSequence*>(content());
}

void DynamicBox::resize_default_baseline(Context &context) {
  base::resize_default_baseline(context);
  must_resize(false);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void DynamicBox::before_paint_inline(Context &context) {
  if(must_resize()) {
    //context.canvas().save();
    //base::resize(context);
    invalidate();
    must_resize(false);
    //context.canvas().restore();
  }
  
  if(must_update()) {
    must_update(false);
    
    if(style) {
      dynamic.synchronous_updating((AutoBoolValues)get_own_style(SynchronousUpdating, dynamic.synchronous_updating()));
      // TODO: update TrackedSymbols setting inside dynamic from our style
    }
    
    Expr result;
    if(dynamic.get_value(&result)) 
      dynamic_finished(Expr(), result);
  }
  
  base::before_paint_inline(context);
}

void DynamicBox::paint_content(Context &context) {
  if(must_resize()) {
    context.canvas().save();
    base::resize(context);
    must_resize(false);
    context.canvas().restore();
  }
  
  base::paint_content(context);
  
  if(must_update()) {
    must_update(false);
    
    if(style) {
      dynamic.synchronous_updating((AutoBoolValues)get_own_style(SynchronousUpdating, dynamic.synchronous_updating()));
      // TODO: update TrackedSymbols setting inside dynamic from our style
    }
    
    Expr result;
    if(dynamic.get_value(&result)) 
      dynamic_finished(Expr(), result);
  }
}

Expr DynamicBox::to_pmath_symbol() { 
  return Symbol(richmath_System_DynamicBox); 
}

Expr DynamicBox::to_pmath_impl(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Literal))
    return content()->to_pmath(flags);
  
  Gather g;
  Gather::emit(dynamic.expr()[1]);
  style->emit_to_pmath(false);
  Expr expr = g.end();
  
  expr.set(0, Symbol(richmath_System_DynamicBox));
  return expr;
}

void DynamicBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  request_repaint_all();
}

void DynamicBox::dynamic_finished(Expr info, Expr result) {
  BoxInputFlags opt = BoxInputFlags::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxInputFlags::FormatNumbers;
    
  content()->load_from_object(result, opt);
  if(find_parent<Document>(false))
    content()->after_insertion();
  
  must_resize(true);
  invalidate();
}

bool DynamicBox::edit_selection(SelectionReference &selection, EditAction action) {
  if(get_own_style(Editable, false)) {
    if(auto p = parent())
      return p->edit_selection(selection, action);
  }
  
  return false;
}

//} ... class DynamicBox
