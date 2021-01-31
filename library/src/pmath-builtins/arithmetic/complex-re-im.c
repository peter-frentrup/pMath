#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_Im;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Re;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Undefined;

PMATH_PRIVATE
pmath_bool_t _pmath_is_imaginary(
  pmath_t *z
) {
  if(pmath_is_expr(*z)) {
    size_t len = pmath_expr_length(*z);
    pmath_t head = pmath_expr_get_item(*z, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_Times)) {
      size_t i;
      pmath_t x = pmath_expr_get_item(*z, 1);
      
      if(pmath_is_number(x)) {
        pmath_unref(x);
        return FALSE;
      }
      
      i = 1;
      while(i <= len) {
        if(pmath_is_expr(x)) {
          if(pmath_expr_length(x) > 2) {
            pmath_unref(x);
            return FALSE;
          }
          
          if(pmath_expr_length(x) == 2) {
            head = pmath_expr_get_item(x, 0);
            pmath_unref(head);
            
            if(pmath_same(head, pmath_System_Complex)) {
              pmath_t xx = pmath_expr_get_item(x, 1);
              
              if(pmath_equals(xx, PMATH_FROM_INT32(0))) {
                pmath_unref(xx);
                
                xx = pmath_expr_get_item(x, 2);
                pmath_unref(x);
                *z = pmath_expr_set_item(*z, i, xx);
                
                return TRUE;
              }
              
              pmath_unref(xx);
              pmath_unref(x);
              return FALSE;
            }
          }
        }
        
        pmath_unref(x);
        ++i;
        x = pmath_expr_get_item(*z, i);
      }
      
      pmath_unref(x);
      return FALSE;
    }
    
    if(pmath_same(head, pmath_System_Complex) && len == 2) {
      pmath_t x = pmath_expr_get_item(*z, 1);
      
      if(pmath_equals(x, PMATH_FROM_INT32(0))) {
        pmath_unref(x);
        x = *z;
        *z = pmath_expr_get_item(x, 2);
        pmath_unref(x);
        return TRUE;
      }
      
      pmath_unref(x);
      return FALSE;
    }
  }
  
  return FALSE;
}

static pmath_bool_t contains_float_or_complex(pmath_expr_t args) {
  size_t i;
  for(i = pmath_expr_length(args); i > 0; --i) {
    pmath_t x = pmath_expr_get_item(args, i);
    if(pmath_is_float(x) || pmath_is_expr_of_len(x, pmath_System_Complex, 2)) {
      pmath_unref(x);
      return TRUE;
    }
    pmath_unref(x);
  }
  return FALSE;
}

PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb(pmath_t *expr, pmath_t x, void (*func)(acb_t, const acb_t, slong)) {
  if(pmath_is_float(x) || pmath_is_expr_of_len(x, pmath_System_Complex, 2)) {
    acb_t z;
    slong prec;
    pmath_bool_t is_machine_prec;
    
    acb_init(z);
    if(_pmath_complex_float_extract_acb(z, &prec, &is_machine_prec, x)) {
      func(z, z, prec);
      if(acb_is_finite(z)) {
        pmath_unref(*expr);
        *expr = _pmath_complex_new_from_acb_destructive(z, is_machine_prec ? -1 : prec);
        return TRUE;
      }
    }
    acb_clear(z);
  }
  return FALSE;
}

PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb_2(pmath_t *expr, pmath_t x, pmath_t y, void (*func)(acb_t, const acb_t, const acb_t, slong)) {
  if( pmath_is_number(x) || pmath_is_expr_of_len(x, pmath_System_Complex, 2) ||
      pmath_is_number(y) || pmath_is_expr_of_len(y, pmath_System_Complex, 2))
  {
    double x_precision = pmath_precision(pmath_ref(x));
    double y_precision = pmath_precision(pmath_ref(y));
    double precision = FLINT_MIN(x_precision, y_precision);
    pmath_t x_approx;
    pmath_t y_approx;
    slong prec;
    acb_t x_c;
    acb_t y_c;
    
    if(!(precision < HUGE_VAL))
      return FALSE;
      
    x_approx = pmath_ref(x);
    y_approx = pmath_ref(y);
    if(x_precision == HUGE_VAL) x_approx = pmath_set_precision(x_approx, precision);
    if(y_precision == HUGE_VAL) y_approx = pmath_set_precision(y_approx, precision);
    
    if(precision == -HUGE_VAL)              prec = DBL_MANT_DIG;
    else if(precision < 2)                  prec = 2;
    else if(precision < PMATH_MP_PREC_MAX)  prec = (slong)precision;
    else                                    prec = PMATH_MP_PREC_MAX;
    
    acb_init(x_c);
    acb_init(y_c);
    if( _pmath_complex_float_extract_acb_for_precision(x_c, x_approx, prec) &&
        _pmath_complex_float_extract_acb_for_precision(y_c, y_approx, prec))
    {
      func(x_c, x_c, y_c, prec);
      if(acb_is_finite(x_c)) {
        pmath_unref(x_approx);
        pmath_unref(y_approx);
        pmath_unref(*expr);
        acb_clear(y_c);
        *expr = _pmath_complex_new_from_acb_destructive(x_c, precision == -HUGE_VAL ? -1 : prec);
        return TRUE;
      }
    }
    pmath_unref(x_approx);
    pmath_unref(y_approx);
    acb_clear(y_c);
    acb_clear(x_c);
  }
  return FALSE;
}

PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb_ex(
  pmath_t *expr,
  pmath_t args, // won't be freed
  void (*func)(acb_t, const acb_ptr args, size_t nargs, slong prec, void *context),
  void *context
) {
  size_t i;
  size_t num_args;
  double precision;
  slong prec;
  acb_struct few_complex_args[4];
  acb_ptr    complex_args;
  acb_t result;
  pmath_bool_t success;
  
  num_args = pmath_expr_length(args);
  if(num_args < 1 || num_args >= SIZE_MAX / sizeof(acb_t))
    return FALSE;
    
  if(!contains_float_or_complex(args))
    return FALSE;
    
  precision = HUGE_VAL;
  for(i = 0; i < num_args; ++i) {
    pmath_t x = pmath_expr_get_item(args, i + 1);
    double x_precision = pmath_precision(x); // frees x
    precision = FLINT_MIN(precision, x_precision);
  }
  
  if(!(precision < HUGE_VAL))
    return FALSE;
  
  if(precision == -HUGE_VAL)              prec = DBL_MANT_DIG;
  else if(precision < 2)                  prec = 2;
  else if(precision < PMATH_MP_PREC_MAX)  prec = (slong)precision;
  else                                    prec = PMATH_MP_PREC_MAX;
    
  if(num_args < sizeof(few_complex_args) / sizeof(few_complex_args[0]))
    complex_args = few_complex_args;
  else
    complex_args = pmath_mem_alloc(sizeof(acb_t) * num_args);
    
  if(!complex_args)
    return FALSE;
    
  success = FALSE;
  for(size_t i = 0; i < num_args; ++i) {
    pmath_t x = pmath_expr_get_item(args, i + 1);
    double x_precision = pmath_precision(pmath_ref(x));
    if(x_precision == HUGE_VAL)
      x = pmath_set_precision(x, precision);
    
    acb_init(&complex_args[i]);
    if(!_pmath_complex_float_extract_acb_for_precision(&complex_args[i], x, prec)) {
      num_args = i + 1;
      success = FALSE;
      goto CLEANUP;
    }
    pmath_unref(x);
  }
  
  acb_init(result);
  func(result, complex_args, num_args, prec, context);
  if(acb_is_finite(result)) {
    pmath_unref(*expr);
    *expr = _pmath_complex_new_from_acb_destructive(result, precision == -HUGE_VAL ? -1 : prec);
    success = TRUE;
  }
  else
    acb_clear(result);
  
CLEANUP:
  for(size_t i = 0;i < num_args;++i)
    acb_clear(&complex_args[i]);
  
  if(complex_args != few_complex_args)
    pmath_mem_free(complex_args);
    
  return success;
}

PMATH_PRIVATE pmath_bool_t _pmath_re_im(
  pmath_t  z,   // will be freed
  pmath_t *re,  // optional output
  pmath_t *im   // optional output
) {
  pmath_t z2;
  
  if(re) *re = PMATH_NULL;
  if(im) *im = PMATH_NULL;
  
  if( pmath_same(z, pmath_System_Undefined)    ||
      pmath_equals(z, _pmath_object_overflow)  ||
      pmath_equals(z, _pmath_object_underflow) ||
      pmath_is_number(z))
  {
    if(re) *re = z;
    if(im) *im = PMATH_FROM_INT32(0);
    return TRUE;
  }
  
  if(pmath_is_expr(z)) {
    pmath_t zhead = pmath_expr_get_item(z, 0);
    pmath_unref(zhead);
    
    if( pmath_expr_length(z) == 1 &&
        (pmath_same(zhead, pmath_System_Re) || pmath_same(zhead, pmath_System_Im)))
    {
      if(re) *re = z;
      if(im) *im = PMATH_FROM_INT32(0);
      return TRUE;
    }
    
    if( pmath_expr_length(z) == 2 &&
        pmath_same(zhead, pmath_System_Complex))
    {
      if(re) *re = pmath_expr_get_item(z, 1);
      if(im) *im = pmath_expr_get_item(z, 2);
      pmath_unref(z);
      return TRUE;
    }
    
    if(pmath_same(zhead, pmath_System_Plus)) {
      size_t i, j;
      
      for(j = 0, i = pmath_expr_length(z); i > 0; --i) {
        pmath_t re2, im2;
        
        if( _pmath_re_im(pmath_expr_get_item(z, i), &re2, &im2) &&
            !_pmath_contains_symbol(re2, pmath_System_Re) &&
            !_pmath_contains_symbol(re2, pmath_System_Im) &&
            !_pmath_contains_symbol(im2, pmath_System_Re) &&
            !_pmath_contains_symbol(im2, pmath_System_Im))
        {
          z = pmath_expr_set_item(z, i, PMATH_UNDEFINED);
          
          if(j == 0) {
            if(re) *re = pmath_expr_new(pmath_ref(pmath_System_Plus), i);
            if(im) *im = pmath_expr_new(pmath_ref(pmath_System_Plus), i);
          }
          
          ++j;
          
          if(re) *re = pmath_expr_set_item(*re, j, pmath_ref(re2));
          if(im) *im = pmath_expr_set_item(*im, j, pmath_ref(im2));
        }
        
        pmath_unref(re2);
        pmath_unref(im2);
      }
      
      if(j == pmath_expr_length(z)) {
        pmath_unref(z);
        return TRUE;
      }
      
      if(j > 0) {
        z = pmath_expr_remove_all(z, PMATH_UNDEFINED);
        
        if(re) {
          z2 = pmath_expr_get_item_range(*re, 1, j);
          
          pmath_unref(*re);
          
          *re = PLUS(z2, FUNC(pmath_ref(pmath_System_Re), pmath_ref(z)));
        }
        
        if(im) {
          z2 = pmath_expr_get_item_range(*im, 1, j);
          
          pmath_unref(*im);
          
          *im = PLUS(z2, FUNC(pmath_ref(pmath_System_Im), pmath_ref(z)));
        }
        
        pmath_unref(z);
        return TRUE;
      }
      
      if(re) { pmath_unref(*re); *re = PMATH_NULL; }
      if(im) { pmath_unref(*im); *im = PMATH_NULL; }
      
      pmath_unref(z);
      return FALSE;
    }
    
    if(pmath_same(zhead, pmath_System_Times) && pmath_expr_length(z) > 0) {
      pmath_t re2 = INT(1);
      pmath_t im2 = INT(0);
      size_t i;
      size_t zlen = pmath_expr_length(z);
      
      for(i = 1; i <= zlen; ++i) {
        pmath_t zi = pmath_expr_get_item(z, i);
        pmath_t re_zi, im_zi;
        pmath_t tmp_re, tmp_im;
        
        if(!_pmath_re_im(zi, &re_zi, &im_zi) ||
            _pmath_contains_symbol(re_zi, pmath_System_Re) ||
            _pmath_contains_symbol(re_zi, pmath_System_Im) ||
            _pmath_contains_symbol(im_zi, pmath_System_Re) ||
            _pmath_contains_symbol(im_zi, pmath_System_Im))
        {
          pmath_unref(re_zi);
          pmath_unref(im_zi);
          
          if(i > 1) {
            if(pmath_equals(re2, INT(0))) {
              // Re(a I * z) = -a Im(z)
              // Im(a I * z) =  a Re(z)
              
              if(re) {
                *re = TIMES3(
                        INT(-1),
                        pmath_ref(im2),
                        FUNC(pmath_ref(pmath_System_Im), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              if(im) {
                *im = TIMES(
                        pmath_ref(im2),
                        FUNC(pmath_ref(pmath_System_Re), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              pmath_unref(re2);
              pmath_unref(im2);
              pmath_unref(z);
              return TRUE;
            }
            
            if(pmath_equals(im2, INT(0))) {
              // Re(a * z) = a Re(z)
              // Im(a * z) = a Im(z)
              
              if(re) {
                *re = TIMES(
                        pmath_ref(re2),
                        FUNC(pmath_ref(pmath_System_Re), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              if(im) {
                *im = TIMES(
                        pmath_ref(re2),
                        FUNC(pmath_ref(pmath_System_Im), pmath_expr_get_item_range(z, i, SIZE_MAX)));
              }
              
              pmath_unref(re2);
              pmath_unref(im2);
              pmath_unref(z);
              return TRUE;
            }
          }
          
          pmath_unref(re2);
          pmath_unref(im2);
          pmath_unref(z);
          return FALSE;
        }
        
        tmp_re = MINUS(TIMES(pmath_ref(re2), pmath_ref(re_zi)), TIMES(pmath_ref(im2), pmath_ref(im_zi)));
        tmp_im = PLUS( TIMES(pmath_ref(re2), pmath_ref(im_zi)), TIMES(pmath_ref(im2), pmath_ref(re_zi)));
        
        pmath_unref(re2);
        pmath_unref(im2);
        pmath_unref(re_zi);
        pmath_unref(im_zi);
        
        re2 = pmath_evaluate(tmp_re);
        im2 = pmath_evaluate(tmp_im);
      }
      
      if(re) *re = pmath_ref(re2);
      if(im) *im = pmath_ref(im2);
      
      pmath_unref(re2);
      pmath_unref(im2);
      pmath_unref(z);
      return TRUE;
    }
    
    {
      pmath_t zinfdir = _pmath_directed_infinity_direction(z);
      if(!pmath_is_null(zinfdir)) {
        pmath_unref(z);
        if(re) *re = pmath_expr_new_extended(
                         pmath_ref(pmath_System_DirectedInfinity), 1,
                         pmath_expr_new_extended(
                           pmath_ref(pmath_System_Re), 1,
                           pmath_ref(zinfdir)));
                           
        if(im) *im = pmath_expr_new_extended(
                         pmath_ref(pmath_System_DirectedInfinity), 1,
                         pmath_expr_new_extended(
                           pmath_ref(pmath_System_Im), 1,
                           pmath_ref(zinfdir)));
                           
        pmath_unref(zinfdir);
        return TRUE;
      }
    }
  }
  
  if(pmath_is_numeric(z)) {
    int z_class = _pmath_number_class(z);
    
    if(z_class & PMATH_CLASS_REAL) {
      if(re) *re = pmath_ref(z);
      if(im) *im = PMATH_FROM_INT32(0);
      pmath_unref(z);
      return TRUE;
    }
    
    if(z_class & PMATH_CLASS_IMAGINARY) {
      if(re) *re = PMATH_FROM_INT32(0);
      if(im) *im = pmath_ref(z);
      pmath_unref(z);
      return TRUE;
    }
  }
  
  pmath_unref(z);
  return FALSE;
}

PMATH_PRIVATE pmath_bool_t _pmath_is_nonreal_complex_number(pmath_t z) {
  pmath_t re, im;
  pmath_bool_t both_numbers;
  
  if(!pmath_is_expr_of_len(z, pmath_System_Complex, 2))
    return FALSE;
    
  re = pmath_expr_get_item(z, 1);
  im = pmath_expr_get_item(z, 2);
  
  both_numbers = pmath_is_number(re) && pmath_is_number(im);
  
  pmath_unref(re);
  pmath_unref(im);
  return both_numbers;
}

PMATH_PRIVATE
pmath_bool_t _pmath_complex_float_extract_acb(
  acb_t         result,
  slong        *precision,
  pmath_bool_t *is_machine_prec,
  pmath_t       complex
) {
  if(pmath_is_double(complex)) {
    _pmath_number_get_arb(acb_realref(result), complex, DBL_MANT_DIG);
    arb_set_ui(acb_imagref(result), 0);
    
    if(precision)       *precision = DBL_MANT_DIG;
    if(is_machine_prec) *is_machine_prec = TRUE;
    
    return TRUE;
  }
  
  if(pmath_is_mpfloat(complex)) {
    _pmath_number_get_arb(acb_realref(result), complex, PMATH_AS_ARB_WORKING_PREC(complex));
    arb_set_ui(acb_imagref(result), 0);
    
    if(precision)       *precision = PMATH_AS_ARB_WORKING_PREC(complex);
    if(is_machine_prec) *is_machine_prec = FALSE;
    
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(complex, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(complex, 1);
    pmath_t im = pmath_expr_get_item(complex, 2);
    
    if(pmath_is_float(re) || pmath_is_float(im)) {
      slong prec = 0;
      if(pmath_is_double(re))       prec = DBL_MANT_DIG;
      else if(pmath_is_mpfloat(re)) prec = PMATH_AS_ARB_WORKING_PREC(re);
      
      if(pmath_is_double(im))       prec = FLINT_MAX(prec, DBL_MANT_DIG);
      else if(pmath_is_mpfloat(im)) prec = FLINT_MAX(prec, PMATH_AS_ARB_WORKING_PREC(im));
      
      if(prec == 0) {
        pmath_unref(re);
        pmath_unref(im);
        return FALSE;
      }
      
      _pmath_number_get_arb(acb_realref(result), re, prec);
      _pmath_number_get_arb(acb_imagref(result), im, prec);
      
      if(precision)       *precision = prec;
      if(is_machine_prec) *is_machine_prec = pmath_is_double(re) || pmath_is_double(im);
      
      pmath_unref(re);
      pmath_unref(im);
      
      return TRUE;
    }
    
    pmath_unref(re);
    pmath_unref(im);
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_complex_float_extract_acb_for_precision(
  acb_t         result,
  pmath_t       complex,
  slong         precision
) {
  if(pmath_is_number(complex)) {
    _pmath_number_get_arb(acb_realref(result), complex, precision);
    arb_set_ui(acb_imagref(result), 0);
    return TRUE;
  }
  if(pmath_is_expr_of_len(complex, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(complex, 1);
    pmath_t im = pmath_expr_get_item(complex, 2);
    
    if(pmath_is_number(re) && pmath_is_number(im)) {
      _pmath_number_get_arb(acb_realref(result), re, precision);
      _pmath_number_get_arb(acb_imagref(result), im, precision);
      pmath_unref(re);
      pmath_unref(im);
      return TRUE;
    }
    
    pmath_unref(re);
    pmath_unref(im);
  }
  return FALSE;
}

static pmath_float_t new_float_from_arb_destructive(arb_t value, slong prec_or_double) {
  pmath_mpfloat_t result;
  
  if(prec_or_double < 0) {
    double d = arf_get_d(arb_midref(value), ARF_RND_NEAR);
    if(isfinite(d)) {
      arb_clear(value);
      return PMATH_FROM_DOUBLE(d);
    }
    
    prec_or_double = DBL_MANT_DIG;
  }
  
  result = _pmath_create_mp_float(prec_or_double);
  if(!pmath_is_null(result))
    arb_swap(PMATH_AS_ARB(result), value);
    
  arb_clear(value);
  return result;
}

PMATH_PRIVATE pmath_t _pmath_complex_new_from_acb_destructive(acb_t value, slong prec_or_double) {
  if(acb_is_real(value)) {
    arb_clear(acb_imagref(value));
    return new_float_from_arb_destructive(acb_realref(value), prec_or_double);
  }
  
  return COMPLEX(
           new_float_from_arb_destructive(acb_realref(value), prec_or_double),
           new_float_from_arb_destructive(acb_imagref(value), prec_or_double));
}

PMATH_PRIVATE pmath_t builtin_complex(pmath_expr_t expr) {
  pmath_t x;
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 2);
  if(pmath_equals(x, PMATH_FROM_INT32(0))) {
    pmath_unref(x);
    x = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return x;
  }
  
  pmath_unref(x);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_re(pmath_expr_t expr) {
  pmath_t re, z;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(!_pmath_re_im(z, &re, NULL))
    return expr;
    
  pmath_unref(expr);
  return re;
}

PMATH_PRIVATE pmath_t builtin_im(pmath_expr_t expr) {
  pmath_t im, z;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(!_pmath_re_im(z, NULL, &im))
    return expr;
    
  pmath_unref(expr);
  return im;
}
