#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>


struct symbol_definition_t {
  struct symbol_definition_t    *next;
  
  pmath_symbol_t                 symbol;
  struct _pmath_symbol_rules_t  *rules;
  pmath_t                        value;
  pmath_symbol_attributes_t      attributes;
};

static void destroy_definitions(struct symbol_definition_t *def) {
  while(def) {
    struct symbol_definition_t *old = def;
    def = old->next;
    
    pmath_unref(old->symbol);
    if(old->rules)
      _pmath_symbol_rules_done(old->rules);
    pmath_unref(old->value);
    pmath_mem_free(old);
  }
}

static struct symbol_definition_t *get_definition_and_clear(pmath_symbol_t sym) {
  struct symbol_definition_t *def;
  struct _pmath_symbol_rules_t  *old_rules;
  
  def = pmath_mem_alloc(sizeof(struct symbol_definition_t));
  if(!def)
    return NULL;
    
  def->next       = NULL;
  def->symbol     = pmath_ref(sym);
  def->attributes = pmath_symbol_get_attributes(sym);
  def->value      = pmath_symbol_get_value(sym);
  def->rules      = NULL;
  
  old_rules = _pmath_symbol_get_rules(sym, RULES_READ);
  if(old_rules) {
    def->rules = pmath_mem_alloc(sizeof(struct _pmath_symbol_rules_t));
    if(!def->rules) {
      destroy_definitions(def);
      return NULL;
    }
    
    _pmath_symbol_rules_copy(def->rules, old_rules);
  }
  
  pmath_symbol_set_attributes(sym, def->attributes & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  pmath_symbol_set_value(sym, PMATH_UNDEFINED);
  _pmath_clear(sym, TRUE); // also clears most other attributes
  
  return def;
}

static void restore_definitions(struct symbol_definition_t *def) {
  struct _pmath_symbol_rules_t  *sym_rules;
  
  while(def) {
    struct symbol_definition_t *old = def;
    def = old->next;
    
    pmath_symbol_set_attributes(old->symbol, old->attributes & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
    
    pmath_symbol_set_value(old->symbol, pmath_ref(old->value));
    
    if(old->rules) {
      sym_rules = _pmath_symbol_get_rules(old->symbol, RULES_WRITE);
      
      if(sym_rules) {
        _pmath_symbol_rules_clear(sym_rules);
        
        _pmath_symbol_rules_copy(sym_rules, old->rules);
      }
    }
    else {
      sym_rules = _pmath_symbol_get_rules(old->symbol, RULES_READ);
      
      if(sym_rules) {
        sym_rules = _pmath_symbol_get_rules(old->symbol, RULES_WRITE);
        if(sym_rules) {
          _pmath_symbol_rules_clear(sym_rules);
        }
      }
    }
    
    pmath_symbol_set_attributes(old->symbol, old->attributes);
    
    /* Symbols like $MaxExtraPrecision overwrite Assign/AssignDelayed... to 
       implement additional external storage. 
       We call the associated C-code here.
     */
    if( _pmath_have_code(old->symbol, PMATH_CODE_USAGE_UPCALL) &&
        !(old->attributes & PMATH_SYMBOL_ATTRIBUTE_PROTECTED))
    {
      pmath_t expr;
      
      if(pmath_same(old->value, PMATH_UNDEFINED)) {
        expr = pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_UNASSIGN), 1, 
                 pmath_ref(old->symbol));
      }
      else if(pmath_is_evaluatable(old->value)) {
        expr = pmath_expr_new_extended(
                         pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED), 2,
                         pmath_ref(old->symbol),
                         pmath_ref(old->value));
      }
      
      _pmath_run_code(old->symbol, PMATH_CODE_USAGE_UPCALL, &expr);
      pmath_unref(expr);
    }
  }
}

PMATH_PRIVATE pmath_t builtin_block(pmath_expr_t expr) {
  /* Block({x1:= v1, x2:= v2, x3, ...}, body)
   */
  pmath_t vars, body, ex;
  struct symbol_definition_t *olddefs = NULL;
  size_t i;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  vars = pmath_expr_get_item(expr, 1);
  for(i = 1; i <= pmath_expr_length(vars); ++i) {
    pmath_t def = pmath_expr_get_item(vars, i);
    
    if(pmath_is_symbol(def)) {
      pmath_unref(def);
      continue;
    }
    
    if( pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGNDELAYED, 2) ||
        pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGN, 2))
    {
      pmath_t lhs = pmath_expr_get_item(def, 1);
      
      if(pmath_is_symbol(lhs)) {
        pmath_unref(def);
        pmath_unref(lhs);
        continue;
      }
      
    }
    
    pmath_message(PMATH_NULL, "lvsym", 2, vars, def);
    return expr;
  }
  
  body = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  for(i = 1; i <= pmath_expr_length(vars); ++i) {
    pmath_t def = pmath_expr_extract_item(vars, i);
    
    if(pmath_is_expr_of_len(def, PMATH_SYMBOL_ASSIGN, 2)) {
      pmath_t val = pmath_expr_extract_item(def, 2);
      
      val = pmath_evaluate(val);
      
      def = pmath_expr_set_item(def, 2, val);
    }
    
    vars = pmath_expr_set_item(vars, i, def);
  }
  
  for(i = pmath_expr_length(vars); i > 0; --i) {
    struct symbol_definition_t *def;
    pmath_t sym = pmath_expr_get_item(vars, i);
    
    if(!pmath_is_symbol(sym)) {
      pmath_t tmp = sym;
      
      assert(pmath_is_expr(tmp));
      
      sym = pmath_expr_get_item(tmp, 1);
      pmath_unref(tmp);
      
      assert(pmath_is_symbol(sym));
    }
    
    def = get_definition_and_clear(sym);
    pmath_unref(sym);
    
    if(!def) {
      restore_definitions(olddefs);
      destroy_definitions(olddefs);
      
      pmath_unref(vars);
      pmath_unref(body);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
    
    def->next = olddefs;
    olddefs = def;
  }
  
  pmath_unref(pmath_evaluate(vars));
  body = pmath_evaluate(body);
  
  /* Should not be necessary, because restore_definitions performs no
     evaluations/does not call pmath_aborting() -> not interruptable
     TODO: Check this claim!
   */
  ex = pmath_catch();
  restore_definitions(olddefs);
  destroy_definitions(olddefs);
  if(!pmath_same(ex, PMATH_UNDEFINED))
    pmath_throw(ex);
    
  return body;
}
