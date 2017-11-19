#include "stdafx.h"

struct hypgeom_type_t {
  size_t p;
  size_t q;
  pmath_bool_t regularized;
};

extern pmath_symbol_t pmath_System_Private_AutoSimplifyHypGeom;

static void do_hypgeom_pfq(acb_t result, acb_ptr args, size_t nargs, slong prec, void *context) {
  struct hypgeom_type_t *type = context;
  
  acb_indeterminate(result);
  
  if(type->p + type->q + 1 != nargs)
    return;
    
  acb_hypgeom_pfq(
    result,
    args, (slong)type->p,
    args + type->p, (slong)type->q,
    args + type->p + type->q,
    0,
    prec);
}

static pmath_t do_special_hypgeom_pfq(pmath_t expr, size_t p, size_t q, pmath_bool_t regularized) {
  /* Hypergeometric0F1(b, z)
     Hypergeometric1F1(a, b, z)
     Hypergeometric2F1(a1, a2, b, z)
     Hypergeometric0F1Regularized(b, z)
     Hypergeometric1F1Regularized(a, b, z)
     Hypergeometric2F1Regularized(a1, a2, b, z)
   */
  
  struct hypgeom_type_t type;
  size_t exprlen = pmath_expr_length(expr);
  pmath_expr_t a_list;
  pmath_expr_t b_list;
  pmath_expr_t z;
  
  if(exprlen != p + q + 1) {
    pmath_message_argxxx(exprlen, p + q + 1, p + q + 1);
    return expr;
  }
  
  type.p = p;
  type.q = q;
  type.regularized = regularized;
  
  if(pmath_complex_try_evaluate_acb_ex(&expr, expr, do_hypgeom_pfq, &type))
    return expr;
    
  a_list = pmath_expr_get_item_range(expr, 1, p);
  a_list = pmath_expr_set_item(a_list, 0, pmath_ref(PMATH_SYMBOL_LIST));
  
  b_list = pmath_expr_get_item_range(expr, p + 1, q);
  b_list = pmath_expr_set_item(b_list, 0, pmath_ref(PMATH_SYMBOL_LIST));
  
  z = pmath_expr_get_item(expr, p + q + 1);
  
  z = pmath_expr_new_extended(
        pmath_ref(pmath_System_Private_AutoSimplifyHypGeom), 4,
        pmath_expr_get_item(expr, 0),
        a_list,
        b_list,
        z);
        
  z = pmath_evaluate(z);
  if(!pmath_same(z, PMATH_SYMBOL_FAILED)) {
    pmath_unref(expr);
    return z;
  }
  
  pmath_unref(z);
  return expr;
}

static pmath_t do_general_hypgeom_pfq_scalar(
  pmath_t expr,        // will be freed
  pmath_expr_t a_list, // won't be freed
  pmath_expr_t b_list, // won't be freed
  pmath_t z,           // will be freed
  pmath_bool_t regularized
) {
  struct hypgeom_type_t type;
  pmath_t args;
  size_t i;
  
  if(!pmath_is_expr_of(a_list, PMATH_SYMBOL_LIST) || !pmath_is_expr_of(b_list, PMATH_SYMBOL_LIST)) {
    pmath_unref(z);
    return expr;
  }
  
  type.p = pmath_expr_length(a_list);
  type.q = pmath_expr_length(b_list);
  type.regularized = regularized;
  args = pmath_expr_new(PMATH_NULL, type.p + type.q + 1);
  for(i = 1; i <= type.p; ++i)
    args = pmath_expr_set_item(args, i, pmath_expr_get_item(a_list, i));
  for(i = 1; i <= type.q; ++i)
    args = pmath_expr_set_item(args, type.p + i, pmath_expr_get_item(b_list, i));
  args = pmath_expr_set_item(args, type.p + type.q + 1, pmath_ref(z));
  
  if(pmath_complex_try_evaluate_acb_ex(&expr, args, do_hypgeom_pfq, &type)) {
    pmath_unref(args);
    pmath_unref(z);
    return expr;
  }
  
  pmath_unref(args);
  z = pmath_expr_new_extended(
           pmath_ref(pmath_System_Private_AutoSimplifyHypGeom), 4,
           pmath_expr_get_item(expr, 0),
           pmath_ref(a_list),
           pmath_ref(b_list),
           z);
  
  z = pmath_evaluate(z);
  if(!pmath_same(z, PMATH_SYMBOL_FAILED)) {
    pmath_unref(expr);
    return z;
  }
  
  pmath_unref(z);
  return expr;
}

static pmath_t do_general_hypgeom_pfq(pmath_t expr, pmath_bool_t regularized) {
  /* HypergeometricPFQ({a1, a2, ...}, {b1, b2, ...}, z)
     HypergeometricPFQRegularized({a1, a2, ...}, {b1, b2, ...}, z)
   */
  
  size_t exprlen = pmath_expr_length(expr);
  pmath_expr_t a_list;
  pmath_expr_t b_list;
  pmath_t z;
  
  if(exprlen != 3) {
    pmath_message_argxxx(exprlen, 3, 3);
    return expr;
  }
  
  a_list = pmath_expr_get_item(expr, 1);
  b_list = pmath_expr_get_item(expr, 2);
  z = pmath_expr_get_item(expr, 3);
  
  if(pmath_is_expr_of(z, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = pmath_expr_length(z); i > 0; --i) {
      pmath_t zi = pmath_expr_get_item(z, i);
      pmath_t expri = pmath_expr_set_item(pmath_ref(expr), 3, zi);
      zi = do_general_hypgeom_pfq_scalar(expri, a_list, b_list, zi, regularized);
      zi = pmath_evaluate(zi);
      z = pmath_expr_set_item(z, i, zi);
      
      if((1 % 64 == 0) && pmath_aborting())
        break;
    }
    pmath_unref(expr);
    pmath_unref(a_list);
    pmath_unref(b_list);
    return z;
  }
  
  expr = do_general_hypgeom_pfq_scalar(expr, a_list, b_list, z, regularized);
  pmath_unref(a_list);
  pmath_unref(b_list);
  return expr;
}

PMATH_PRIVATE pmath_t eval_System_Hypergeometric0F1(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 0, 1, FALSE);
}

PMATH_PRIVATE pmath_t eval_System_Hypergeometric0F1Regularized(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 0, 1, TRUE);
}

PMATH_PRIVATE pmath_t eval_System_Hypergeometric1F1(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 1, 1, FALSE);
}

PMATH_PRIVATE pmath_t eval_System_Hypergeometric1F1Regularized(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 1, 1, TRUE);
}

/* TODO: implement N(Hypergeometric2F1(a1,a2,b,x)) by using  acb_hypgeom_2f1 with the 
   ACB_HYPGEOM_2F1_ABC etc. flags as appropriate to avoid catastrophic cancellation,
   e.e. in N(Hypergeometric2F1(Sqrt(2), 1/2, Sqrt(2) + 3/2, 9/10))
 */
PMATH_PRIVATE pmath_t eval_System_Hypergeometric2F1(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 2, 1, FALSE);
}

PMATH_PRIVATE pmath_t eval_System_Hypergeometric2F1Regularized(pmath_expr_t expr) {
  return do_special_hypgeom_pfq(expr, 2, 1, TRUE);
}

PMATH_PRIVATE pmath_t eval_System_HypergeometricPFQ(pmath_expr_t expr) {
  return do_general_hypgeom_pfq(expr, FALSE);
}

PMATH_PRIVATE pmath_t eval_System_HypergeometricPFQRegularized(pmath_expr_t expr) {
  return do_general_hypgeom_pfq(expr, TRUE);
}
