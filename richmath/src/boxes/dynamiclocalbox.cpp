#include <boxes/dynamiclocalbox.h>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/binding.h>

extern pmath_symbol_t richmath_FE_SymbolDefinitions;

using namespace richmath;

extern pmath_symbol_t richmath_System_Deinitialization;
extern pmath_symbol_t richmath_System_DynamicLocalBox;
extern pmath_symbol_t richmath_System_DynamicLocalValues;
extern pmath_symbol_t richmath_System_Initialization;
extern pmath_symbol_t richmath_System_UnsavedVariables;

//{ class DynamicLocalBox ...

DynamicLocalBox::DynamicLocalBox()
  : AbstractDynamicBox()
{
}

DynamicLocalBox::~DynamicLocalBox() {
  if( _deinitialization.is_valid() &&
      _deinitialization != PMATH_SYMBOL_NONE &&
      !_init_call.is_valid())
  {
    // only call Deinitialization if the initialization was called
    Application::interrupt_wait(prepare_dynamic(_deinitialization), Application::dynamic_timeout);
  }
}

bool DynamicLocalBox::try_load_from_object(Expr expr, BoxInputFlags options) {
  if(expr[0] != richmath_System_DynamicLocalBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options_expr = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options_expr.is_null())
    return false;
    
  Expr symbols = expr[1];
  if(symbols[0] != PMATH_SYMBOL_LIST)
    return false;
    
  for(auto sym : symbols.items()) {
    if(!sym.is_symbol())
      return false;
  }
  
  /* now success is guaranteed */
  
  if(_public_symbols != symbols) {
    // TODO: call previous deinitialization
    
    _public_symbols  = symbols;
    
    /* FIXME: only introduce new private symbols for *new* public symbols, because 
        DynamicBox() will not see the change: DynamicBox::try_load_from_object() will not reset
        its `must_update` if merely our private symbol changed.
        
        To work around the issue, we will force must_update=true for all contained DynamicBox's etc.
     */
    options|= BoxInputFlags::ForceResetDynamic;

    _private_symbols = symbols;
    for(size_t i = symbols.expr_length(); i > 0; --i) {
      Expr sym = symbols[i];
      
      sym = Expr(pmath_symbol_create_temporary(
                   pmath_symbol_name(sym.get()),
                   TRUE));
                   
      _private_symbols.set(i, sym);
    }
  }
  
  Expr values = Expr(pmath_option_value(
                       richmath_System_DynamicLocalBox,
                       richmath_System_DynamicLocalValues,
                       options_expr.get()));
                       
  _initialization = Expr(pmath_option_value(
                           richmath_System_DynamicLocalBox,
                           richmath_System_Initialization,
                           options_expr.get()));
                           
  _deinitialization = Expr(pmath_option_value(
                             richmath_System_DynamicLocalBox,
                             richmath_System_Deinitialization,
                             options_expr.get()));
                             
  _unsaved_variables = Expr(pmath_option_value(
                              richmath_System_DynamicLocalBox,
                              richmath_System_UnsavedVariables,
                              options_expr.get()));
                              
  _init_call = List(values, _initialization);
  content()->load_from_object(expr[2], options);
  
  finish_load_from_object(std::move(expr));
  return true;
}

void DynamicLocalBox::paint(Context &context) {
  ensure_init();
  
  AbstractDynamicBox::paint(context);
  
  // todo: fetch variables from Server, maybe after each paint()
}

static pmath_t internal_replace_symbols(pmath_t expr, const Expr &old_syms, const Expr &new_syms) {
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
  Gather::emit(RuleDelayed(Symbol(richmath_System_Initialization),   _initialization));
  Gather::emit(RuleDelayed(Symbol(richmath_System_Deinitialization), _deinitialization));
  Gather::emit(RuleDelayed(Symbol(richmath_System_UnsavedVariables), _unsaved_variables));
  
  {
    Gather g2;
    
    for(auto sym : _public_symbols.items()) {
      for(auto unsaved : _unsaved_variables.items_reverse()) {
        if(unsaved == sym) {
          sym = Expr();
          break;
        }
      }
      
      if(sym.is_valid())
        emit_values(sym);
    }
    
    Expr values = g2.end();
    values = AbstractDynamicBox::prepare_dynamic(values);
    values = Expr(internal_replace_symbols(values.release(), _private_symbols, _public_symbols));
    Gather::emit(RuleDelayed(Symbol(richmath_System_DynamicLocalValues), values));
  }
  
  Expr expr = g.end();
  expr.set(0, Symbol(richmath_System_DynamicLocalBox));
  return expr;
}

Expr DynamicLocalBox::prepare_dynamic(Expr expr) {
  expr = Expr(internal_replace_symbols(expr.release(), _public_symbols, _private_symbols));
  return AbstractDynamicBox::prepare_dynamic(expr);
}

void DynamicLocalBox::ensure_init() {
  if(_init_call.is_valid()) {
    Application::interrupt_wait_for(prepare_dynamic(_init_call), this, Application::dynamic_timeout);
    _init_call = Expr();
  }
}

void DynamicLocalBox::emit_values(Expr symbol) {
  // todo: fetch variables from Server, maybe after each paint()
  
  Expr rules =  Application::interrupt_wait(
                  Call(Symbol(richmath_FE_SymbolDefinitions), prepare_dynamic(symbol)),
                  Application::dynamic_timeout);
                  
  if(rules[0] == PMATH_SYMBOL_HOLDCOMPLETE && rules.expr_length() > 0) {
    rules.set(0, Symbol(PMATH_SYMBOL_LIST));
    Gather::emit(rules);
  }
}

//} ... class DynamicLocalBox
