#include <pmath-language/source-location.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <pmath-util/helpers.h>


extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_Language_SourceLocation;


PMATH_API
pmath_t pmath_language_new_file_location(pmath_t ref, int startline, int startcol, int endline, int endcol) {
  return pmath_expr_new_extended(
           pmath_ref(pmath_Language_SourceLocation), 2,
           ref,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_Range), 2,
             pmath_build_value("(ii)", startline, startcol),
             pmath_build_value("(ii)", endline,   endcol)));
}

PMATH_API
pmath_t pmath_language_new_simple_location(pmath_t ref, int start, int end) {
  return pmath_expr_new_extended(
           pmath_ref(pmath_Language_SourceLocation), 2,
           ref,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_Range), 2,
             PMATH_FROM_INT32(start),
             PMATH_FROM_INT32(end)));
}

