#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_Heads;
extern pmath_symbol_t pmath_System_True;

struct contains_info_t {
  pmath_bool_t with_heads; // currently not set (allways FALSE)
  long levelmin;
  long levelmax;
};

static pmath_bool_t contains( // return = search more?
  struct contains_info_t *info,
  pmath_t                 obj,     // will be freed
  pmath_t                 pattern, // wont be freed
  long                    level
) {
  size_t i;
  int reldepth;

  reldepth = _pmath_object_in_levelspec(
               obj, info->levelmin, info->levelmax, level);

  if(reldepth > 0) {
    pmath_unref(obj);
    return TRUE;
  }

  if(pmath_is_expr(obj)) {
    size_t len = pmath_expr_length(obj);

    for(i = info->with_heads ? 0 : 1; i <= len; i++) {
      if(contains(
            info,
            pmath_expr_get_item(obj, i),
            pattern,
            level + 1)
        ) {
        pmath_unref(obj);
        return TRUE;
      }
    }
  }

  if( reldepth == 0 && _pmath_pattern_match(obj, pmath_ref(pattern), NULL)) {
    pmath_unref(obj);
    return TRUE;
  }

  pmath_unref(obj);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_isfreeof(pmath_expr_t expr) {
  /* IsFreeOf(obj, pattern, levelspec)
     IsFreeOf(obj, pattern)             = IsFreeOf(obj, pattern, {0, Infinity})

     options:
       Heads->True

     messages:
       General::innf
       General::level
   */
  struct contains_info_t info;
  size_t last_nonoption, len = pmath_expr_length(expr);
  pmath_expr_t options;
  pmath_t obj, pattern;

  if(len < 2) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }

  last_nonoption = 2;
  info.with_heads = TRUE;
  info.levelmin = 0;
  info.levelmax = LONG_MAX;
  if(len > 2) {
    pmath_t levels = pmath_expr_get_item(expr, 3);

    if(_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      last_nonoption = 3;
    }
    else if(!pmath_is_set_of_options(levels)) {
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }

    pmath_unref(levels);
  }

  pattern = pmath_expr_get_item(expr, 2);
  if(!_pmath_pattern_validate(pattern)) {
    pmath_unref(pattern);
    return expr; 
  }

  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(pattern);
    return expr;
  }

  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_Heads, options));
  if(pmath_same(obj, pmath_System_True)) {
    info.with_heads = TRUE;
  }
  else if(!pmath_same(obj, pmath_System_False)) {
    pmath_unref(pattern);
    pmath_unref(options);
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(pmath_System_Heads),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(contains(&info, obj, pattern, 0)) {
    pmath_unref(pattern);
    return pmath_ref(pmath_System_False);
  }

  pmath_unref(pattern);
  return pmath_ref(pmath_System_True);
}
