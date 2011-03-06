#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>


#define DIRECTION_LESS      (1<<0)
#define DIRECTION_EQUAL     (1<<1)
#define DIRECTION_GREATER   (1<<2)

#define TOLERANCE_FACTOR 64

static pmath_bool_t almost_equal_machine(double a, double b){
  if(a == b)
    return TRUE;
  
  if((a < 0) != (b < 0))
    return FALSE;
  
  if(a < 0) a = -a;
  if(b < 0) b = -b;
  
  if(a < b){
    double tmp = a;
    a = b;
    b = tmp;
  }
  
  return a <= b * (1 + TOLERANCE_FACTOR * DBL_EPSILON);
}

static pmath_bool_t almost_equal_mp(
  pmath_float_t a, 
  pmath_float_t b
){ 
  pmath_number_t test;
  pmath_number_t err;
  
  assert(pmath_instance_of(a, PMATH_TYPE_MP_FLOAT));
  assert(pmath_instance_of(b, PMATH_TYPE_MP_FLOAT));
  
  if(mpfr_equal_p(PMATH_AS_MP_VALUE(a), PMATH_AS_MP_VALUE(b)))
    return TRUE;
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(a)) || mpfr_zero_p(PMATH_AS_MP_VALUE(b)))
    return FALSE;
  
  if(mpfr_cmpabs(PMATH_AS_MP_VALUE(a), PMATH_AS_MP_VALUE(b)) < 0){
    test = _mul_nn(
      pmath_ref(b), 
      _pow_fi(
        pmath_ref(a), 
        -1, 
        TRUE));
  }
  else{
    test = _mul_nn(
      pmath_ref(a), 
      _pow_fi(
        pmath_ref(b), 
        -1, 
        TRUE));
  }
  
  if(pmath_instance_of(test, PMATH_TYPE_MP_FLOAT)
  && pmath_number_sign(test) < 0){
    pmath_unref(test);
    return FALSE;
  }
  
  err = pmath_ref(test);
  test = _mul_nn(
    pmath_rational_new(
      pmath_integer_new_si(1), 
      pmath_integer_new_si(TOLERANCE_FACTOR)),
    _add_nn(test, pmath_integer_new_si(-1)));
  
  if(pmath_instance_of(err,  PMATH_TYPE_MP_FLOAT)
  && pmath_instance_of(test, PMATH_TYPE_MP_FLOAT)){
    // Max(a,b)/Min(a,b) <= 1 + TOLERANCE_FACTOR * epsilon
    if(mpfr_lessequal_p(PMATH_AS_MP_VALUE(test), PMATH_AS_MP_ERROR(err))){
      pmath_unref(err);
      pmath_unref(test);
      return TRUE;
    }
  }
  
  pmath_unref(err);
  pmath_unref(test);
  return FALSE;
}

static pmath_bool_t almost_equal(pmath_t a, pmath_t b){
  if(PMATH_IS_MAGIC(a) || PMATH_IS_MAGIC(b)){
    return pmath_same(a, b);
  }
  
  if(pmath_is_double(a)){
    if(pmath_is_double(b))
      return almost_equal_machine(
        PMATH_AS_DOUBLE(a),
        PMATH_AS_DOUBLE(b));
  }
  else if(pmath_instance_of(a, PMATH_TYPE_MP_FLOAT)
  &&      pmath_instance_of(b, PMATH_TYPE_MP_FLOAT)){
    return almost_equal_mp(a, b);
  }
  
  return pmath_equals(a, b);
}

static int pmath_fuzzy_compare(pmath_t a, pmath_t b){
  if(almost_equal(a, b))
    return 0;
  
  return pmath_compare(a, b);
}

static pmath_t ordered(
  pmath_expr_t expr,      // will be freed
  int          directions // DIRECTION_XXX bitset
){
  size_t len = pmath_expr_length(expr);
  
  if(len > 1){
    pmath_t prev;
    pmath_bool_t prev_was_true = TRUE;
    pmath_bool_t have_marker_after_start = FALSE;
    size_t start, i;
    
    start = 1;
    prev = pmath_expr_get_item(expr, 1);
    if(pmath_same(prev, PMATH_SYMBOL_UNDEFINED)){
      pmath_unref(expr);
      return prev;
    }
    
    for(i = 2;i <= len;i++){
      pmath_t next = pmath_expr_get_item(expr, i);
      
      do{ /* ... while(0) */
        pmath_bool_t old_prev_was_true = prev_was_true;
        prev_was_true = TRUE;
        
        if(pmath_is_double(prev) && _pmath_is_numeric(next)){
          pmath_t n = pmath_approximate(pmath_ref(next), -HUGE_VAL, -HUGE_VAL);
          int c;
          
          if(!pmath_is_number(n)){
            pmath_unref(n);
            continue;
          }
          
          c = pmath_fuzzy_compare(prev, n);
          
          if((c <  0 && (directions & DIRECTION_LESS) == 0)
          || (c == 0 && (directions & DIRECTION_EQUAL) == 0)
          || (c >  0 && (directions & DIRECTION_GREATER) == 0)){
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(n);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }
          
          if(start == i-1){
            start++;
          }
          else if(old_prev_was_true){
            have_marker_after_start = TRUE;
            expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          }
          
          pmath_unref(n);
          continue;
        }
        
        if(pmath_is_double(next) && _pmath_is_numeric(prev)){
          pmath_t p = pmath_approximate(pmath_ref(prev), -HUGE_VAL, -HUGE_VAL);
          int c;
          
          if(!pmath_is_number(p)){
            pmath_unref(p);
            continue;
          }
          
          c = pmath_fuzzy_compare(p, next);
          
          if((c <  0 && (directions & DIRECTION_LESS) == 0)
          || (c == 0 && (directions & DIRECTION_EQUAL) == 0)
          || (c >  0 && (directions & DIRECTION_GREATER) == 0)){
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(p);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }
          
          if(start == i-1){
            start++;
          }
          else if(old_prev_was_true){
            have_marker_after_start = TRUE;
            expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          }
          
          pmath_unref(p);
          continue;
        }
      
        if(pmath_is_number(prev) && pmath_is_number(next)){
          int c = pmath_fuzzy_compare(prev, next);
          
          if((c <  0 && (directions & DIRECTION_LESS) == 0)
          || (c == 0 && (directions & DIRECTION_EQUAL) == 0)
          || (c >  0 && (directions & DIRECTION_GREATER) == 0)){
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }
          
          if(start == i-1){
            start++;
          }
          else if(old_prev_was_true){
            have_marker_after_start = TRUE;
            expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          }
          
          continue;
        }
        
        if(pmath_is_string(prev) && pmath_is_string(next)){
          pmath_bool_t equal = pmath_equals(prev, next);
          
          if(( equal && (directions & DIRECTION_EQUAL) == 0)
          || (!equal && (directions & (DIRECTION_LESS |
                                       DIRECTION_EQUAL |
                                       DIRECTION_GREATER)) == DIRECTION_EQUAL))
          {
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }
          
          continue;
        }
        
        if(pmath_equals(prev, next)){ // symbols, expressions
          if((directions & DIRECTION_EQUAL) == 0){
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }

          if(start == i-1)
            start++;
          else if(old_prev_was_true){
            have_marker_after_start = TRUE;
            expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          }
          continue;
        }
        
        { // +/- Infinity ...
          pmath_t prev_infdir = _pmath_directed_infinity_direction(prev);
          pmath_t next_infdir = _pmath_directed_infinity_direction(next);
          pmath_t p;
          pmath_t n;

          if(pmath_equals(prev_infdir, PMATH_NUMBER_MINUSONE)){
            n = pmath_ref(next);
            if(pmath_is_null(next_infdir) && _pmath_is_numeric(n))
              n = pmath_approximate(n, -HUGE_VAL, -HUGE_VAL);
            
            if(pmath_is_number(n)
            || pmath_equals(next_infdir, PMATH_NUMBER_ONE)){
              if((directions & DIRECTION_LESS) == 0){
                pmath_unref(prev_infdir);
                pmath_unref(next_infdir);
                pmath_unref(prev);
                pmath_unref(next);
                pmath_unref(n);
                pmath_unref(expr);
                return pmath_ref(PMATH_SYMBOL_FALSE);
              }
              else if(start == i-1)
                start++;
              else if(old_prev_was_true){
                have_marker_after_start = TRUE;
                expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
              }
            }
            
            pmath_unref(n);
          }
          else if(pmath_equals(prev_infdir, PMATH_NUMBER_ONE)){
            n = pmath_ref(next);
            if(pmath_is_null(next_infdir) && _pmath_is_numeric(n))
              n = pmath_approximate(n, -HUGE_VAL, -HUGE_VAL);
              
            if(pmath_is_number(n)
            || pmath_equals(next_infdir, PMATH_NUMBER_MINUSONE)){
              if((directions & DIRECTION_GREATER) == 0){
                pmath_unref(prev_infdir);
                pmath_unref(next_infdir);
                pmath_unref(prev);
                pmath_unref(next);
                pmath_unref(n);
                pmath_unref(expr);
                return pmath_ref(PMATH_SYMBOL_FALSE);
              }
              else if(start == i-1)
                start++;
              else if(old_prev_was_true){
                have_marker_after_start = TRUE;
                expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
              }
            }
            
            pmath_unref(n);
          }
          else if(pmath_equals(next_infdir, PMATH_NUMBER_MINUSONE)){
            p = pmath_ref(prev);
            if(pmath_is_null(prev_infdir) && _pmath_is_numeric(p))
              p = pmath_approximate(p, -HUGE_VAL, -HUGE_VAL);
              
            if(pmath_is_number(p)
            || pmath_equals(prev_infdir, PMATH_NUMBER_ONE)){
              if((directions & DIRECTION_GREATER) == 0){
                pmath_unref(prev_infdir);
                pmath_unref(next_infdir);
                pmath_unref(prev);
                pmath_unref(p);
                pmath_unref(next);
                pmath_unref(expr);
                return pmath_ref(PMATH_SYMBOL_FALSE);
              }
              else if(start == i-1)
                start++;
              else if(old_prev_was_true){
                have_marker_after_start = TRUE;
                expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
              }
            }
            
            pmath_unref(p);
          }
          else if(pmath_equals(next_infdir, PMATH_NUMBER_ONE)){
            p = pmath_ref(prev);
            if(pmath_is_null(prev_infdir) && _pmath_is_numeric(p))
              p = pmath_approximate(p, -HUGE_VAL, -HUGE_VAL);
              
            if(pmath_is_number(prev)
            || pmath_equals(prev_infdir, PMATH_NUMBER_MINUSONE)){
              if((directions & DIRECTION_LESS) == 0){
                pmath_unref(prev_infdir);
                pmath_unref(next_infdir);
                pmath_unref(prev);
                pmath_unref(p);
                pmath_unref(next);
                pmath_unref(expr);
                return pmath_ref(PMATH_SYMBOL_FALSE);
              }
              else if(start == i-1)
                start++;
              else if(old_prev_was_true){
                have_marker_after_start = TRUE;
                expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
              }
            }
            
            pmath_unref(p);
          }
          
          pmath_unref(prev_infdir);
          pmath_unref(next_infdir);
        }
        
        if(_pmath_is_numeric(prev) && _pmath_is_numeric(next)){
          int c = 0;
          
          if(pmath_instance_of(prev, PMATH_TYPE_MP_FLOAT)){
            double precacc = DBL_MANT_DIG + LOG10_2;
            pmath_t n;
            
            n = pmath_approximate(pmath_ref(next), precacc, HUGE_VAL);
            
            if(!pmath_is_number(n)){
              pmath_unref(n);
              continue;
            }
            
            c = pmath_fuzzy_compare(prev, n);
            if(c == 0){
              precacc = DBL_MANT_DIG + pmath_accuracy(n);
              n = pmath_approximate(pmath_ref(next), HUGE_VAL, precacc);
                
              if(!pmath_is_number(n)){
                pmath_unref(n);
                continue;
              }
              
              c = pmath_fuzzy_compare(prev, n);
            }
            
            pmath_unref(n);
          }
          else if(pmath_instance_of(next, PMATH_TYPE_MP_FLOAT)){
            double precacc = DBL_MANT_DIG + LOG10_2;
            pmath_t p;
            
            p = pmath_approximate(pmath_ref(prev), precacc, HUGE_VAL);
            
            if(!pmath_is_number(p)){
              pmath_unref(p);
              continue;
            }
            
            c = pmath_fuzzy_compare(p, next);
            if(c == 0){
              precacc = DBL_MANT_DIG + pmath_accuracy(p);
              p = pmath_approximate(pmath_ref(prev), HUGE_VAL, precacc);
                
              if(!pmath_is_number(p)){
                pmath_unref(p);
                continue;
              }
              
              c = pmath_fuzzy_compare(p, next);
            }
            
            pmath_unref(p);
          }
          else{
            pmath_bool_t error = FALSE;
            double pprec = DBL_MANT_DIG + LOG10_2;
            double nprec = DBL_MANT_DIG + LOG10_2;
            pmath_t p;
            pmath_t n;
            
            p = pmath_approximate(pmath_ref(prev), pprec, HUGE_VAL);
            n = pmath_approximate(pmath_ref(next), nprec, HUGE_VAL);
            
            if(!pmath_is_number(p) || !pmath_is_number(n)){
              pmath_unref(p);
              pmath_unref(n);
              continue;
            }
            
            c = pmath_fuzzy_compare(p, n);
            if(c == 0 && !pmath_aborting()){
              nprec+= 1;
              while(c == 0){
                pmath_unref(n);
                pmath_unref(p);
                
                p = pmath_approximate(pmath_ref(prev), pprec, HUGE_VAL);
                n = pmath_approximate(pmath_ref(next), nprec, HUGE_VAL);
                    
                if(!pmath_is_number(p) || !pmath_is_number(n)){
                  error = TRUE;
                  break;
                }
                
                c = pmath_fuzzy_compare(p, n);
                
                if(pprec >= DBL_MANT_DIG + pmath_max_extra_precision
                || nprec >= DBL_MANT_DIG + pmath_max_extra_precision){
                  if(c == 0){
                    pmath_message(PMATH_SYMBOL_N, "meprec", 2,
                      pmath_evaluate(pmath_ref(PMATH_SYMBOL_MAXEXTRAPRECISION)),
                      pmath_expr_get_item_range(expr, i-1, 2));
                    error = TRUE;
                  }
                  
                  break;
                }
                
                pprec = 2 * pprec + 33;
                nprec = 2 * nprec + 33;
                
                if(pprec > DBL_MANT_DIG + pmath_max_extra_precision)
                   pprec = DBL_MANT_DIG + pmath_max_extra_precision;
                if(nprec > DBL_MANT_DIG + pmath_max_extra_precision)
                   nprec = DBL_MANT_DIG + pmath_max_extra_precision;
              }
            }
          
            pmath_unref(n);
            pmath_unref(p);
            
            if(error || pmath_aborting())
              continue;
          }
          
          if((c <  0 && (directions & DIRECTION_LESS) == 0)
          || (c == 0 && (directions & DIRECTION_EQUAL) == 0)
          || (c >  0 && (directions & DIRECTION_GREATER) == 0)){
            pmath_unref(prev);
            pmath_unref(next);
            pmath_unref(expr);
            return pmath_ref(PMATH_SYMBOL_FALSE);
          }
          
          if(start == i-1){
            start++;
          }
          else if(old_prev_was_true){
            have_marker_after_start = TRUE;
            expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          }
          
          continue;
        }
          
        prev_was_true = FALSE;
        
      }while(0);
      
      pmath_unref(prev);
      prev = next;
      if(pmath_same(prev, PMATH_SYMBOL_UNDEFINED)){
        pmath_unref(expr);
        return prev;
      }
    }
    
    pmath_unref(prev);
    if(start >= len){
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_TRUE);
    }
    
    if(prev_was_true){
      have_marker_after_start = TRUE;
      expr = pmath_expr_set_item(expr, len, PMATH_UNDEFINED);
    }
    
    {
      pmath_t result;
      
      if(start > 1){
        result = pmath_expr_get_item_range(expr, start, len);
        pmath_unref(expr);
      }
      else
        result = expr;
        
      if(have_marker_after_start)
        return pmath_expr_remove_all(result, PMATH_UNDEFINED);
      return result;
    }
  }
  
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_less(pmath_expr_t expr){
  return ordered(expr, DIRECTION_LESS);
}

PMATH_PRIVATE pmath_t builtin_lessequal(pmath_expr_t expr){
  return ordered(expr, DIRECTION_LESS | DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_greater(pmath_expr_t expr){
  return ordered(expr, DIRECTION_GREATER);
}

PMATH_PRIVATE pmath_t builtin_greaterequal(pmath_expr_t expr){
  return ordered(expr, DIRECTION_GREATER | DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_equal(pmath_expr_t expr){
  return ordered(expr, DIRECTION_EQUAL);
}

PMATH_PRIVATE pmath_t builtin_unequal(pmath_expr_t expr){
  pmath_bool_t have_marker = FALSE;
  size_t i, len;
  pmath_t a;
  
  len = pmath_expr_length(expr);
  if(len <= 1){
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }

  for(i = 1;i < len;i++){
    size_t j;
    
    a = pmath_expr_get_item(expr, i);
    if(pmath_same(a, PMATH_SYMBOL_UNDEFINED)){
      pmath_unref(expr);
      return a;
    }
    
    for(j = i+1;j <= len;j++){
      pmath_t b = pmath_expr_get_item(expr, j);
      if(pmath_equals(a, b)){
        pmath_unref(a);
        pmath_unref(b);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      else if(!pmath_is_symbol(a) && !pmath_is_expr(a)){
        have_marker = TRUE;
        expr = pmath_expr_set_item(expr, j, PMATH_UNDEFINED);
      }
      pmath_unref(b);
    }
    pmath_unref(a);
  }
  
  a = pmath_expr_get_item(expr, len);
  if(pmath_same(a, PMATH_SYMBOL_UNDEFINED)){
    pmath_unref(expr);
    return a;
  }
  pmath_unref(a);
  
  if(have_marker)
    return pmath_expr_remove_all(expr, PMATH_UNDEFINED);

  return expr;
}

static int relation_direction(pmath_t rel){
  if(pmath_same(rel, PMATH_SYMBOL_EQUAL))        return DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_LESS))         return DIRECTION_LESS;
  if(pmath_same(rel, PMATH_SYMBOL_LESSEQUAL))    return DIRECTION_LESS | DIRECTION_EQUAL;
  if(pmath_same(rel, PMATH_SYMBOL_GREATER))      return DIRECTION_GREATER;
  if(pmath_same(rel, PMATH_SYMBOL_GREATEREQUAL)) return DIRECTION_GREATER | DIRECTION_EQUAL;
  return 0;
}

static pmath_t combine_relations(pmath_t rel1, pmath_t rel2){
  if(pmath_same(rel1, PMATH_SYMBOL_EQUAL))
    return rel2;
  
  if(pmath_same(rel2, PMATH_SYMBOL_EQUAL))
    return rel1;
  
  if(pmath_same(rel1, PMATH_SYMBOL_UNEQUAL)
  || pmath_same(rel2, PMATH_SYMBOL_UNEQUAL))
    return PMATH_NULL;
  
  if(pmath_same(rel1, PMATH_SYMBOL_LESS)){
    if(pmath_same(rel2, PMATH_SYMBOL_LESS)
    || pmath_same(rel2, PMATH_SYMBOL_LESSEQUAL))
      return rel1;
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_LESSEQUAL)){
    if(pmath_same(rel2, PMATH_SYMBOL_LESS)
    || pmath_same(rel2, PMATH_SYMBOL_LESSEQUAL))
      return rel2;
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_GREATER)){
    if(pmath_same(rel2, PMATH_SYMBOL_GREATER)
    || pmath_same(rel2, PMATH_SYMBOL_GREATEREQUAL))
      return rel1;
  }
  
  if(pmath_same(rel1, PMATH_SYMBOL_GREATEREQUAL)){
    if(pmath_same(rel2, PMATH_SYMBOL_GREATER)
    || pmath_same(rel2, PMATH_SYMBOL_GREATEREQUAL))
      return rel2;
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_inequation(pmath_expr_t expr){
  pmath_bool_t prev_was_true = TRUE;
  pmath_bool_t have_marker = FALSE;
  pmath_t prev;
  pmath_t prev_relation = pmath_ref(PMATH_SYMBOL_EQUAL);
  int direction;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if((len & 1) != 1)
    return expr;

  if(len == 1){
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  if(len == 3){
    pmath_t result = pmath_expr_new_extended(
      pmath_expr_get_item(expr, 2), 2,
      pmath_expr_get_item(expr, 1),
      pmath_expr_get_item(expr, 3));
    pmath_unref(expr);
    return result;
  }

  direction = 0;
  prev = pmath_expr_get_item(expr, 1);
  
  for(i = 2;i <= len;i+= 2){
    pmath_t relation = pmath_expr_get_item(expr, i);
    pmath_t next     = pmath_expr_get_item(expr, i+1);

    pmath_t test = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(relation), 2,
        pmath_ref(prev),
        pmath_ref(next)));

    if(pmath_same(test, PMATH_SYMBOL_FALSE)
    || pmath_same(test, PMATH_SYMBOL_UNDEFINED)){
      pmath_unref(relation);
      pmath_unref(prev);
      pmath_unref(next);
      pmath_unref(expr);
      return test;
    }
    
    if(pmath_same(test, PMATH_SYMBOL_TRUE)){
      if(prev_was_true){
        pmath_t new_relation = combine_relations(prev_relation, relation);
        
        if(!pmath_is_null(new_relation)){
          have_marker = TRUE;
          expr = pmath_expr_set_item(expr, i-2, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, i-1, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, i,   pmath_ref(new_relation));
        }
      }
      
      if(i+1 == len){
        have_marker = TRUE;
        expr = pmath_expr_set_item(expr, i,   PMATH_UNDEFINED); // relation
        expr = pmath_expr_set_item(expr, i+1, PMATH_UNDEFINED); // next
      }
    }
    else{
      int new_direction = relation_direction(relation);
      
      if(i == 2){
        direction = new_direction;
      }
      else if(direction == 0 || new_direction == 0
      || ((new_direction & DIRECTION_LESS) != 0
           && (direction & DIRECTION_GREATER) != 0)
      || ((new_direction & DIRECTION_GREATER) != 0
           && (direction & DIRECTION_LESS) != 0))
      {
        pmath_expr_t until_here;
        pmath_t rest;
        
        until_here = pmath_expr_get_item_range(expr, 1, i - 1);
        
        if(have_marker)
          until_here = pmath_expr_remove_all(until_here, PMATH_UNDEFINED);

        rest = pmath_expr_get_item_range(expr, i-1, SIZE_MAX);

        pmath_unref(test);
        pmath_unref(relation);
        pmath_unref(prev);
        pmath_unref(next);
        pmath_unref(expr);
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_AND), 2,
          until_here,
          rest);
      }
    }
    
    prev_was_true = pmath_same(test, PMATH_SYMBOL_TRUE);
    pmath_unref(test);
    pmath_unref(prev_relation);
    prev_relation = relation;
    pmath_unref(prev);
    prev = next;
  }
  
  pmath_unref(prev);
  if(have_marker)
    return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
  return expr;
}
