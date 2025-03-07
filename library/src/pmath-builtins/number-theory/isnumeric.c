
#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/number-theory-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_IsNumeric;
extern pmath_symbol_t pmath_System_Numericalize;
extern pmath_symbol_t pmath_System_True;

static void destroy_symset_entry(void *p) {
  pmath_t entry = PMATH_FROM_PTR(p);
  
  assert(!p || pmath_is_symbol(entry));
  
  pmath_unref(entry);
}

static pmath_bool_t ptr_equal(void *a, void *b) {
  return a == b;
}

static const pmath_ht_class_t symbol_set_class = {
  destroy_symset_entry,
  _pmath_hash_pointer,
  ptr_equal,
  _pmath_hash_pointer,
  ptr_equal
};

static pmath_atomic_t numeric_symbols = PMATH_ATOMIC_STATIC_INIT;

PMATH_PRIVATE pmath_bool_t _pmath_is_inexact(pmath_t obj) {
  if(pmath_is_float(obj))
    return TRUE;
    
  if(_pmath_is_nonreal_complex_number(obj)) {
    pmath_t part = pmath_expr_get_item(obj, 1);
    if(pmath_is_float(part)) {
      pmath_unref(part);
      return TRUE;
    }
    pmath_unref(part);
    
    part = pmath_expr_get_item(obj, 2);
    if(pmath_is_float(part)) {
      pmath_unref(part);
      return TRUE;
    }
    pmath_unref(part);
  }
  
  return FALSE;
}

static int _pmath_arf_simple_real_class(const arf_t x) {
  int sign;
  if(arf_is_nan(x))
    return PMATH_CLASS_UNKNOWN;
    
  sign = arf_sgn(x);
  if(sign < 0) {
    int one = arf_cmp_si(x, -1);
    
    if(one < 0) {
      if(arf_is_finite(x))
        return PMATH_CLASS_NEGBIG;
      else
        return PMATH_CLASS_NEGINF;
    }
    
    if(one == 0)
      return PMATH_CLASS_NEGONE;
      
    return PMATH_CLASS_NEGSMALL;
  }
  
  if(sign > 0) {
    int one = arf_cmp_si(x, 1);
    
    if(one > 0) {
      if(arf_is_finite(x))
        return PMATH_CLASS_POSBIG;
      else
        return PMATH_CLASS_POSINF;
    }
    
    if(one == 0)
      return PMATH_CLASS_POSONE;
      
    return PMATH_CLASS_POSSMALL;
  }
  
  return PMATH_CLASS_ZERO;
}

static int _simple_real_class(pmath_t obj) {
  if(pmath_is_int32(obj)) {
    switch(PMATH_AS_INT32(obj)) {
      case -1: return PMATH_CLASS_NEGONE;
      case  0: return PMATH_CLASS_ZERO;
      case  1: return PMATH_CLASS_POSONE;
    }
    
    if(PMATH_AS_INT32(obj) < 0)
      return PMATH_CLASS_NEGBIG;
      
    return PMATH_CLASS_POSBIG;
  }
  
  if(pmath_is_mpint(obj)) {
    if(mpz_sgn(PMATH_AS_MPZ(obj)) < 0)
      return PMATH_CLASS_NEGBIG;
      
    return PMATH_CLASS_POSBIG;
  }
  
  if(pmath_is_double(obj)) {
    if(PMATH_AS_DOUBLE(obj) < -1)
      return PMATH_CLASS_NEGBIG;
      
    if(PMATH_AS_DOUBLE(obj) == -1)
      return PMATH_CLASS_NEGONE;
      
    if(PMATH_AS_DOUBLE(obj) < 0)
      return PMATH_CLASS_NEGSMALL;
      
    if(PMATH_AS_DOUBLE(obj) == 0)
      return PMATH_CLASS_ZERO;
      
    if(PMATH_AS_DOUBLE(obj) < 1)
      return PMATH_CLASS_POSSMALL;
      
    if(PMATH_AS_DOUBLE(obj) == 1)
      return PMATH_CLASS_POSONE;
      
    return PMATH_CLASS_POSBIG;
  }
  
  if(pmath_is_quotient(obj)) {
    pmath_integer_t num = PMATH_QUOT_NUM(obj);
    pmath_integer_t den = PMATH_QUOT_DEN(obj);
    
    if(pmath_is_int32(num)) {
      if(pmath_is_int32(den)) {
        if( PMATH_AS_INT32(num) <  PMATH_AS_INT32(den) &&
            PMATH_AS_INT32(num) > -PMATH_AS_INT32(den))
        {
          if(PMATH_AS_INT32(num) < 0)
            return PMATH_CLASS_NEGSMALL;
          return PMATH_CLASS_POSSMALL;
        }
      }
      
      if(PMATH_AS_INT32(num) < 0)
        return PMATH_CLASS_NEGSMALL;
      return PMATH_CLASS_POSSMALL;
    }
    
    assert(pmath_is_mpint(num));
    
    if(pmath_is_int32(den)) {
      if(mpz_sgn(PMATH_AS_MPZ(num)) < 0)
        return PMATH_CLASS_NEGBIG;
      return PMATH_CLASS_POSBIG;
    }
    
    assert(pmath_is_mpint(den));
    
    if(mpz_cmpabs(PMATH_AS_MPZ(num), PMATH_AS_MPZ(den)) < 0) {
      if(mpz_sgn(PMATH_AS_MPZ(num)) < 0)
        return PMATH_CLASS_NEGSMALL;
      return PMATH_CLASS_POSSMALL;
    }
    
    if(mpz_sgn(PMATH_AS_MPZ(num)) < 0)
      return PMATH_CLASS_NEGBIG;
    return PMATH_CLASS_POSBIG;
  }
  
  if(pmath_is_mpfloat(obj)) {
    arf_t lower;
    arf_t upper;
    int lclass;
    int uclass;
    arf_init(lower);
    arf_init(upper);
    _pmath_arb_bounds(lower, upper, PMATH_AS_ARB(obj), ARF_PREC_EXACT);
    lclass = _pmath_arf_simple_real_class(lower);
    uclass = _pmath_arf_simple_real_class(upper);
    arf_clear(lower);
    arf_clear(upper);
    if(lclass == uclass)
      return lclass;
    return PMATH_CLASS_REAL;
  }
  
  if(pmath_is_expr_of_len(obj, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(obj, 1);
    pmath_t im = pmath_expr_get_item(obj, 2);
    
    if(pmath_is_number(re) && pmath_is_number(im)) {
      if(pmath_number_sign(im) == 0) {
        int result = _simple_real_class(re);
        
        pmath_unref(re);
        pmath_unref(im);
        return result;
      }
      
      if(pmath_number_sign(re) == 0) {
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
  
  if(_pmath_is_infinite(obj)) {
    pmath_t dir = pmath_expr_get_item(obj, 1);
    
    int dir_class = _simple_real_class(dir);
    pmath_unref(dir);
    
    if(dir_class & PMATH_CLASS_ZERO)
      return PMATH_CLASS_UINF;
      
    if(dir_class & PMATH_CLASS_POS)
      return PMATH_CLASS_POSINF;
      
    if(dir_class & PMATH_CLASS_NEG)
      return PMATH_CLASS_NEGINF;
      
    if(dir_class & PMATH_CLASS_IMAGINARY)
      return PMATH_CLASS_CINF | PMATH_CLASS_IMAGINARY;
      
    return PMATH_CLASS_CINF;
  }
  
  return PMATH_CLASS_UNKNOWN;
}

PMATH_PRIVATE int _pmath_number_class(pmath_t obj) {
  int result = _simple_real_class(obj);
  
  if((result & PMATH_CLASS_UNKNOWN) && pmath_is_numeric(obj)) {
    pmath_thread_t me = pmath_thread_get_current();
    if(me) {
      double prec = me->min_precision;
      double maxprec = me->max_precision;
      
      if(prec < DBL_MANT_DIG)
        prec = DBL_MANT_DIG;
        
      if(maxprec > prec + me->max_extra_precision)
        maxprec = prec + me->max_extra_precision;
        
      while(!pmath_thread_aborting(me)) {
        pmath_t n_obj = pmath_set_precision(pmath_ref(obj), prec);
        int n_class = _simple_real_class(n_obj);
        pmath_unref(n_obj);
        
        if(n_class & PMATH_CLASS_UNKNOWN)
          break;
          
        if(n_class != 0 && (n_class & (n_class - 1)) == 0)  // is power of two: one definite class
          return n_class;
          
        if(prec >= maxprec) {
          pmath_message(
            pmath_System_Numericalize, "meprec", 2,
            _pmath_from_precision(me->max_extra_precision),
            pmath_ref(obj));
          break;
        }
        
        prec = 2 * prec;
        if(prec > maxprec)
          prec = maxprec;
      }
    }
  }
  
  return result;
}

PMATH_API pmath_bool_t pmath_is_numeric(pmath_t obj) {
  if(pmath_is_number(obj))
    return TRUE;
    
  if(pmath_is_expr(obj)) {
    pmath_t h = pmath_expr_get_item(obj, 0);
    if(pmath_is_symbol(h) &&
        (pmath_symbol_get_attributes(h) & PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION) != 0)
    {
      pmath_bool_t result;
      size_t i;
      
      pmath_unref(h);
      for(i = pmath_expr_length(obj); i > 0; --i) {
        h = pmath_expr_get_item(obj, i);
        result = pmath_is_numeric(h);
        pmath_unref(h);
        
        if(!result)
          return FALSE;
      }
      
      return TRUE;
    }
    
    pmath_unref(h);
    return FALSE;
  }
  
  if(pmath_is_symbol(obj)) {
    pmath_bool_t      result;
    pmath_hashtable_t table;
    
    table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&numeric_symbols);
    
    result = pmath_ht_search(table, PMATH_AS_PTR(obj)) != NULL;
    
    _pmath_atomic_unlock_ptr(&numeric_symbols, table);
    
    return result;
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_assign_isnumeric(pmath_expr_t expr) {
  pmath_t         tag;
  pmath_t         lhs;
  pmath_t         rhs;
  pmath_t         sym;
  int                    assignment;
  pmath_hashtable_t      table;
  void                  *entry;
  
  assignment = _pmath_is_assignment(expr, &tag, &lhs, &rhs);
  if(!assignment)
    return expr;
    
  if(!pmath_is_expr_of_len(lhs, pmath_System_IsNumeric, 1)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  sym = pmath_expr_get_item(lhs, 1);
  
  if( !pmath_same(tag, PMATH_UNDEFINED) &&
      !pmath_same(tag, sym))
  {
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "fnsym", 1, lhs);
    
    pmath_unref(sym);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(pmath_System_DollarFailed);
    return rhs;
  }
  
  if( !pmath_same(rhs, pmath_System_True) &&
      !pmath_same(rhs, pmath_System_False) &&
      !pmath_same(rhs, PMATH_UNDEFINED))
  {
    pmath_unref(sym);
    pmath_message(
      pmath_System_IsNumeric, "set", 2,
      lhs,
      pmath_ref(rhs));
      
    if(assignment > 0)
      return rhs;
      
    pmath_unref(rhs);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(lhs);
  
  table = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&numeric_symbols);
  
  entry = pmath_ht_search(table, PMATH_AS_PTR(sym));
  if(!entry && pmath_same(rhs, pmath_System_True)) {
    entry = pmath_ht_insert(table, PMATH_AS_PTR(sym));
    sym = PMATH_NULL;
  }
  else if(entry && !pmath_same(rhs, pmath_System_True)) {
    entry = pmath_ht_remove(table, PMATH_AS_PTR(sym));
  }
  
  _pmath_atomic_unlock_ptr(&numeric_symbols, table);
  
  pmath_unref(sym);
  pmath_unref(PMATH_FROM_PTR(entry));
  
  if(assignment > 0)
    return rhs;
    
  pmath_unref(rhs);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_isnumeric(pmath_expr_t expr) {
  pmath_bool_t result;
  pmath_t  obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  result = pmath_is_numeric(obj);
  pmath_unref(obj);
  
  return pmath_ref(result ? pmath_System_True : pmath_System_False);
}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_numeric_init(void) {
  pmath_hashtable_t table = pmath_ht_create(&symbol_set_class, 10);
  
  if(!table)
    return FALSE;
    
  pmath_atomic_write_release(&numeric_symbols, (intptr_t)table);
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_numeric_done(void) {
  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&numeric_symbols));
}
