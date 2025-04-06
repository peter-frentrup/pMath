#include <eval/simple-evaluator.h>

#include <eval/current-value.h>
#include <eval/application.h>

#include <boxes/dynamiclocalbox.h>

#include <util/autovaluereset.h>
#include <util/hashtable.h>


using namespace richmath;

namespace {
  struct Sym {
    pmath_symbol_t symbol;
    
    Sym(pmath_symbol_t symbol) : symbol{symbol} {}
    
    friend bool operator==(const Sym &left, const Sym &right) { return pmath_same(left.symbol, right.symbol); }
    
    unsigned int hash() const { return pmath_hash(symbol); }
  };
}

namespace richmath {
  class SimpleEvaluator::Impl {
    public:
      static void ensure_init();
      static void done();
    
      static bool try_eval(Expr *result, Expr call);
      
      static pmath_t expand_compressed_data_raw(pmath_t obj);
        
    private:
      static bool eval_expr(Expr *result, Expr call);
      static bool eval_symbol(Expr *result, pmath_symbol_t sym);
      static bool can_evaluate_symbol(pmath_symbol_t sym);
      
      static bool eval_noop(Expr *result, Expr call);
      static bool eval_CurrentValue(Expr *result, Expr call);
      static bool eval_Dynamic(Expr *result, Expr call);
      static bool eval_Identical(Expr *result, Expr call);
      static bool eval_If(Expr *result, Expr call);
      
    private:
      static int depth;
      static bool _initialized;
      static Hashset<Sym>                        Constants;
      static Hashtable<Sym, bool(*)(Expr*,Expr)> Functions;
  };
}

int                                 SimpleEvaluator::Impl::depth = 0;
bool                                SimpleEvaluator::Impl::_initialized = false;
Hashset<Sym>                        SimpleEvaluator::Impl::Constants;
Hashtable<Sym, bool(*)(Expr*,Expr)> SimpleEvaluator::Impl::Functions;


extern pmath_symbol_t richmath_Language_SourceLocation;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_CompressedData;
extern pmath_symbol_t richmath_System_CurrentValue;
extern pmath_symbol_t richmath_System_DocumentObject;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_Identical;
extern pmath_symbol_t richmath_System_If;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_True;


//{ class SimpleEvaluator ...

bool SimpleEvaluator::try_eval(FrontEndObject *scope, Expr *result, Expr call) {
  Impl::ensure_init();
  
  struct Args {
    Expr *result;
    Expr  call;
    bool  success;
  } args;
  args.result = result;
  args.call = PMATH_CPP_MOVE(call);
  args.success = false; 
  
  FrontEndReference old_observer_id = Dynamic::current_observer_id;
  Dynamic::current_observer_id      = scope ? scope->id() : FrontEndReference::None;
  
  Application::with_evaluation_box(scope, [](void *_args) {
      Args *args = (Args*)_args;
      args->success = Impl::try_eval(args->result, PMATH_CPP_MOVE(args->call));
    }, &args);
  
  Dynamic::current_observer_id = old_observer_id;
  
  //return Impl::try_eval(result, PMATH_CPP_MOVE(call));
  return args.success;
}

Expr SimpleEvaluator::expand_compressed_data(Expr expr) {
  return Expr{Impl::expand_compressed_data_raw(expr.release())};
}

void SimpleEvaluator::done() {
  Impl::done();
}

//} ... class SimpleEvaluator

//{ class SimpleEvaluator::Impl ...

bool SimpleEvaluator::Impl::try_eval(Expr *result, Expr call) {
  AutoValueReset<int> avr_depth(depth);
  
  ++depth;
  if(depth > 30)
    return false;
  
  if(call.is_expr()) 
    return eval_expr(result, PMATH_CPP_MOVE(call));
  
  if(call.is_symbol())
    return eval_symbol(result, call.get());
  
  *result = PMATH_CPP_MOVE(call);
  return true;
}

pmath_t SimpleEvaluator::Impl::expand_compressed_data_raw(pmath_t obj) {
  if(!pmath_is_expr(obj))
    return obj;
  
  if(pmath_is_packed_array(obj))
    return obj;
  
  pmath_t debug_metadata = pmath_get_debug_metadata(obj);
  size_t len = pmath_expr_length(obj);
  for(size_t i = 0; i <= len; ++i) {
    pmath_t item = pmath_expr_extract_item(obj, i);
    item = expand_compressed_data_raw(item);
    obj = pmath_expr_set_item(obj, i, item);
  }
  
  obj = pmath_try_set_debug_metadata(obj, debug_metadata);
  
  if(len == 1 && pmath_expr_item_equals(obj, 0, richmath_System_CompressedData)) {
    pmath_string_t str = pmath_expr_get_item(obj, 1);
    
    if(pmath_is_string(str)) {
      pmath_serialize_error_t err = PMATH_SERIALIZE_OK;
      
      pmath_t data = pmath_decompress_from_string_quiet(str, &err);
      if(err == PMATH_SERIALIZE_OK) {
        pmath_unref(obj);
        return data;
      }
      
      pmath_unref(data);
    }
    else
      pmath_unref(str);
  }
  
  return obj;
}

bool SimpleEvaluator::Impl::eval_expr(Expr *result, Expr call) {
  if(pmath_is_packed_array(call.get())) {
    *result = PMATH_CPP_MOVE(call);
    return true;
  }
  
  Expr head;
  if(!try_eval(&head, call[0]))
    return false;
  
  bool (**func)(Expr*,Expr) = Functions.search(head.get());
  if(!func)
    return false;
  
  call.set(0, head);
  size_t len = call.expr_length();
  
  if(len >= 1) {
    pmath_symbol_attributes_t attr = 0;
    if(head.is_symbol()) {
      attr = pmath_symbol_get_attributes(head.get());
    }
    
    if(!(attr & (PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE))) {
      Expr item;
      if(!try_eval(&item, call[1]))
        return false;
      
      call.set(1, PMATH_CPP_MOVE(item));
    }
    if(!(attr & (PMATH_SYMBOL_ATTRIBUTE_HOLDREST | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE))) {
      for(size_t i = 2; i <= len; ++i) {
        Expr item;
        if(!try_eval(&item, call[i]))
          return false;
        
        call.set(i, PMATH_CPP_MOVE(item));
      }
    }
  }
  
  return (*func)(result, PMATH_CPP_MOVE(call));
}

bool SimpleEvaluator::Impl::eval_symbol(Expr *result, pmath_symbol_t sym) {
  if(Functions.search(sym)) {
    *result = Symbol(sym);
    return true;
  }
  
  if(Constants.contains(sym)) {
    *result = Symbol(sym);
    return true;
  }
  
  if(StyleData::is_style_name(Symbol(sym))) {
    *result = Symbol(sym);
    return true;
  }
  
  if(can_evaluate_symbol(sym)) {
    Expr val(pmath_symbol_get_value(sym));
    if(val == PMATH_UNDEFINED)
      return false;
    
    //if(intptr_t id = pmath_dynamic_get_current_tracker_id())
    if(Dynamic::current_observer_id)
      pmath_symbol_track_dynamic(sym, (intptr_t)FrontEndReference::unsafe_cast_to_pointer(Dynamic::current_observer_id));
    return try_eval(result, PMATH_CPP_MOVE(val));
  }
   
  return false;
}

bool SimpleEvaluator::Impl::can_evaluate_symbol(pmath_symbol_t sym) {
  pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(sym);
  if(!(attr & PMATH_SYMBOL_ATTRIBUTE_TEMPORARY))
    return false;
  
  return true;
//  Box *box = dynamic_cast<Box*>(Application::get_evaluation_object());
//  for(; box; box = box->parent()) {
//    if(auto dyn_local = dynamic_cast<DynamicLocalBox*>(box)) {
//      if(dyn_local->is_private_symbol(sym)) {
//        return true;
//      }
//    }
//  }
//  return false;
}

void SimpleEvaluator::Impl::ensure_init() {
  if(_initialized)
    return;
  
  Constants.add(richmath_System_Automatic);
  Constants.add(richmath_System_False);
  Constants.add(richmath_System_None);
  Constants.add(richmath_System_True);
  
  Functions.set(richmath_Language_SourceLocation, eval_noop);
  Functions.set(richmath_System_CurrentValue,     eval_CurrentValue);
  Functions.set(richmath_System_DocumentObject,   eval_noop);
  Functions.set(richmath_System_Dynamic,          eval_Dynamic);
  Functions.set(richmath_System_FrontEndObject,   eval_noop);
  Functions.set(richmath_System_Identical,        eval_Identical);
  Functions.set(richmath_System_If,               eval_If);
  Functions.set(richmath_System_List,             eval_noop);
  Functions.set(richmath_System_Range,            eval_noop);
  
  _initialized = true;
}

void SimpleEvaluator::Impl::done() {
  _initialized = false;
  Constants.clear();
  Functions.clear();
}

bool SimpleEvaluator::Impl::eval_noop(Expr *result, Expr call) {
  *result = PMATH_CPP_MOVE(call);
  return true;
}

bool SimpleEvaluator::Impl::eval_CurrentValue(Expr *result, Expr call) {
  if(call.expr_length() == 1)
    return try_eval(result, CurrentValue::get(call[1]));
  
  if(call.expr_length() == 2) 
    return try_eval(result, CurrentValue::get(call[1], call[2]));
  
  return false;
}

bool SimpleEvaluator::Impl::eval_Dynamic(Expr *result, Expr call) {
  size_t len = call.expr_length();
  if(len < 1)
    return false;
  
  if(len == 1)
    return try_eval(result, call[1]);
  
  // TODO: consider TrackedSymbols option like in dynamic.c :: builtin_internal_dynamicevaluatemultiple()

  return false;
}

bool SimpleEvaluator::Impl::eval_Identical(Expr *result, Expr call) {
  if(call.expr_length() != 2)
    return false;
  
  *result = Symbol((call[1] == call[2]) ? richmath_System_True : richmath_System_False);
  return true;
}

bool SimpleEvaluator::Impl::eval_If(Expr *result, Expr call) {
  size_t len = call.expr_length();
  if(len < 2 || len > 4)
    return false;
  
  Expr cond;
  if(!try_eval(&cond, call[1]))
    return false;
  
  if(cond == richmath_System_True)
    return try_eval(result, call[2]);
  
  if(cond == richmath_System_False) {
    if(len < 3)
      return false;
    return try_eval(result, call[3]);
  }
  
  if(len < 4)
    return false;
  
  return try_eval(result, call[4]);
}

//} ... class SimpleEvaluator::Impl
