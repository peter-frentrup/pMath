#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>


static pmath_t expand_ui_power(pmath_t sum, unsigned long n){
  pmath_t a;
  pmath_t b;
  pmath_t bin;
  size_t k;
  
  if(n == 0){
    pmath_unref(sum);
    return pmath_integer_new_si(1);
  }
  
  if(n == 1)
    return sum;
  
  a = pmath_expr_get_item(sum, 1);
  if(pmath_expr_length(sum) == 2){
    b = pmath_expr_get_item(sum, 2);
  }
  else{
    b = pmath_expr_get_item_range(sum, 2, SIZE_MAX);
  }
  
  pmath_unref(sum);
  pmath_gather_begin(NULL);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_POWER), 2,
      pmath_ref(a),
      pmath_integer_new_ui(n)), 
    NULL);
  
  bin = pmath_integer_new_ui(1);
  for(k = 1;k < n;++k){
    bin = _mul_nn(bin, 
      pmath_rational_new(
        pmath_integer_new_ui(n-k+1),
        pmath_integer_new_ui(k)));
    
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 3,
        pmath_ref(bin),
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          pmath_ref(a),
          pmath_integer_new_ui(n-k)),
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          pmath_ref(b),
          pmath_integer_new_ui(k))), 
      NULL);
  }
  pmath_unref(bin);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_POWER), 2,
      pmath_ref(b),
      pmath_integer_new_ui(n)), 
    NULL);
  
  pmath_unref(a);
  pmath_unref(b);
  return pmath_expr_set_item(pmath_gather_end(), 0, pmath_ref(PMATH_SYMBOL_PLUS));
}

static pmath_t expand_product(pmath_t expr, pmath_bool_t *changed){
  if(pmath_is_expr(expr)){
    pmath_t item = pmath_expr_get_item(expr, 0);
    pmath_unref(item);
    
    if(item == PMATH_SYMBOL_TIMES){
      size_t i;
      
      for(i = pmath_expr_length(expr);i > 0;--i){
        item = pmath_expr_get_item(expr, i);
        
        if(pmath_is_expr_of(item, PMATH_SYMBOL_PLUS)){
          size_t j;
          
          for(j = pmath_expr_length(item);j > 0;--j){
            expr = pmath_expr_set_item(expr, i,
              pmath_expr_get_item(item, j));
            
            item = pmath_expr_set_item(item, j, pmath_ref(expr));
          }
          
          *changed = TRUE;
          pmath_unref(expr);
          return item;
        }
        
        pmath_unref(item);
      }
      
      return expr;
    }
    
    if(item == PMATH_SYMBOL_POWER && pmath_expr_length(expr) == 2){
      pmath_t exp = pmath_expr_get_item(expr, 2);
      
      if(pmath_is_integer(exp)){
        if(pmath_integer_fits_ui(exp)){
          unsigned long uexp = pmath_integer_get_ui(exp);
          
          if(exp+1 != 0){
            pmath_t base = pmath_expr_get_item(expr, 1);
            
            if(pmath_is_expr_of(base, PMATH_SYMBOL_PLUS)){
              pmath_unref(expr);
              pmath_unref(exp);
              expr = expand_ui_power(base, uexp);
              *changed = TRUE;
              return expr;
            }
            
            pmath_unref(base);
          }
        }
      }
      else if(pmath_instance_of(exp, PMATH_TYPE_RATIONAL)){
        pmath_integer_t num = pmath_rational_numerator(exp);
        pmath_integer_t den = pmath_rational_denominator(exp);
        
        if(pmath_compare(num, den) > 0){
          struct _pmath_integer_t *q = _pmath_create_integer();
          struct _pmath_integer_t *r = _pmath_create_integer();
          
          if(q && r){
            mpz_fdiv_qr(
              q->value,
              r->value,
              ((struct _pmath_integer_t*)num)->value,
              ((struct _pmath_integer_t*)den)->value);
            
            pmath_unref(num);
            pmath_unref(exp);
            exp = pmath_rational_new((pmath_integer_t)r, den);
            
            num  = pmath_expr_set_item(pmath_ref(expr), 2, (pmath_t)q);
            expr = pmath_expr_set_item(expr, 2, exp);
            *changed = TRUE;
            return pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              num, 
              expr);
          }
          
          pmath_unref((pmath_t)q);
          pmath_unref((pmath_t)r);
        }
        
        pmath_unref(num);
        pmath_unref(den);
      }
      
      pmath_unref(exp);
      return expr;
    }
  }
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_expand(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(obj)){
    pmath_t head = pmath_expr_get_item(obj, 0);
    pmath_unref(head);
    
    if(head == PMATH_SYMBOL_PLUS || head == PMATH_SYMBOL_LIST){
      size_t i;
      pmath_unref(expr);
      expr = obj;
      
      for(i = pmath_expr_length(expr);i > 0;--i){
        obj = pmath_expr_get_item(expr, i);
        
        obj = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_EXPAND), 1,
          obj);
        
        expr = pmath_expr_set_item(expr, i, obj);
      }
      
      return expr;
    }
    
    if(head == PMATH_SYMBOL_TIMES){
      pmath_bool_t changed;
      size_t i;
      pmath_unref(expr);
      expr = obj;
      
      for(i = pmath_expr_length(expr);i > 0;--i){
        obj = pmath_expr_get_item(expr, i);
        
        obj = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_EXPAND), 1,
          obj);
        
        expr = pmath_expr_set_item(expr, i, obj);
      }
      
      expr = pmath_evaluate(expr);
      
      changed = FALSE;
      expr = expand_product(expr, &changed);
      if(changed){
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_EXPAND), 1,
          expr);
      }
      
      return expr;
    }
    
    if(head == PMATH_SYMBOL_POWER){
      pmath_bool_t changed;
      pmath_unref(expr);
      expr = obj;
      obj = pmath_expr_get_item(expr, 2);
      
      if(pmath_is_integer(obj)
      && pmath_integer_fits_ui(obj)){
        pmath_unref(obj);
        obj = pmath_expr_get_item(expr, 1);
        
        obj = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_EXPAND), 1,
          obj);
        
        obj = pmath_evaluate(obj);
        expr = pmath_expr_set_item(expr, 1, obj);
      }
      else
        pmath_unref(obj);
      
      changed = FALSE;
      expr = expand_product(expr, &changed);
      if(changed){
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_EXPAND), 1,
          expr);
      }
      return expr;
    }
  }
  pmath_unref(expr);
  
  return obj;
}

static pmath_t make_expandall(pmath_t obj){
  if(pmath_is_expr(obj)){
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_EXPANDALL), 1,
      obj);
  }
  
  return obj;
}

static pmath_t eval_expandall(pmath_t obj){
  if(pmath_is_expr(obj)){
    return pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_EXPANDALL), 1,
        obj));
  }
  
  return obj;
}

PMATH_PRIVATE pmath_t builtin_expandall(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr(obj)){
    pmath_t head;
    pmath_bool_t expand_first = TRUE;
    pmath_bool_t expand_rest = TRUE;
    
    pmath_unref(expr);
    expr = obj; obj = NULL;
    
    head = eval_expandall(pmath_expr_get_item(expr, 0));
    expr = pmath_expr_set_item(expr, 0, pmath_ref(head));
    
    if(pmath_is_symbol(head)){
      pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(head);
      
      expand_first = !(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST);
      expand_rest  = !(attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST);
    }
    
    pmath_unref(head);
    
    if(expand_first){
      obj = make_expandall(pmath_expr_get_item(expr, 1));
      expr = pmath_expr_set_item(expr, 1, obj);
    }
    
    if(expand_rest){
      size_t i;
      for(i = pmath_expr_length(expr);i > 1;--i){
        obj = make_expandall(pmath_expr_get_item(expr, i));
        expr = pmath_expr_set_item(expr, i, obj);
      }
    }
    
    if(head == PMATH_SYMBOL_TIMES){
      pmath_bool_t changed;
      
      expr = pmath_evaluate(expr);
      
      changed = FALSE;
      expr = expand_product(expr, &changed);
      if(changed)
        return make_expandall(expr);
      
      return expr;
    }
    
    if(head == PMATH_SYMBOL_POWER){
      pmath_bool_t changed;
      obj = pmath_expr_get_item(expr, 2);
      
      if(pmath_instance_of(obj, PMATH_TYPE_RATIONAL)){
        if(pmath_number_sign(obj) < 0){
          expr = pmath_expr_set_item(expr, 2, pmath_number_neg(obj));
          expr = pmath_evaluate(expr);
          
          changed = FALSE;
          expr = expand_product(expr, &changed);
          expr = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_POWER), 2,
            expr,
            pmath_integer_new_si(-1));
            
          if(changed)
            return make_expandall(expr);
          
          return expr;
        }
      }
      
      pmath_unref(obj);
      expr = pmath_evaluate(expr);
      
      changed = FALSE;
      expr = expand_product(expr, &changed);
      if(changed)
        return make_expandall(expr);
      
      return expr;
    }
    
    return expr;
  }
  
  pmath_unref(expr);
  return obj;
}
