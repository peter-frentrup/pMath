#include <boxes/graphics/dynamicgraphicsbox.h>

#include <boxes/graphics/graphicserrorbox.h>

#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <eval/observable.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String DollarContext_namespace;
}}

extern pmath_symbol_t richmath_System_DynamicBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_Internal_DynamicRemove;

namespace richmath {
  class DynamicGraphicsBox::Impl {
    public:
      Impl(DynamicGraphicsBox &self) : self{self} {}
      
      void ensure_init();
      void load_content(Expr expr, BoxInputFlags opts);
      
    private:
      DynamicGraphicsBox &self;
  };
}

//{ class DynamicGraphicsBox ...

DynamicGraphicsBox::DynamicGraphicsBox()
  : base(),
    dynamic(this, Expr()),
    _content(nullptr),
    _style(new StyleData())
{
}

DynamicGraphicsBox::~DynamicGraphicsBox() {
  if(Expr deinit = get_own_style(InternalDeinitialization)) {
    if(deinit != richmath_System_None)
      Application::interrupt_wait(PMATH_CPP_MOVE(deinit), Application::dynamic_timeout);
  }
  
  if(_content) _content->safe_destroy();
  
  // TODO: DynamicRemove(...) should actually be called on every box that did any Dynamic(...)
  //       Similar for Observable::unregister_oberserver()
  Application::interrupt_wait_for(
    Call(Symbol(richmath_Internal_DynamicRemove), id().to_pmath_raw()), 
    0,
    Application::interrupt_timeout);
  Observable::unregister_oberserver(id());
}

GraphicsElement *DynamicGraphicsBox::convert_to_literal() {
  if(GraphicsElement *child = _content) {
    _content = nullptr;
    set_style_parent_of_child(child, style_parent());
    safe_destroy();
    return child->convert_to_literal();
  }
  else {
    GraphicsElementCollection *res = new GraphicsElementCollection(style_parent());
    safe_destroy();
    return res;
  }
}

bool DynamicGraphicsBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_DynamicBox))
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  reset_style();
  _style.add_pmath(options);
  
  expr.set(0, Symbol(richmath_System_Dynamic)); // TODO: update the Dynamic expr when a style changes
  
  if(dynamic.expr() != expr || has(opts, BoxInputFlags::ForceResetDynamic)){
    dynamic = expr;
    must_update(true);
  }
  
  Expr cached = get_own_style(CachedValue);
  if(cached)
    Impl(*this).load_content(PMATH_CPP_MOVE(cached), opts);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

DynamicGraphicsBox *DynamicGraphicsBox::try_create(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_DynamicBox))
    return nullptr;
    
  if(expr.expr_length() < 1)
    return nullptr;
    
  DynamicGraphicsBox *box = new DynamicGraphicsBox();
  if(!box->try_load_from_object(PMATH_CPP_MOVE(expr), opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

GraphicsElement *DynamicGraphicsBox::create_or_error(Expr expr, BoxInputFlags opts) {
  if(DynamicGraphicsBox *box = try_create(expr, opts))
    return box;
  
  if(expr.expr_length() < 1)
    return new GraphicsErrorBox(expr, GraphicsErrorBox::message_argxxx(expr, 1, SIZE_MAX));
  
  return new GraphicsErrorBox(expr, GraphicsErrorBox::message_badarg(expr));
}

void DynamicGraphicsBox::find_extends(GraphicsBounds &bounds) {
  if(_content)
    _content->find_extends(bounds);
}

void DynamicGraphicsBox::paint(GraphicsDrawingContext &gc) {
  Impl(*this).ensure_init();
  
  if(must_update()) {
    must_update(false);
    
    if(_style) {
      dynamic.synchronous_updating((AutoBoolValues)get_own_style(SynchronousUpdating, dynamic.synchronous_updating()));
      // TODO: update TrackedSymbols setting inside dynamic from our style
    }
    
    Expr result;
    if(dynamic.get_value(&result))
      dynamic_finished(Expr(), result);
  }
  
  if(_content) {
    _content->paint(gc);
  }
}

void DynamicGraphicsBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  base::dynamic_updated();
}

void DynamicGraphicsBox::dynamic_finished(Expr info, Expr result) {
  BoxInputFlags opt = BoxInputFlags::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxInputFlags::FormatNumbers;
  
  Impl(*this).load_content(PMATH_CPP_MOVE(result), opt);
  
//  if(find_parent<Document>(false))
//    content()->after_insertion();
  
  request_repaint_all();
}

Expr DynamicGraphicsBox::to_pmath_impl(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Literal)) {
    if(!_content)
      return Expr();
      
    return _content->to_pmath(flags);
  }
  
  Gather g;
  Gather::emit(dynamic.expr()[1]);
  _style.emit_to_pmath(false);
  Expr expr = g.end();
  
  expr.set(0, Symbol(richmath_System_DynamicBox));
  return expr;
}

//} ... class DynamicGraphicsBox

//{ class DynamicGraphicsBox::Impl ...

void DynamicGraphicsBox::Impl::ensure_init() {
// See AbstractDynamicBox::ensure_init()

  if(self.get_own_style(InternalDeinitialization))
    return;
    
  String ctx = EvaluationContexts::resolve_context(&self);
  
  Expr deinit = self.get_own_style(Deinitialization);
  deinit = EvaluationContexts::make_context_block(
                  EvaluationContexts::replace_symbol_namespace(
                    self.prepare_dynamic(PMATH_CPP_MOVE(deinit)),
                    strings::DollarContext_namespace, 
                    ctx),
                  ctx);
  if(deinit)
    self._style.set(InternalDeinitialization, PMATH_CPP_MOVE(deinit));
  else
    self._style.set(InternalDeinitialization, Symbol(richmath_System_None));
  
  Expr init_call = self.get_own_style(Initialization);
  if(init_call == richmath_System_None)
    init_call = Expr();
  
  if(Expr vars = self.get_own_style(DynamicLocalValues))
    init_call = List(PMATH_CPP_MOVE(vars), PMATH_CPP_MOVE(init_call));
  
  if(init_call) {
    init_call = EvaluationContexts::make_context_block(
                  EvaluationContexts::replace_symbol_namespace(
                    self.prepare_dynamic(
                      PMATH_CPP_MOVE(init_call)), 
                    strings::DollarContext_namespace, 
                    ctx),
                  ctx);
    
    Application::interrupt_wait_for(PMATH_CPP_MOVE(init_call), &self, Application::dynamic_timeout);
  }
}

void DynamicGraphicsBox::Impl::load_content(Expr expr, BoxInputFlags opts) {
  if(!self._content || !self._content->try_load_from_object(expr, opts)) {
    if(self._content) self._content->safe_destroy();
    
    self._content = GraphicsElement::create(expr, opts);
    self.set_style_parent_of_child(self._content, &self);
  }
}

//} ... class DynamicGraphicsBox::Impl
