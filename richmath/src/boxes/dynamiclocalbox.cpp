#include <boxes/dynamiclocalbox.h>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/binding.h>
#include <eval/eval-contexts.h>


namespace richmath {
  class DynamicLocalBox::Impl {
    public:
      Impl(DynamicLocalBox &self) : self{self} {}
      
      static pmath_t internal_replace_symbols(pmath_t expr, const Expr &old_syms, const Expr &new_syms);
      
      Expr collect_definitions();
      
    private:
      void emit_values(Expr symbol, String eval_ctx);
      
    private:
      DynamicLocalBox &self;
  };
  
  namespace strings {
    extern String DollarContext_namespace;
  }
}

extern pmath_symbol_t richmath_System_DynamicLocalBox;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_FE_SymbolDefinitions;

using namespace richmath;

//{ class DynamicLocalBox ...

DynamicLocalBox::DynamicLocalBox()
  : base()
{
}

DynamicLocalBox::~DynamicLocalBox() {
}

bool DynamicLocalBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_DynamicLocalBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  Expr symbols = expr[1];
  if(symbols[0] != richmath_System_List)
    return false;
    
  for(auto sym : symbols.items()) {
    if(!sym.is_symbol())
      return false;
  }
  
  /* now success is guaranteed */
  
  reset_style();
  if(options != PMATH_UNDEFINED) 
    style->add_pmath(options);
  
  if(_public_symbols != symbols) {
    // TODO: call previous deinitialization
    
    _public_symbols  = symbols;
    
    /* FIXME: only introduce new private symbols for *new* public symbols, because 
        DynamicBox() will not see the change: DynamicBox::try_load_from_object() will not reset
        its `must_update` if merely our private symbol changed.
        
        To work around the issue, we will force must_update=true for all contained DynamicBox's etc.
     */
    opts|= BoxInputFlags::ForceResetDynamic;

    _private_symbols = symbols;
    for(size_t i = symbols.expr_length(); i > 0; --i) {
      Expr sym = symbols[i];
      
      sym = Expr(pmath_symbol_create_temporary(
                   pmath_symbol_name(sym.get()),
                   TRUE));
                   
      _private_symbols.set(i, sym);
    }
  }
  
  content()->load_from_object(expr[2], opts);
  
  finish_load_from_object(std::move(expr));
  return true;
}

MathSequence *DynamicLocalBox::as_inline_span() {
  return dynamic_cast<MathSequence*>(content());
}

Expr DynamicLocalBox::to_pmath_symbol() {
  return Symbol(richmath_System_DynamicLocalBox); 
}
      
Expr DynamicLocalBox::to_pmath(BoxOutputFlags flags) {
  ensure_init();
  
  if(has(flags, BoxOutputFlags::Literal))
    return content()->to_pmath(flags);
    
  Gather g;
  
  Gather::emit(_public_symbols);
  Gather::emit(content()->to_pmath(flags));
  style->set(DynamicLocalValues, Impl(*this).collect_definitions());
  style->emit_to_pmath();
  
  Expr expr = g.end();
  expr.set(0, Symbol(richmath_System_DynamicLocalBox));
  return expr;
}
      
Expr DynamicLocalBox::prepare_dynamic(Expr expr) {
  expr = Expr(Impl::internal_replace_symbols(expr.release(), _public_symbols, _private_symbols));
  return AbstractDynamicBox::prepare_dynamic(std::move(expr));
}

//} ... class DynamicLocalBox

//{ class DynamicLocalBox::Impl ...

pmath_t DynamicLocalBox::Impl::internal_replace_symbols(pmath_t expr, const Expr &old_syms, const Expr &new_syms) {
  if(pmath_is_symbol(expr)) {
    for(size_t i = old_syms.expr_length();i > 0;--i) {
      if(old_syms[i] == expr) {
        pmath_unref(expr);
        return new_syms[i].release();
      }
    }
    return expr;
  }
  
  if(pmath_is_expr(expr)) {
    for(size_t i = 0; i <= pmath_expr_length(expr); ++i) {
      pmath_t item = pmath_expr_extract_item(expr, i);
      
      item = internal_replace_symbols(item, old_syms, new_syms);
      
      expr = pmath_expr_set_item(expr, i, item);
    }
  }
  
  return expr;
}

Expr DynamicLocalBox::Impl::collect_definitions() {
  String ctx = EvaluationContexts::resolve_context(&self);
  
  Gather g;
  
  Expr unsaved_variables = self.get_own_style(UnsavedVariables);
  
  for(auto sym : self._public_symbols.items()) {
    for(auto unsaved : unsaved_variables.items_reverse()) {
      if(unsaved == sym) {
        sym = Expr();
        break;
      }
    }
    
    if(sym.is_valid())
      Impl(*this).emit_values(std::move(sym), ctx);
  }
  
  Expr values = g.end();
  values = EvaluationContexts::replace_symbol_namespace(std::move(values), ctx, strings::DollarContext_namespace);
  
  values = self.prepare_dynamic(std::move(values));
  values = Expr(Impl::internal_replace_symbols(values.release(), self._private_symbols, self._public_symbols));
  return values;
}

void DynamicLocalBox::Impl::emit_values(Expr symbol, String eval_ctx) {
  // todo: fetch variables from Server, maybe after each paint()
  
  Expr rules =  Application::interrupt_wait(
                  Call(Symbol(richmath_FE_SymbolDefinitions), 
                    EvaluationContexts::replace_symbol_namespace(
                      self.prepare_dynamic(std::move(symbol)), 
                      strings::DollarContext_namespace, 
                      eval_ctx)),
                  Application::dynamic_timeout);
                  
  if(rules[0] == richmath_System_HoldComplete && rules.expr_length() > 0) {
    rules.set(0, Symbol(richmath_System_List));
    Gather::emit(rules);
  }
}

//} ... class DynamicLocalBox::Impl
