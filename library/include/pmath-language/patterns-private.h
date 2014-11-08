#ifndef __PMATH_LANGUAGE__PATTERNS_PRIVATE_H__
#define __PMATH_LANGUAGE__PATTERNS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-util/hashtables.h>


#define PMATH_MAGIC_PATTERN_SEQUENCE  PMATH_FROM_TAG(PMATH_TAG_MAGIC, 2)


// initialized in pmath_init():
extern PMATH_PRIVATE pmath_t _pmath_object_range_from_one; /* Range(1,()) readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_range_from_zero; /* Range(0,()) readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_singlematch; /* SingleMatch() readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_multimatch; /* Repeated(SingleMatch(),1..) readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_zeromultimatch; /* Repeated(SingleMatch(),0..) readonly */
extern PMATH_PRIVATE pmath_t _pmath_object_empty_pattern_sequence; /* MPS()   where MPS = PMATH_MAGIC_PATTERN_SEQUENCE */

PMATH_PRIVATE
int _pmath_pattern_compare(
  pmath_t pat1,  // wont be freed
  pmath_t pat2); // wont be freed

PMATH_PRIVATE
pmath_bool_t _pmath_rhs_has_condition(
  pmath_t      *rhs,
  pmath_bool_t  adjust);

PMATH_PRIVATE
pmath_bool_t _pmath_pattern_is_const(pmath_t pattern);

typedef struct {
  pmath_t parent_pat_head; // wont be freed
  pmath_t pat;             // wont be freed
  
  /* pattern_variables is an optimization to possibly limit the matchable size 
   */
  pmath_hashtable_t pattern_variables; // should be NULL or a ht_pattern_variable_class (private)
  
  uint8_t associative;
} _pmath_pattern_analyse_input_t;

typedef struct {
  size_t min;
  size_t max;
} _pmath_pattern_size_t;

typedef struct {
  unsigned no_sequence: 1; // can only be TRUE if input.associative is TRUE
  unsigned longest: 1;
  unsigned prefer_nonempty: 1;
} _pmath_pattern_options_t;

typedef struct {
  _pmath_pattern_size_t    size;
  _pmath_pattern_options_t options;
//  size_t min;
//  size_t max;
//  
//  uint8_t no_sequence; // can only be TRUE if input.associative is TRUE
//  uint8_t longest;
//  uint8_t prefer_nonempty;
} _pmath_pattern_analyse_output_t;

PMATH_PRIVATE
void _pmath_pattern_analyse(
  _pmath_pattern_analyse_input_t  *input,
  _pmath_pattern_analyse_output_t *output);

PMATH_PRIVATE
pmath_bool_t _pmath_pattern_match(
  pmath_t  obj,      // wont be freed
  pmath_t  pattern,  // will be freed
  pmath_t *rhs);     // [optional] in/out (right hand side of assign, Rule, ...)

PMATH_PRIVATE pmath_bool_t _pmath_contains_any(
  pmath_t           object,         // wont be freed
  pmath_hashtable_t replacements);  // entries are pmath_ht_obj_[int_]entry_t*

// retains debug-info
PMATH_PRIVATE pmath_expr_t _pmath_preprocess_local(
  pmath_expr_t local_expr); // will be freed.

// retains debug-info
PMATH_PRIVATE pmath_t _pmath_replace_local(
  pmath_t  object, // will be freed
  pmath_t  name,
  pmath_t  value);

#endif /* __PMATH_LANGUAGE__PATTERNS_PRIVATE_H__ */
