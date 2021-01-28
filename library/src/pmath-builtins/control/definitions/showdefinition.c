#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/dispatch-tables.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>


#define EVAL_CODE_ARGS(code, format, ...) \
  pmath_evaluate( \
                  pmath_parse_string_args( \
                      (code), (format), __VA_ARGS__))

extern pmath_symbol_t pmath_System_FormatRules;
extern pmath_symbol_t pmath_System_BoxForm_DollarUseTextFormatting;
extern pmath_symbol_t pmath_System_Private_PrepareUsageLine;
extern pmath_symbol_t pmath_System_Private_PrepareDefinitionLine;

static void print_definition_line(pmath_t expr) { // expr will be freed
  expr = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_HOLDFORM), 1, expr);
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_Private_PrepareDefinitionLine), 1,
           expr);
  expr = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_SECTIONPRINT), 2,
           PMATH_C_STRING("Print"),
           expr);
  pmath_unref(pmath_evaluate(expr));
}

static void print_rule_defs(
  pmath_symbol_t  sym,   // wont be freed
  pmath_t         rules, // will be freed
  pmath_bool_t    tagged
) {
  size_t i;
  
  if(!pmath_is_list_of_rules(rules)) {
    pmath_unref(rules);
    return;
  }
  
  for(i = 1; i <= pmath_expr_length(rules); ++i) {
    pmath_expr_t rule_i = pmath_expr_get_item(rules,  i);
    pmath_t lhs        = pmath_expr_get_item(rule_i, 1);
    pmath_t rhs        = pmath_expr_get_item(rule_i, 2);
    pmath_unref(rule_i);
    
    if(pmath_is_expr_of_len(lhs, PMATH_SYMBOL_HOLDPATTERN, 1)) {
      rule_i = pmath_expr_get_item(lhs, 1);
      pmath_unref(lhs);
      lhs = rule_i;
    }
    
    if(tagged) {
      if(pmath_is_evaluated(rhs)) {
        print_definition_line(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TAGASSIGN), 3, 
            pmath_ref(sym), 
            lhs, 
            rhs));
      }
      else {
        print_definition_line(
          pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_TAGASSIGNDELAYED), 3, 
          pmath_ref(sym), 
          lhs, 
          rhs));
      }
    }
    else {
      if(pmath_is_evaluated(rhs)) {
        print_definition_line(
          pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
          lhs, 
          rhs));
      }
      else {
        print_definition_line(
          pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED), 2,
          lhs, 
          rhs));
      }
    }
  }
  
  pmath_unref(rules);
}

PMATH_PRIVATE pmath_t builtin_showdefinition(pmath_expr_t expr) {
  pmath_symbol_t sym;
  pmath_t name;
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pmath_is_string(sym)) {
    obj = sym;
    
    expr = pmath_evaluate(
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_NAMES), 1,
               pmath_ref(obj)));
               
    sym = PMATH_NULL;
    if(pmath_is_expr_of(expr, PMATH_SYMBOL_LIST)) {
      switch(pmath_expr_length(expr)) {
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
    if(pmath_is_null(sym)) {
      pmath_message(PMATH_NULL, "notfound", 1, obj);
      return PMATH_NULL;
    }
    
    pmath_unref(obj);
  }
  else if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "ssym", 1, sym);
    return PMATH_NULL;
  }
  
  obj = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_SECTIONPRINT), 2,
          PMATH_C_STRING("PrintUsage"),
          pmath_expr_new_extended(
            pmath_ref(pmath_System_Private_PrepareUsageLine), 1, 
            pmath_ref(sym)));
  pmath_unref(pmath_evaluate(obj));
  
  /* We use Attributes("Global`a"), UpRules("Global`a"), ... instead of 
     Attributes(a), UpRules(a), ..., to prevent unfriendly definitions like
     
       a/: ~h(~~~, a, ~~~)::= (Print("a fired in ", h);)
     
     from disturbing us.
   */
  name = pmath_symbol_name(sym);
  
  obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ATTRIBUTES), 1, pmath_ref(name));
  obj = pmath_evaluate(obj);
  if(!pmath_is_expr_of_len(obj, PMATH_SYMBOL_LIST, 0)) {
    
    print_definition_line(
      pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_ATTRIBUTES), 1,
        pmath_ref(sym)), 
      obj));
  }
  else
    pmath_unref(obj);
    
  obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_DEFAULTRULES), 1, pmath_ref(name));
  obj = pmath_evaluate(obj);
  print_rule_defs(sym, obj, FALSE);
  
  if((pmath_symbol_get_attributes(sym) & PMATH_SYMBOL_ATTRIBUTE_READPROTECTED) == 0) {
  
    pmath_t old_use_text_formatting = pmath_thread_local_save(
      pmath_System_BoxForm_DollarUseTextFormatting, 
      pmath_ref(PMATH_SYMBOL_TRUE));
    
    obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_NRULES), 1, pmath_ref(name));
    obj = pmath_evaluate(obj);
    print_rule_defs(sym, obj, FALSE);
    
    obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_DOWNRULES), 1, pmath_ref(name));
    obj = pmath_evaluate(obj);
    print_rule_defs(sym, obj, FALSE);
    
    obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_SUBRULES), 1, pmath_ref(name));
    obj = pmath_evaluate(obj);
    print_rule_defs(sym, obj, FALSE);
    
    obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_UPRULES), 1, pmath_ref(name));
    obj = pmath_evaluate(obj);
    print_rule_defs(sym, obj, TRUE);
    
    obj = pmath_expr_new_extended(pmath_ref(pmath_System_FormatRules), 1, pmath_ref(name));
    obj = pmath_evaluate(obj);
    print_rule_defs(sym, obj, FALSE);
    
    obj = pmath_symbol_get_value(sym);
    if(!pmath_same(obj, PMATH_UNDEFINED)) {
      if(pmath_is_evaluatable(obj)) {
        if(pmath_is_evaluated(obj)) {
          print_definition_line(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_ASSIGN), 2, 
              pmath_ref(sym), 
              obj));
        }
        else {
          print_definition_line(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED), 2, 
                pmath_ref(sym), 
                obj));
        }
      }
      else {
        pmath_unref(obj);
        
        obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_OWNRULES), 1, pmath_ref(name));
        obj = pmath_evaluate(obj);
        print_rule_defs(sym, obj, FALSE);
      }
    }
    
    pmath_unref(pmath_thread_local_save(
      pmath_System_BoxForm_DollarUseTextFormatting, 
      old_use_text_formatting));
  }
  
  pmath_unref(name);
  pmath_unref(sym);
  return PMATH_NULL;
}
