#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/number-theory-private.h>


static pmath_bool_t ptr_equal(void *a, void *b){
  return a == b;
}

static const pmath_ht_class_t symbol_set_class = {
  (pmath_callback_t)                    pmath_unref,
  (pmath_ht_entry_hash_func_t)        _pmath_hash_pointer,
  (pmath_ht_entry_equal_func_t)       ptr_equal,
  (pmath_ht_key_hash_func_t)          _pmath_hash_pointer,
  (pmath_ht_entry_equals_key_func_t)  ptr_equal
};

static void * volatile numeric_symbols;

PMATH_PRIVATE pmath_bool_t _pmath_is_inexact(pmath_t obj){
  if(pmath_instance_of(obj, PMATH_TYPE_FLOAT))
    return TRUE;
  
  if(_pmath_is_nonreal_complex(obj)){
    pmath_t part = pmath_expr_get_item(obj, 1);
    if(pmath_instance_of(part, PMATH_TYPE_FLOAT)){
      pmath_unref(part);
      return TRUE;
    }
    pmath_unref(part);
    
    part = pmath_expr_get_item(obj, 2);
    if(pmath_instance_of(part, PMATH_TYPE_FLOAT)){
      pmath_unref(part);
      return TRUE;
    }
    pmath_unref(part);
  }
  
  return FALSE;
}

  static int _simple_real_class(pmath_t obj){
    if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)){
      int sign = mpz_sgn(((struct _pmath_integer_t*)obj)->value);
      
      if(sign == 0)
        return PMATH_CLASS_ZERO;
      
      if(mpz_cmpabs_ui(((struct _pmath_integer_t*)obj)->value, 1) > 0)
        return sign * PMATH_CLASS_POSBIG;
      
      return sign * PMATH_CLASS_POSONE;
    }
    
    if(pmath_instance_of(obj, PMATH_TYPE_QUOTIENT)){
      int sign = mpz_sgn(((struct _pmath_quotient_t*)obj)->numerator->value);
      int smallbig = mpz_cmpabs(
        ((struct _pmath_quotient_t*)obj)->numerator->value,
        ((struct _pmath_quotient_t*)obj)->denominator->value);
      
      if(smallbig < 0)
        return sign * PMATH_CLASS_POSSMALL;
        
      return sign * PMATH_CLASS_POSBIG;
    }
    
    if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
      if(((struct _pmath_machine_float_t*)obj)->value < -1)
        return PMATH_CLASS_NEGBIG;
        
      if(((struct _pmath_machine_float_t*)obj)->value == -1)
        return PMATH_CLASS_NEGONE;
        
      if(((struct _pmath_machine_float_t*)obj)->value < 0)
        return PMATH_CLASS_NEGSMALL;
        
      if(((struct _pmath_machine_float_t*)obj)->value == 0)
        return PMATH_CLASS_ZERO;
        
      if(((struct _pmath_machine_float_t*)obj)->value < 1)
        return PMATH_CLASS_POSSMALL;
        
      if(((struct _pmath_machine_float_t*)obj)->value == 1)
        return PMATH_CLASS_POSONE;
      
      return PMATH_CLASS_POSBIG;
    }
    
    if(pmath_instance_of(obj, PMATH_TYPE_MP_FLOAT)){
      int sign = mpfr_sgn(((struct _pmath_mp_float_t*)obj)->value);
      
      if(sign < 0){
        int one = mpfr_cmp_si(((struct _pmath_mp_float_t*)obj)->value, -1);
        
        if(one < 0)
          return PMATH_CLASS_NEGBIG;
        
        if(one == 0)
          return PMATH_CLASS_NEGONE;
          
        return PMATH_CLASS_NEGSMALL;
      }
      
      if(sign > 0){
        int one = mpfr_cmp_si(((struct _pmath_mp_float_t*)obj)->value, 1);
        
        if(one > 0)
          return PMATH_CLASS_POSBIG;
        
        if(one == 0)
          return PMATH_CLASS_POSONE;
          
        return PMATH_CLASS_POSSMALL;
      }
      
      return PMATH_CLASS_ZERO;
    }
    
    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_COMPLEX, 2)){
      pmath_t re = pmath_expr_get_item(obj, 1);
      pmath_t im = pmath_expr_get_item(obj, 2);
      
      if(pmath_instance_of(re, PMATH_TYPE_NUMBER) 
      && pmath_instance_of(re, PMATH_TYPE_NUMBER)){
        if(pmath_number_sign(re) == 0){
          pmath_unref(re);
          pmath_unref(im);
          
          return PMATH_CLASS_IMAGINARY;
        }
        
        pmath_unref(re);
        pmath_unref(im);
        return PMATH_CLASS_OTCOMPLEX;
      }
      
      pmath_unref(re);
      pmath_unref(im);
      return PMATH_CLASS_UNKNOWN;
    }
    
    if(_pmath_is_infinite(obj)){
      pmath_t dir = pmath_expr_get_item(obj, 1);
      if(pmath_equals(dir, PMATH_NUMBER_ONE)){
        pmath_unref(dir);
        return PMATH_CLASS_POSINF;
      }
      
      if(pmath_equals(dir, PMATH_NUMBER_MINUSONE)){
        pmath_unref(dir);
        return PMATH_CLASS_NEGINF;
      }
      
      if(!dir || pmath_equals(dir, PMATH_NUMBER_ZERO)){
        pmath_unref(dir);
        return PMATH_CLASS_UINF;
      }
      
      pmath_unref(dir);
      return PMATH_CLASS_CINF;
    }
    
    return PMATH_CLASS_UNKNOWN;
  }

PMATH_PRIVATE int _pmath_number_class(pmath_t obj){
  int result = _simple_real_class(obj);
  
  if((result & PMATH_CLASS_UNKNOWN) && _pmath_is_numeric(obj)){
    obj = pmath_approximate(pmath_ref(obj), -HUGE_VAL, -HUGE_VAL);
    result = _simple_real_class(obj);
    pmath_unref(obj);
  }
  
  return result;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_numeric(pmath_t obj){
  if(pmath_instance_of(obj, PMATH_TYPE_NUMBER))
    return TRUE;
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    pmath_t h = pmath_expr_get_item(obj, 0);
    if(pmath_instance_of(h, PMATH_TYPE_SYMBOL)
    && (pmath_symbol_get_attributes(h) & PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION) != 0){
      pmath_bool_t result;
      size_t i;
      
      pmath_unref(h);
      for(i = pmath_expr_length(obj);i > 0;--i){
        h = pmath_expr_get_item(obj, i);
        result = _pmath_is_numeric(h);
        pmath_unref(h);
        
        if(!result)
          return FALSE;
      }
      
      return TRUE;
    }
    
    pmath_unref(h);
    return FALSE;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_SYMBOL)){
    pmath_bool_t   result;
    pmath_hashtable_t table;
    
    table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&numeric_symbols);
    
    result = pmath_ht_search(table, obj) != NULL;
    
    _pmath_atomic_unlock_ptr(&numeric_symbols, table);
    
    return result;
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_assign_isnumeric(pmath_expr_t expr){
  pmath_t         tag;
  pmath_t         lhs;
  pmath_t         rhs;
  pmath_t         sym;
  int                    assignment;
  pmath_hashtable_t      table;
  pmath_symbol_t         entry;
  
  assignment = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  if(!assignment)
    return expr;
  
  if(!pmath_is_expr_of_len(lhs, PMATH_SYMBOL_ISNUMERIC, 1)){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if(tag != PMATH_UNDEFINED && tag != sym){
    pmath_message(NULL, "tag", 2, tag, lhs);
    
    pmath_unref(sym);
    pmath_unref(expr);
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_message(NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(rhs == PMATH_UNDEFINED)
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  if(rhs != PMATH_SYMBOL_TRUE 
  && rhs != PMATH_SYMBOL_FALSE
  && rhs != PMATH_UNDEFINED){
    pmath_unref(sym);
    pmath_message(
      PMATH_SYMBOL_ISNUMERIC, "set", 2,
      lhs, 
      pmath_ref(rhs));
    
    if(assignment > 0)
      return rhs;
    
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(lhs);
  
  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&numeric_symbols);
  
  entry = pmath_ht_search(table, sym);
  if(!entry && rhs == PMATH_SYMBOL_TRUE){
    entry = pmath_ht_insert(table, sym);
    sym = NULL;
  }
  else if(entry && rhs != PMATH_SYMBOL_TRUE){
    entry = pmath_ht_remove(table, sym);
  }
  
  _pmath_atomic_unlock_ptr(&numeric_symbols, table);
  
  pmath_unref(sym);
  pmath_unref(entry);
  
  if(assignment > 0)
    return rhs;
  
  pmath_unref(rhs);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_isnumeric(pmath_expr_t expr){
  pmath_bool_t result;
  pmath_t  obj;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  result = _pmath_is_numeric(obj);
  pmath_unref(obj);
  
  return pmath_ref(result ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE);
}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_numeric_init(void){
  numeric_symbols = pmath_ht_create(&symbol_set_class, 10);
  
  if(!numeric_symbols)
    return FALSE;
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_numeric_done(void){
  pmath_ht_destroy((pmath_hashtable_t)numeric_symbols);
}
