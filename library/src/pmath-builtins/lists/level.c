#include <pmath-core/symbols.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>


PMATH_PRIVATE int _pmath_object_in_levelspec(
  pmath_t obj,
  long levelmin,
  long levelmax,
  long level
) {
  long depth;

  if(levelmin >= 0) {
    if(level < levelmin)
      return -1;

    if(levelmax >= 0) {
      if(level > levelmax)
        return +1;

      return 0;
    }
  }
  else if(levelmax >= 0) {
    if(level > levelmax)
      return +1;
  }

  depth = _pmath_object_depth(obj);

  if(levelmin < 0 && depth > -levelmin)
    return -1;

  if(levelmax < 0 && depth < -levelmax)
    return +1;

  return 0;
}

PMATH_PRIVATE
pmath_bool_t _pmath_extract_levels(
  pmath_t  levelspec,
  long    *levelmin,
  long    *levelmax
) {
  if(pmath_is_int32(levelspec)) {
    *levelmin = *levelmax = PMATH_AS_INT32(levelspec);
    return TRUE;
  }

  if(pmath_is_expr_of_len(levelspec, PMATH_SYMBOL_RANGE, 2)) {
    pmath_t obj = pmath_expr_get_item(levelspec, 1);

    if(pmath_is_int32(obj)) {
      *levelmin = PMATH_AS_INT32(obj);
    }
    else if(pmath_same(obj, PMATH_SYMBOL_AUTOMATIC)) {
      *levelmin = 1;
    }
    else {
      pmath_unref(obj);
      return FALSE;
    }

    pmath_unref(obj);
    obj = pmath_expr_get_item(levelspec, 2);

    if(pmath_is_int32(obj)) {
      *levelmax = PMATH_AS_INT32(obj);
    }
    else if( pmath_same(obj, PMATH_SYMBOL_AUTOMATIC) ||
             pmath_equals(obj, _pmath_object_pos_infinity))
    {
      *levelmax = LONG_MAX;
    }
    else {
      pmath_unref(obj);
      return FALSE;
    }

    pmath_unref(obj);
    return TRUE;
  }

  return FALSE;
}

struct emit_level_info_t {
  long         levelmin;
  long         levelmax;
  pmath_bool_t with_heads;
};

static void emit_level(
  struct emit_level_info_t  *info,
  pmath_t             obj,      // will be freed
  long                       level
) {
  int reldepth = _pmath_object_in_levelspec(
                   obj, info->levelmin, info->levelmax, level);

  if(reldepth <= 0 && pmath_is_expr(obj)) {
    size_t len = pmath_expr_length(obj);
    size_t i;

    for(i = info->with_heads ? 0 : 1; i <= len; ++i) {
      emit_level(
        info,
        pmath_expr_get_item(obj, i),
        level + 1);
    }
  }

  if(reldepth == 0)
    pmath_emit(obj, PMATH_NULL);
  else
    pmath_unref(obj);
}

PMATH_PRIVATE pmath_t builtin_level(pmath_expr_t expr) {
  /* Level(expr, levelspec, f)
     Level(expr, levelspec)    = Level(expr, levelspec, List)

     options:
       Heads->False

     messages:
       General::level
       General::opttf
   */
  struct emit_level_info_t info;
  pmath_expr_t options;
  pmath_t obj, head;
  size_t last_nonoption;
  size_t len = pmath_expr_length(expr);

  if(len < 2) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }

  info.with_heads = FALSE;
  obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_extract_levels(obj, &info.levelmin, &info.levelmax)) {
    pmath_message(PMATH_NULL, "level", 1, obj);
    return expr;
  }
  pmath_unref(obj);

  last_nonoption = 2;
  if(len >= 3) {
    head = pmath_expr_get_item(expr, 3);

    if(pmath_is_set_of_options(head)) {
      pmath_unref(head);
      head = pmath_ref(PMATH_SYMBOL_LIST);
    }
    else
      last_nonoption = 3;
  }
  else
    head = pmath_ref(PMATH_SYMBOL_LIST);

  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(head);
    return expr;
  }

  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_HEADS, options));
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    info.with_heads = TRUE;
  }
  else if(!pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_unref(options);
    pmath_unref(head);
    pmath_message(
      PMATH_NULL, "opttf", 2,
      pmath_ref(PMATH_SYMBOL_HEADS),
      obj);
    return expr;
  }
  pmath_unref(obj);
  pmath_unref(options);

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  pmath_gather_begin(PMATH_NULL);
  emit_level(&info, obj, 0);
  return pmath_expr_set_item(pmath_gather_end(), 0, head);
}
