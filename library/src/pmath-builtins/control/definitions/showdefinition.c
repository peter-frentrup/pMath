#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <pmath-language/scanner.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

#define EVAL_CODE_ARGS(code, format, ...) \
  pmath_evaluate( \
    pmath_parse_string_args( \
      (code), (format), __VA_ARGS__))

static void print_rule_defs(
  pmath_symbol_t  sym,   // wont be freed
  pmath_t         rules, // will be freed
  pmath_bool_t    tagged
){
  size_t i;
  
  if(!_pmath_is_list_of_rules(rules)){
    pmath_unref(rules);
    return;
  }
  
  for(i = 1;i <= pmath_expr_length(rules);++i){
    pmath_expr_t rule_i = pmath_expr_get_item(rules,  i);
    pmath_t lhs        = pmath_expr_get_item(rule_i, 1);
    pmath_t rhs        = pmath_expr_get_item(rule_i, 2);
    pmath_unref(rule_i);
    
    if(pmath_is_expr_of_len(lhs, PMATH_SYMBOL_HOLDPATTERN, 1)){
      rule_i = pmath_expr_get_item(lhs, 1);
      pmath_unref(lhs);
      lhs = rule_i;
    }
    
    if(tagged){
      if(pmath_is_evaluated(rhs)){
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(InputForm(`1`/: `2`:= `3`)))", 
          "(ooo)", 
          pmath_ref(sym),
          lhs,
          rhs);
      }
      else{
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(InputForm(`1`/: `2`::= `3`)))", 
          "(ooo)", 
          pmath_ref(sym),
          lhs,
          rhs);
      }
    }
    else{
      if(pmath_is_evaluated(rhs)){
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(InputForm(`1`:= `2`)))", 
          "(oo)", 
          lhs,
          rhs);
      }
      else{
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(InputForm(`1`::= `2`)))", 
          "(oo)", 
          lhs,
          rhs);
      }
    }
  }
  
  pmath_unref(rules);
}

PMATH_PRIVATE pmath_t builtin_showdefinition(pmath_expr_t expr){
  pmath_symbol_t sym;
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pmath_is_string(sym)){
    obj = sym;
    
    expr = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_NAMES), 1,
        pmath_ref(obj)));
    
    sym = PMATH_NULL;
    if(pmath_is_expr_of(expr, PMATH_SYMBOL_LIST)){
      switch(pmath_expr_length(expr)){
        case 0: break;
        
        case 1:
          sym = pmath_symbol_find(pmath_expr_get_item(expr, 1), FALSE);
          break;
          
        default:
          pmath_unref(obj);
          return pmath_parse_string_args(
            "SectionPrint(\"PrintUsage\", `1`.Sort.Row(\"\\n\").ToString)", "(o)", expr);
            // "SectionPrint(\"PrintUsage\", `1`.Sort.Riffle(\"\\n\").Apply(Join))"
      }
    }
    
    pmath_unref(expr);
    if(pmath_is_null(sym)){
      pmath_message(PMATH_NULL, "notfound", 1, obj);
      return PMATH_NULL;
    }
    
    pmath_unref(obj);
        
//    obj = sym;
//    sym = pmath_symbol_find(pmath_ref(obj), FALSE);
//    
//    if(!sym){
//      pmath_message(PMATH_NULL, "notfound", 1, obj);
//      return PMATH_NULL;
//    }
//    
//    pmath_unref(obj);
  }
  else if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "ssym", 1, sym);
    return PMATH_NULL;
  }
  
  
  obj = EVAL_CODE_ARGS("`1`::usage", "(o)", pmath_ref(sym));
  if(pmath_is_string(obj)){
    PMATH_RUN_ARGS(
        "SectionPrint(\"PrintUsage\", HoldForm(LongForm(`1`)))", 
      "(o)", 
      obj);
  }
  else{
    pmath_unref(obj);
    PMATH_RUN_ARGS(
        "SectionPrint(\"PrintUsage\", HoldForm(LongForm(`1`)))", 
      "(o)", 
      pmath_ref(sym));
  }
  
  
  obj = EVAL_CODE_ARGS("Attributes(`1`)", "(o)", pmath_ref(sym));
  if(!pmath_is_expr_of_len(obj, PMATH_SYMBOL_LIST, 0)){
    PMATH_RUN_ARGS(
        "SectionPrint(\"Print\", HoldForm(Attributes(`1`):= `2`))", 
      "(oo)", 
      pmath_ref(sym),
      obj);
  }
  else
    pmath_unref(obj);
  
  obj = EVAL_CODE_ARGS("DefaultRules(`1`)", "(o)", pmath_ref(sym));
  print_rule_defs(sym, obj, FALSE);
    
  if((pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_READPROTECTED) == 0){
    obj = EVAL_CODE_ARGS("NRules(`1`)", "(o)", pmath_ref(sym));
    print_rule_defs(sym, obj, FALSE);
    
    obj = EVAL_CODE_ARGS("DownRules(`1`)", "(o)", pmath_ref(sym));
    print_rule_defs(sym, obj, FALSE);
    
    obj = EVAL_CODE_ARGS("SubRules(`1`)", "(o)", pmath_ref(sym));
    print_rule_defs(sym, obj, FALSE);
    
    obj = EVAL_CODE_ARGS("UpRules(`1`)", "(o)", pmath_ref(sym));
    print_rule_defs(sym, obj, TRUE);
    
    obj = EVAL_CODE_ARGS("FormatRules(`1`)", "(o)", pmath_ref(sym));
    print_rule_defs(sym, obj, FALSE);
    
    obj = pmath_symbol_get_value(sym);
    if(!pmath_same(obj, PMATH_UNDEFINED)){
      if(pmath_is_evaluated(obj)){
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(`1`:= `2`))", 
          "(oo)", 
          pmath_ref(sym),
          obj);
      }
      else{
        PMATH_RUN_ARGS(
            "SectionPrint(\"Print\", HoldForm(`1`::= `2`))", 
          "(oo)", 
          pmath_ref(sym),
          obj);
      }
    }
  }
  
  pmath_unref(sym);
  return PMATH_NULL;
}
