#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyAiry;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyAiry

static void acb_airy_ai(acb_t r, const acb_t z, slong prec) {
  acb_hypgeom_airy(r, NULL, NULL, NULL, z, prec);
}

static void acb_airy_ai_prime(acb_t r, const acb_t z, slong prec) {
  acb_hypgeom_airy(NULL, r, NULL, NULL, z, prec);
}

static void acb_airy_bi(acb_t r, const acb_t z, slong prec) {
  acb_hypgeom_airy(NULL, NULL, r, NULL, z, prec);
}

static void acb_airy_bi_prime(acb_t r, const acb_t z, slong prec) {
  acb_hypgeom_airy(NULL, NULL, NULL, r, z, prec);
}

PMATH_PRIVATE pmath_t eval_System_AiryAi(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_airy_ai
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_AiryAiPrime(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_airy_ai_prime
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_AiryBi(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_airy_bi
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_AiryBiPrime(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_airy_bi_prime
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

