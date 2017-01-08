#include <pmath-util/approximate.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/intervals-private.h>
#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/debug.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_API
double pmath_precision(pmath_t obj) { // will be freed
// -HUGE_VAL = machine precision
  double prec;

  if(pmath_is_double(obj)) {
    pmath_unref(obj);
    return -HUGE_VAL;
  }

  if(pmath_is_mpfloat(obj)) {
    mpfr_prec_t prec = mpfr_get_prec(PMATH_AS_MP_VALUE(obj));
    pmath_unref(obj);
    return prec;
  }
  
  if(pmath_is_interval(obj)) {
    mpfr_prec_t prec = mpfi_get_prec(PMATH_AS_MP_INTERVAL(obj));
    pmath_unref(obj);
    return prec;
  }

  if(pmath_is_packed_array(obj)) {
    switch(pmath_packed_array_get_element_type(obj)) {
      case PMATH_PACKED_INT32:
        pmath_unref(obj);
        return HUGE_VAL;

      case PMATH_PACKED_DOUBLE:
        pmath_unref(obj);
        return -HUGE_VAL;
    }
  }

  prec = HUGE_VAL;

  if(pmath_is_expr(obj)) {
    size_t i;

    for(i = 0; i <= pmath_expr_length(obj); ++i) {
      double prec2 = pmath_precision(
                       pmath_expr_get_item(obj, i));

      if(prec2 < prec) {
        prec = prec2;

        if(prec == -HUGE_VAL)
          break;
      }
    }
  }

  pmath_unref(obj);
  return prec;
}

static pmath_t set_infinite_precision_number(pmath_number_t obj) {
  if(pmath_is_double(obj)) {
    pmath_float_t mp = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(obj));

    if(pmath_is_null(mp))
      return obj;

    pmath_unref(obj);
    obj = mp;
  }

  if(pmath_is_mpfloat(obj)) {
    pmath_mpint_t num;
    mpfr_exp_t exp;

    num = _pmath_create_mp_int(0);
    if(!pmath_is_null(num)) {
      exp = mpfr_get_z_2exp(PMATH_AS_MPZ(num), PMATH_AS_MP_VALUE(obj));

      if(exp < 0 && mpz_sgn(PMATH_AS_MPZ(num)) != 0) {
        pmath_mpint_t den = _pmath_create_mp_int(0);

        if(!pmath_is_null(den)) {
          mpz_set_ui(PMATH_AS_MPZ(den), 1);
          mpz_mul_2exp(PMATH_AS_MPZ(den), PMATH_AS_MPZ(den), (unsigned long int) - exp);

          pmath_unref(obj);
          return pmath_rational_new(_pmath_mp_int_normalize(num), _pmath_mp_int_normalize(den));
        }
      }
      else {
        mpz_mul_2exp(PMATH_AS_MPZ(num), PMATH_AS_MPZ(num), (unsigned long int)exp);
        pmath_unref(obj);
        return _pmath_mp_int_normalize(num);
      }

      pmath_unref(num);
    }

    return obj;
  }

  return obj;
}

static pmath_t set_finite_precision_interval(pmath_number_t obj, double prec) {
  pmath_interval_t result;

  if(prec >= PMATH_MP_PREC_MAX) {
    pmath_unref(obj);
    return PMATH_NULL; // overflow message?
  }

  if(prec < 0)
    prec = 0; // error message?

  result = _pmath_create_interval((mpfr_prec_t)ceil(prec));
  if(pmath_is_null(result)) {
    pmath_unref(obj);
    return PMATH_NULL;
  }

  if(_pmath_interval_set_point(PMATH_AS_MP_INTERVAL(result), obj)) {
    pmath_unref(obj);
    return result;
  }

  pmath_unref(obj);
  pmath_unref(result);

  return PMATH_NULL;
}

static pmath_t set_finite_precision_number(pmath_number_t obj, double prec) {
  pmath_float_t result;

  if(prec >= PMATH_MP_PREC_MAX) {
    pmath_unref(obj);
    return PMATH_NULL; // overflow message?
  }

  if(prec < 0)
    prec = 0; // error message?

  result = _pmath_create_mp_float((mpfr_prec_t)ceil(prec));
  if(pmath_is_null(result)) {
    pmath_unref(obj);
    return PMATH_NULL;
  }

  if(pmath_is_double(obj)) {
    mpfr_set_d(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_DOUBLE(obj),
      _pmath_current_rounding_mode());
  }
  else if(pmath_is_int32(obj)) {
    mpfr_set_si(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_INT32(obj),
      _pmath_current_rounding_mode());
  }
  else switch(PMATH_AS_PTR(obj)->type_shift) {
      case PMATH_TYPE_SHIFT_MP_INT: {
          mpfr_set_z(
            PMATH_AS_MP_VALUE(result),
            PMATH_AS_MPZ(obj),
            _pmath_current_rounding_mode());
        } break;

      case PMATH_TYPE_SHIFT_QUOTIENT: {
          mpq_t tmp_quot;

          mpq_init(tmp_quot);
          if(pmath_is_int32(PMATH_QUOT_NUM(obj))) {
            mpz_set_si(mpq_numref(tmp_quot), PMATH_AS_INT32(PMATH_QUOT_NUM(obj)));
          }
          else {
            assert(pmath_is_mpint(PMATH_QUOT_NUM(obj)));

            mpz_set(mpq_numref(tmp_quot), PMATH_AS_MPZ(PMATH_QUOT_NUM(obj)));
          }

          if(pmath_is_int32(PMATH_QUOT_DEN(obj))) {
            mpz_set_si(mpq_denref(tmp_quot), PMATH_AS_INT32(PMATH_QUOT_DEN(obj)));
          }
          else {
            assert(pmath_is_mpint(PMATH_QUOT_DEN(obj)));

            mpz_set(mpq_denref(tmp_quot), PMATH_AS_MPZ(PMATH_QUOT_DEN(obj)));
          }

          mpfr_set_q(
            PMATH_AS_MP_VALUE(result),
            tmp_quot,
            _pmath_current_rounding_mode());

          mpq_clear(tmp_quot);
        } break;

      case PMATH_TYPE_SHIFT_MP_FLOAT: {
          if(mpfr_get_prec(PMATH_AS_MP_VALUE(result)) == mpfr_get_prec(PMATH_AS_MP_VALUE(obj))) {
            pmath_unref(result);
            return obj;
          }
          mpfr_set(
            PMATH_AS_MP_VALUE(result),
            PMATH_AS_MP_VALUE(obj),
            _pmath_current_rounding_mode());
        } break;
    }

  pmath_unref(obj);
  return result;
}

struct set_precision_data_t {
  double       prec;
  pmath_t      prec_obj;
  pmath_bool_t interval;
};

static pmath_t set_precision(
  pmath_t  obj, // will be freed
  size_t   dummy_index,
  void    *_data // A struct set_precision_data_t*
) {
  struct set_precision_data_t *data = _data;
  pmath_symbol_t sym;

START_SET_PRECISION:

  if(pmath_is_number(obj)) {
    double prec = data->prec;

    if(!isfinite(prec)) {
      double d;

      if(prec > 0)
        return set_infinite_precision_number(obj);

      if(!data->interval) {
        if(pmath_is_double(obj))
          return obj;

        d = pmath_number_get_d(obj);

        if(isfinite(d)) {
          pmath_unref(obj);
          return PMATH_FROM_DOUBLE(d);
        }

        // Overflow during conversion to double.
      }

      prec = DBL_MANT_DIG;
    }

    if(data->interval)
      return set_finite_precision_interval(obj, prec);

    return set_finite_precision_number(obj, prec);
  }
  
  if(pmath_is_interval(obj)) {
    double prec = data->prec;
    pmath_interval_t result;
    
    if(!isfinite(prec)) {
      if(prec > 0) { // return interval with rational endpoints
        pmath_t inf = pmath_interval_get_left(obj);
        pmath_t sup = pmath_interval_get_right(obj);
        
        pmath_unref(obj);
        inf = set_precision(inf, dummy_index, _data);
        sup = set_precision(inf, dummy_index, _data);
        
        return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_INTERVAL), 2, inf, sup);
      }
      else
        prec = DBL_MANT_DIG;
    }
    
    if(prec >= PMATH_MP_PREC_MAX) {
      pmath_unref(obj);
      return PMATH_NULL; // overflow message?
    }

    if(prec < 0)
      prec = 0; // error message?

    result = _pmath_create_interval((mpfr_prec_t)ceil(prec));
    if(!pmath_is_null(result)) {
      if(mpfi_get_prec(PMATH_AS_MP_INTERVAL(result)) == mpfi_get_prec(PMATH_AS_MP_INTERVAL(obj))) {
        pmath_unref(result);
        return obj;
      }
      mpfi_set(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(obj));
    }
    
    pmath_unref(obj);
    return result;
  }

  if(pmath_is_packed_array(obj)) {
    switch(pmath_packed_array_get_element_type(obj)) {
      case PMATH_PACKED_INT32:
        if(data->prec == HUGE_VAL)
          return obj;

        if(data->prec == -HUGE_VAL)
          return pmath_to_packed_array(obj, PMATH_PACKED_DOUBLE);

        break;

      case PMATH_PACKED_DOUBLE:
        if(data->prec == -HUGE_VAL)
          return obj;

        break;
    }
  }

  sym = _pmath_topmost_symbol(obj);
  if(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST)) {
    /* Rule out List(...) to be consistent with packed-array behaviour.
       Nobody should override SetPrecision({...}, ...) anyways.
     */

    struct _pmath_symbol_rules_t  *rules;
    pmath_t result;

    rules = _pmath_symbol_get_rules(sym, RULES_READ);

    if(rules) {
      pmath_bool_t found = FALSE;

      if(data->interval) {
        result = pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_INTERNAL_SETPRECISIONINTERVAL), 2,
                   pmath_ref(obj),
                   pmath_ref(data->prec_obj));

        found = _pmath_rulecache_find(&rules->approx_rules, &result);
        if(!found)
          pmath_unref(result);
      }
      if(!found) {
        result = pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_SETPRECISION), 2,
                   pmath_ref(obj),
                   pmath_ref(data->prec_obj));

        found = _pmath_rulecache_find(&rules->approx_rules, &result);
      }

      if(found) {
        pmath_unref(sym);
        pmath_unref(obj);
        //return result;
        obj = pmath_evaluate(result);

        // TODO? : Warn if interval was requested, but approximate number returned

        if(!data->interval && pmath_is_interval(obj)) {
          pmath_t mid = _pmath_interval_get_value(obj, mpfi_mid);
          pmath_unref(obj);
          obj = mid;
        }

        if(pmath_aborting())
          return obj;

        goto START_SET_PRECISION;
      }

      pmath_unref(result);
    }

    if(_pmath_run_approx_code(sym, &obj, data->prec, data->interval)) {
//      if(data->interval) {
//        if(pmath_is_expr(obj))
//          obj = pmath_evaluate(obj);
//        
//        if(!pmath_is_interval(obj)) {
//          pmath_debug_print_object("[interval expected, but ", obj , " given]\n");
//        }
//      }
//      else {
//        if(pmath_is_expr(obj))
//          obj = pmath_evaluate(obj);
//          
//        if(!pmath_is_float(obj)) {
//          pmath_debug_print_object("[floating point number expected, but ", obj , " given]\n");
//        }
//      }

      pmath_unref(sym);
      return obj;
    }

    pmath_unref(sym);
  }

  if(pmath_is_expr(obj)) {
    obj = _pmath_expr_map(obj, 1, SIZE_MAX, set_precision, data);

    return obj;
  }

  return obj;
}

PMATH_API
pmath_t pmath_set_precision(pmath_t obj, double prec) {
  struct set_precision_data_t data;

  data.prec = prec;
  data.prec_obj = _pmath_from_precision(prec);
  data.interval = FALSE;

  obj = set_precision(obj, 1, &data);

  pmath_unref(data.prec_obj);
  obj = pmath_evaluate(obj);
  return obj;
}

PMATH_API
pmath_t pmath_set_precision_interval(pmath_t obj, double prec) {
  struct set_precision_data_t data;

  data.prec = prec;
  data.prec_obj = _pmath_from_precision(prec);
  data.interval = TRUE;

  obj = set_precision(obj, 1, &data);

  pmath_unref(data.prec_obj);
  obj = pmath_evaluate(obj);
  return obj;
}
