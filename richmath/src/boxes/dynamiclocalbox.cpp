#include <boxes/dynamiclocalbox.h>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/binding.h>


using namespace richmath;

//{ class DynamicLocalBox ...

DynamicLocalBox::DynamicLocalBox()
: OwnerBox()
{
}

DynamicLocalBox::~DynamicLocalBox(){
  if(_deinitialization.is_valid()
  && _deinitialization != PMATH_SYMBOL_NONE
  && !_init_call.is_valid()) // only call Deinitialization if the initialization was called
    Application::interrupt(prepare_dynamic(_deinitialization), Application::dynamic_timeout);
}

DynamicLocalBox *DynamicLocalBox::create(Expr expr, int options){
  if(expr.expr_length() < 2)
    return 0;
  
  Expr options_expr = Expr(pmath_options_extract(expr.get(), 2));
  if(options_expr.is_null())
    return 0;
  
  DynamicLocalBox *box = new DynamicLocalBox();
  Expr init = expr[1];
  if(init[0] != PMATH_SYMBOL_LIST){
    delete box;
    return 0;
  }
  
  box->_public_symbols  = MakeList(init.expr_length());
  box->_private_symbols = box->_public_symbols;
  for(size_t i = init.expr_length();i > 0;--i){
    Expr def = init[i];
    
    if(def.is_symbol()){
      box->_public_symbols.set(i, def);
      continue;
    }
    
    if(def.expr_length() == 2){
      if(def[0] == PMATH_SYMBOL_ASSIGN || def[0] == PMATH_SYMBOL_ASSIGNDELAYED){
        Expr sym = def[1];
        if(sym.is_symbol()){
          box->_public_symbols.set(i, sym);
          continue;
        }
      }
    }
    
    delete box;
    return 0;
  }
  
  for(size_t i = box->_public_symbols.expr_length();i > 0;--i){
    Expr sym = box->_public_symbols[i];
    sym = Expr(pmath_symbol_create_temporary(pmath_symbol_name(sym.get()), TRUE));
    box->_private_symbols.set(i, sym);
  }
  
  Expr values = Expr(pmath_option_value(
    PMATH_SYMBOL_DYNAMICLOCALBOX, 
    PMATH_SYMBOL_DYNAMICLOCALVALUES,
    options_expr.get()));
  
  box->_initialization = Expr(pmath_option_value(
    PMATH_SYMBOL_DYNAMICLOCALBOX, 
    PMATH_SYMBOL_INITIALIZATION,
    options_expr.get()));
  
  box->_deinitialization = Expr(pmath_option_value(
    PMATH_SYMBOL_DYNAMICLOCALBOX, 
    PMATH_SYMBOL_DEINITIALIZATION,
    options_expr.get()));
  
  box->_unsaved_variables = Expr(pmath_option_value(
    PMATH_SYMBOL_DYNAMICLOCALBOX, 
    PMATH_SYMBOL_UNSAVEDVARIABLES,
    options_expr.get()));
  
  box->_init_call = List(init, values, box->_initialization);
  box->content()->load_from_object(expr[2], options);
  
  return box;
}

void DynamicLocalBox::paint(Context *context){
  ensure_init();
  
  OwnerBox::paint(context);
  
  // todo: fetch variables from Server, maybe after each paint()
}

  static pmath_t internal_replace_symbols(pmath_t expr, const Expr &old_syms, const Expr &new_syms){
    if(pmath_is_symbol(expr)){
      size_t i;
      for(i = old_syms.expr_length();i > 0;--i){
        if(old_syms[i] == expr){
          pmath_unref(expr);
          return new_syms[i].release();
        }
      }
      
      return expr;
    }
  
    if(pmath_is_expr(expr)){
      size_t i;
      for(i = 0;i <= pmath_expr_length(expr);++i){
        pmath_t item = pmath_expr_extract_item(expr, i);
        
        item = internal_replace_symbols(item, old_syms, new_syms);
        
        expr = pmath_expr_set_item(expr, i, item);
      }
    }
    
    return expr;
  }

Expr DynamicLocalBox::to_pmath(bool parseable){
  ensure_init();
  
  Gather g;
  
  Gather::emit(_public_symbols);
  Gather::emit(content()->to_pmath(parseable));
  Gather::emit(RuleDelayed(Symbol(PMATH_SYMBOL_INITIALIZATION),   _initialization));
  Gather::emit(RuleDelayed(Symbol(PMATH_SYMBOL_DEINITIALIZATION), _deinitialization));
  Gather::emit(RuleDelayed(Symbol(PMATH_SYMBOL_UNSAVEDVARIABLES), _unsaved_variables));
  
  {
    Gather g2;
    
    for(size_t i = 1;i <= _public_symbols.expr_length();++i){
      Expr sym = _public_symbols[i];
      
      for(size_t j = _unsaved_variables.expr_length();j > 0;--j){
        if(_unsaved_variables[j] == sym){
          sym = Expr();
          break;
        }
      }
      
      if(sym.is_valid())
        emit_values(sym);
    }
    
    Expr values = g2.end();
    values = OwnerBox::prepare_dynamic(values);
    values = Expr(internal_replace_symbols(values.release(), _private_symbols, _public_symbols));
    Gather::emit(RuleDelayed(Symbol(PMATH_SYMBOL_DYNAMICLOCALVALUES), values));
  }
  
  Expr expr = g.end();
  expr.set(0, Symbol(PMATH_SYMBOL_DYNAMICLOCALBOX));
  return expr;
}

Expr DynamicLocalBox::prepare_dynamic(Expr expr){
  expr = Expr(internal_replace_symbols(expr.release(), _public_symbols, _private_symbols));
  return OwnerBox::prepare_dynamic(expr);
}

void DynamicLocalBox::ensure_init(){
  if(_init_call.is_valid()){
    Application::interrupt(prepare_dynamic(_init_call), Application::dynamic_timeout);
    _init_call = Expr();
  }
}

void DynamicLocalBox::emit_values(Expr symbol){
  // todo: fetch variables from Server, maybe after each paint()
  
  Expr rules =  Application::interrupt(
    Call(GetSymbol(SymbolDefinitionsSymbol), prepare_dynamic(symbol)), 
    Application::dynamic_timeout);
  
  if(rules[0] == PMATH_SYMBOL_HOLDCOMPLETE && rules.expr_length() > 0){
    rules.set(0, Symbol(PMATH_SYMBOL_LIST));
    Gather::emit(rules);
  }
}

//} ... class DynamicLocalBox
