#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_t arcsin_as_log(pmath_t x){
  // -I Log(I x + Sqrt(1 - x^2))
  pmath_t y = TIMES(
    COMPLEX(INT(0), INT(-1)),
    LOG(
      PLUS(
        TIMES(
          COMPLEX(INT(0), INT(1)),
          pmath_ref(x)),
        SQRT(
          MINUS(
            INT(1),
            POW(
              pmath_ref(x), 
              INT(2)))))));
  pmath_unref(x);
  return y;
}

PMATH_PRIVATE pmath_t builtin_arcsin(pmath_expr_t expr){
  pmath_t x;
  int xclass;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_equals(x, PMATH_NUMBER_ZERO)){
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
    double d = pmath_number_get_d(x);
    
    pmath_unref(expr);
    if(d < -1.0 || d > 1.0)
      return arcsin_as_log(x);
    
    pmath_unref(x);
    return pmath_float_new_d(asin(d));
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){ 
    pmath_unref(expr);
    
    if(mpfr_cmp_si(((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value, -1) > 0
    && mpfr_cmp_si(((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value,  1) < 0){
      pmath_float_t result;
      pmath_float_t tmp;
      double dprec;
      long exp;
      
      tmp = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
      if(!pmath_is_null(tmp)){
        // dy = dx / Sqrt(1 - x^2)
        mpfr_sqr(
          PMATH_AS_MP_VALUE(tmp),
          PMATH_AS_MP_VALUE(x),
          MPFR_RNDU);
          
        mpfr_ui_sub(
          PMATH_AS_MP_ERROR(tmp),
          1,
          PMATH_AS_MP_VALUE(tmp),
          MPFR_RNDU);
        
        if(mpfr_sgn(PMATH_AS_MP_ERROR(tmp)) > 0){
          mpfr_sqrt(
            PMATH_AS_MP_VALUE(tmp),
            PMATH_AS_MP_ERROR(tmp),
            MPFR_RNDU);
            
          mpfr_div(
            PMATH_AS_MP_ERROR(tmp),
            PMATH_AS_MP_ERROR(x),
            PMATH_AS_MP_VALUE(tmp),
            MPFR_RNDU);
          
          // precision = -Log(2, dy/Abs(y))
          mpfr_div(
            PMATH_AS_MP_VALUE(tmp),
            PMATH_AS_MP_ERROR(tmp),
            PMATH_AS_MP_VALUE(x),
            MPFR_RNDU);
            
          dprec = mpfr_get_d_2exp(&exp, PMATH_AS_MP_VALUE(tmp), MPFR_RNDN);
          dprec = -log(fabs(dprec)) - exp;
          
          if(dprec < 1)
            dprec = 1;
          else if(dprec > PMATH_MP_PREC_MAX)
            dprec = PMATH_MP_PREC_MAX;
          
          result = _pmath_create_mp_float((mp_prec_t)ceil(dprec));
          if(!pmath_is_null(result)){
            mpfr_swap(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(tmp));
        
            mpfr_asin(
              PMATH_AS_MP_VALUE(result), 
              PMATH_AS_MP_VALUE(x),
              MPFR_RNDN);
            
            pmath_unref(x);
            pmath_unref(tmp);
            return result;
          }
        }
        
        pmath_unref(tmp);
      }
    }
    
    return arcsin_as_log(x);
  }
  
  if(pmath_is_expr_of(x, PMATH_SYMBOL_TIMES)){
    pmath_t fst = pmath_expr_get_item(x, 1);
    
    if(pmath_is_number(fst)){
      if(pmath_number_sign(fst) < 0){
        x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(INT(-1), expr);
      }
    }
    
    pmath_unref(fst);
  }
  
  xclass = _pmath_number_class(x);
  
  if(xclass & PMATH_CLASS_ZERO){
    pmath_unref(expr);
    return x;
  }
  
  if(xclass & PMATH_CLASS_POSONE){
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 2), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(xclass & PMATH_CLASS_NEG){
    x = NEG(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
  }
  
  if(xclass & PMATH_CLASS_INF){
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    pmath_t re, im;
    if(_pmath_re_im(infdir, &re, &im)
    && pmath_is_number(re)
    && pmath_is_number(im)){
      int isgn = pmath_number_sign(im);
      int rsgn = pmath_number_sign(re);
      
      pmath_unref(expr);
      pmath_unref(re);
      pmath_unref(im);
      
      if(isgn < 0)
        return pmath_expr_set_item(x, 1, INT(-1));
      
      if(isgn > 0)
        return pmath_expr_set_item(x, 1, INT(1));
      
      if(rsgn < 0)
        return pmath_expr_set_item(x, 1, INT(1));
      
      if(rsgn > 0)
        return pmath_expr_set_item(x, 1, INT(-1));
      
      return pmath_expr_set_item(x, 1, INT(0));
    }
    
    pmath_unref(re);
    pmath_unref(im);
    return expr;
  }
  
  if(xclass & PMATH_CLASS_COMPLEX){
    pmath_unref(expr);
    return arcsin_as_log(x);
  }
  
  pmath_unref(x);
  return expr;
}
