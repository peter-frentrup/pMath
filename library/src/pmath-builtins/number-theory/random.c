#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control-private.h>


static pmath_t stretch(
  pmath_t x,   // will be freed
  pmath_t min, // will be freed, may be PMATH_UNDEFINED
  pmath_t max  // will be freed, may be PMATH_UNDEFINED iff min == PMATH_UNDEFINED
){ // return min + x/(max - min)
  if(pmath_same(min, PMATH_UNDEFINED)){
    if(pmath_same(max, PMATH_UNDEFINED))
      return x;
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_TIMES), 2, 
        x, max);
  }
  else{
    pmath_t dist = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      max,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        PMATH_FROM_INT32(-1),
        pmath_ref(min)));
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      min,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        x,
        dist));
  }
}

#define MAX_DIM  10

struct random_array_data_t{
  size_t          dim;
  size_t          dims;
  size_t          lengths[MAX_DIM];
  pmath_t         min;
  pmath_t         max;
  double          working_precision;
  mpfr_prec_t     prec;
  pmath_bool_t    random_integer; // max is INTEGER !!! and one bigger than real max!!!
};

static pmath_expr_t random_array(
  struct random_array_data_t *data
){
  pmath_expr_t list;
  size_t i;
  
  if(data->dim == data->dims){
    if(data->random_integer){
      pmath_mpint_t result = _pmath_create_mp_int(0);
      
      assert(pmath_is_integer(data->max));
      
      if(!pmath_is_null(result)){
        if(pmath_is_int32(data->max)){
          pmath_mpint_t max = _pmath_create_mp_int(PMATH_AS_INT32(data->max));
          if(pmath_is_null(max)){
            pmath_unref(result);
            return PMATH_NULL;
          }
            
          data->max = max;
        }
      
        pmath_atomic_lock(&_pmath_rand_spinlock);
        
        mpz_urandomm(
          PMATH_AS_MPZ(result), 
          _pmath_randstate,
          PMATH_AS_MPZ(data->max));
        
        pmath_atomic_unlock(&_pmath_rand_spinlock);
        
        result = _pmath_mp_int_normalize(result);
        
        if(!pmath_same(data->min, PMATH_UNDEFINED))
          return pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            pmath_ref(data->min),
            result);
        
        return result;
      }
      
      return PMATH_NULL;
    }
    else{
//      mpfr_prec_t prec = data->working_precision;
//        
//      struct _pmath_mp_float_t *result = _pmath_create_mp_float(prec ? prec : DBL_MANT_DIG);
      pmath_mpfloat_t result = _pmath_create_mp_float(data->prec);
      
      if(!pmath_is_null(result)){
        pmath_atomic_lock(&_pmath_rand_spinlock);
        
        mpfr_urandomb(PMATH_AS_MP_VALUE(result), _pmath_randstate);
        
        pmath_atomic_unlock(&_pmath_rand_spinlock);
        
        if(data->working_precision == -HUGE_VAL){
          double res = mpfr_get_d(PMATH_AS_MP_VALUE(result), MPFR_RNDN);
          
          if(isfinite(res)){
            pmath_unref(result);
            
            return stretch(
              PMATH_FROM_DOUBLE(res), 
              pmath_ref(data->min), 
              pmath_ref(data->max));
          }
        }
        
        mpfr_set_d( PMATH_AS_MP_ERROR(result), -data->working_precision, MPFR_RNDN);
        mpfr_ui_pow(PMATH_AS_MP_ERROR(result), 2, PMATH_AS_MP_ERROR(result), MPFR_RNDU);
        mpfr_mul(   PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), PMATH_AS_MP_VALUE(result), MPFR_RNDU);
        
        return stretch(
          result, 
          pmath_ref(data->min), 
          pmath_ref(data->max));
      }
      return PMATH_NULL;
    }
  }
  
  data->dim++;

  list = pmath_expr_new(
    pmath_ref(PMATH_SYMBOL_LIST),
    data->lengths[data->dim-1]);

  for(i = 1;i <= data->lengths[data->dim-1];++i){
    list = pmath_expr_set_item(list, i, random_array(data));
  }

  data->dim--;
  return list;
}

PMATH_PRIVATE pmath_t builtin_randominteger(pmath_expr_t expr){
/* RandomInteger(a..b, n)
   RandomInteger(a..b)
   RandomInteger(b)     = RandomInteger(0..b)
   RandomInteger()      = RandomInteger(0..1)
   
   Messages:
     General::range
 */
  struct random_array_data_t data;
  size_t exprlen;
  
  data.dim = 0;
  data.dims = 0;
  data.min = PMATH_UNDEFINED;
  data.max = PMATH_NULL;
  data.working_precision = -HUGE_VAL;//PMATH_MACHINE_PRECISION;
  data.prec = DBL_MANT_DIG;
  data.random_integer = TRUE;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen > 2){
    pmath_message_argxxx(exprlen, 0, 2);
    return expr;
  }
  
  if(exprlen > 1){
    pmath_t dims = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_int32(dims) && PMATH_AS_INT32(dims) >= 0){
      data.dims = 1;
      data.lengths[0] = (unsigned)PMATH_AS_INT32(dims);
    }
    else if(pmath_is_expr(dims)){
      pmath_t h = pmath_expr_get_item(dims, 0);
      pmath_unref(h);
      
      if(pmath_same(h, PMATH_SYMBOL_LIST)){
        data.dims = pmath_expr_length(dims);
        
        if(data.dims > MAX_DIM){
          pmath_unref(dims);
          return expr;
        }
        
        for(data.dim = 0;data.dim < data.dims;++data.dim){
          pmath_t len = pmath_expr_get_item(dims, data.dim + 1);
          if(!pmath_is_int32(len) || PMATH_AS_INT32(len) < 0){
            pmath_unref(len);
            goto ERROR_COND;
          }
          
          data.lengths[data.dim] = (unsigned)PMATH_AS_INT32(len);
          pmath_unref(len);
        }
      }
      else
        goto ERROR_COND;
    }
    else{ ERROR_COND:
      pmath_unref(dims);
      
      pmath_message(
        PMATH_NULL, "ilsmn", 2,
        PMATH_FROM_INT32(2),
        pmath_ref(expr));
      
      return expr;
    }
      
    pmath_unref(dims);
  }
  
  if(exprlen > 0){
    pmath_t range = pmath_expr_get_item(expr, 1);
    if(pmath_is_integer(range)){
      data.max = _add_nn(range, PMATH_FROM_INT32(1));
      if(pmath_is_null(data.max)){
        pmath_unref(expr);
        return PMATH_NULL;
      }
      
      if(pmath_compare(data.max, PMATH_FROM_INT32(1)) < 0){
        range = pmath_expr_get_item(expr, 1);
        goto RANGE_ERROR;
      }
    }
    else if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
      data.min = pmath_expr_get_item(range, 1);
      if(!pmath_is_integer(data.min)){
        pmath_unref(data.min);
        goto RANGE_ERROR;
      }
        
      data.max = pmath_expr_get_item(range, 2);
      if(!pmath_is_integer(data.max)){
        pmath_unref(data.min);
        pmath_unref(data.max);
        goto RANGE_ERROR;
      }
      
      data.max = _add_nn(
        data.max,
        PMATH_FROM_INT32(1));
      
      data.max = _add_nn(
        data.max,
        pmath_number_neg(pmath_ref(data.min)));
      
      if(pmath_is_null(data.max)){
        pmath_unref(data.min);
        pmath_unref(range);
        pmath_unref(expr);
        return PMATH_NULL;
      }
      
      if(pmath_compare(data.max, PMATH_FROM_INT32(1)) < 0){
        pmath_unref(data.min);
        pmath_unref(data.max);
        goto RANGE_ERROR;
      }
      
      pmath_unref(range);
    }
    else{ RANGE_ERROR:
      pmath_message(PMATH_NULL, "range", 1, range);
      return expr;
    }
  }
  else{
    data.max = PMATH_FROM_INT32(2);
  }
  
  data.dim = 0;
  pmath_unref(expr);
  expr = random_array(&data);
  pmath_unref(data.min);
  pmath_unref(data.max);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_randomreal(pmath_expr_t expr){
/* RandomReal(a..b, n)
   RandomReal(a..b)
   RandomReal(b)     = RandomReal(0..b)
   RandomReal()      = RandomReal(0..1)
   
   Options:
     WorkingPrecision -> MachinePrecision
   
   Messages:
     General::invprec
 */
  struct random_array_data_t data;
  pmath_expr_t options;
  pmath_t prec_obj;
  size_t exprlen, last_nonoption;
  
  data.dim = 0;
  data.dims = 0;
  data.min = PMATH_UNDEFINED;
  data.max = PMATH_UNDEFINED;
  data.working_precision = -HUGE_VAL;//PMATH_MACHINE_PRECISION;
  data.prec = DBL_MANT_DIG;
  data.random_integer = FALSE;
  last_nonoption = 0;
  options = PMATH_UNDEFINED;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen > 0){
    pmath_t tmp = pmath_expr_get_item(expr, 1);
    if(_pmath_is_rule(tmp) || _pmath_is_list_of_rules(tmp)){
      pmath_unref(tmp);
    }
    else{
      pmath_unref(tmp);
      last_nonoption = 1;
      
      if(exprlen > 1){
        tmp = pmath_expr_get_item(expr, 2);
        
        if(pmath_is_int32(tmp) && PMATH_AS_INT32(tmp) >= 0){
          data.dims = 1;
          data.lengths[0] = (unsigned)PMATH_AS_INT32(tmp);
          last_nonoption = 2;
        }
        else if(!_pmath_is_rule(tmp) 
        && !_pmath_is_list_of_rules(tmp)){
          if(pmath_is_expr(tmp)){
            pmath_t h = pmath_expr_get_item(tmp, 0);
            pmath_unref(h);
            
            if(pmath_same(h, PMATH_SYMBOL_LIST)){
              data.dims = pmath_expr_length(tmp);
              
              if(data.dims > MAX_DIM){
                pmath_unref(tmp);
                return expr;
              }
              
              for(data.dim = 0;data.dim < data.dims;++data.dim){
                pmath_t len = pmath_expr_get_item(tmp, data.dim + 1);
                if(!pmath_is_int32(len) || PMATH_AS_INT32(len) < 0){
                  pmath_unref(len);
                  goto ERROR_COND;
                }
                
                data.lengths[data.dim] = (unsigned)PMATH_AS_INT32(len);
                pmath_unref(len);
              }
              
              last_nonoption = 2;
            }
            else
              goto ERROR_COND;
          }
          else{ ERROR_COND:
            pmath_unref(tmp);
            
            pmath_message(
              PMATH_NULL, "ilsmn", 2,
              PMATH_FROM_INT32(2),
              pmath_ref(expr));
            
            return expr;
          }
        }
        else
          last_nonoption = 1;
          
        pmath_unref(tmp);
      }
    }
    
    options = pmath_options_extract(expr, last_nonoption);
  }
  
  prec_obj = pmath_option_value(
    PMATH_SYMBOL_RANDOMREAL, 
    PMATH_SYMBOL_WORKINGPRECISION,
    options);
  pmath_unref(options);
  
  if(!_pmath_to_precision(prec_obj, &data.working_precision)
  || data.working_precision > PMATH_MP_PREC_MAX){
    pmath_message(PMATH_NULL, "invprec", 1, prec_obj);
    return expr;
  }
  
  pmath_unref(prec_obj);
  
  if(data.working_precision == -HUGE_VAL)
    data.prec = DBL_MANT_DIG;
  else if(data.working_precision < MPFR_PREC_MIN)
    data.prec = MPFR_PREC_MIN;
  else
    data.prec = (mpfr_prec_t)data.working_precision;
  
  if(last_nonoption > 0){
    pmath_t range = pmath_approximate(
      pmath_expr_get_item(expr, 1), 
      data.working_precision,
      HUGE_VAL,
      NULL);
      
    if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
      data.min = pmath_expr_get_item(range, 1);
      data.max = pmath_expr_get_item(range, 2);
    }
    else
      data.max = pmath_ref(range);
    
    pmath_unref(range);
  }
  
  data.dim = 0;
  pmath_unref(expr);
  expr = random_array(&data);
  pmath_unref(data.min);
  pmath_unref(data.max);
  return expr;
}
