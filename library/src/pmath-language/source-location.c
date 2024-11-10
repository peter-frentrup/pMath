#include <pmath-language/source-location.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/symbols.h>

#include <pmath-util/helpers.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_Language_SourceLocation;


struct _pmath_source_location_extra_data_t {
  struct _pmath_custom_expr_data_t base;
  
  int start;
  int end;
  int startcol;
  int endcol;
};

#define SRCLOC_EXPR_EXTRA(EXPR_PTR)      ((struct _pmath_source_location_extra_data_t*)PMATH_CUSTOM_EXPR_DATA(EXPR_PTR))


static void         source_location_destroy_data(       struct _pmath_custom_expr_data_t *_data);
static size_t       source_location_get_length(         struct _pmath_custom_expr_t *e);
static pmath_t      source_location_get_item(           struct _pmath_custom_expr_t *e, size_t i);
static size_t       source_location_get_extra_bytecount(struct _pmath_custom_expr_t *e);
static pmath_bool_t source_location_try_item_equals(    struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result); // does not free e or expected_item
static pmath_bool_t source_location_try_set_item_copy(  struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item, pmath_expr_t *result);      // does not free e, but frees new_item (only if returning TRUE)


static const struct _pmath_custom_expr_api_t source_location_expr_api = {
  .destroy_data        = source_location_destroy_data,
  .get_length          = source_location_get_length,
  .get_item            = source_location_get_item,
  .get_extra_bytecount = source_location_get_extra_bytecount,
  .try_item_equals     = source_location_try_item_equals,
  .try_set_item_copy   = source_location_try_set_item_copy,
};

PMATH_API
pmath_t pmath_language_new_file_location(pmath_t ref, int startline, int startcol, int endline, int endcol) {
  struct _pmath_custom_expr_t *result;
  
  result = _pmath_expr_new_custom(0, &source_location_expr_api, sizeof(struct _pmath_source_location_extra_data_t));
  if(!result) {
    pmath_unref(ref);
    return PMATH_NULL;
  }
  
  result->internals.items[0] = ref;
  
  struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(result);
  data->start    = startline;
  data->end      = endline;
  data->startcol = startcol;
  data->endcol   = endcol;

  return PMATH_FROM_PTR(result);
}

PMATH_API
pmath_t pmath_language_new_simple_location(pmath_t ref, int start, int end) {
  struct _pmath_custom_expr_t *result;
  
  result = _pmath_expr_new_custom(0, &source_location_expr_api, sizeof(struct _pmath_source_location_extra_data_t));
  if(!result) {
    pmath_unref(ref);
    return PMATH_NULL;
  }
  
  result->internals.items[0] = ref;
  
  struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(result);
  data->start    = start;
  data->end      = end;
  data->startcol = -1;
  data->endcol   = -1;

  return PMATH_FROM_PTR(result);
}

static void source_location_destroy_data(struct _pmath_custom_expr_data_t *_data) {
  //struct _pmath_source_location_extra_data_t *data = (void*)_data;
}

static size_t source_location_get_length(struct _pmath_custom_expr_t *e) {
  //struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(e);
  
  return 2;
}

static pmath_t source_location_get_item(struct _pmath_custom_expr_t *e, size_t i) {
  struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(e);
  
  if(i == 0)
    return pmath_ref(pmath_Language_SourceLocation);
  
  if(i == 1)
    return pmath_ref(e->internals.items[0]);
  
  if(i > 2)
    return PMATH_NULL;
  
  if(data->startcol == -1 && data->endcol == -1) {
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_Range), 2, 
             PMATH_FROM_INT32(data->start),
             PMATH_FROM_INT32(data->end));
  }
  else {
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_Range), 2,
             pmath_build_value("(ii)", data->start, data->startcol),
             pmath_build_value("(ii)", data->end,   data->endcol));
  }
}

static size_t source_location_get_extra_bytecount(struct _pmath_custom_expr_t *e) {
  //const struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(e);
  
  return sizeof(struct _pmath_source_location_extra_data_t);
}

static pmath_bool_t source_location_try_item_equals(struct _pmath_custom_expr_t *e, size_t i, pmath_t expected_item, pmath_bool_t *result) {
  struct _pmath_source_location_extra_data_t *data = SRCLOC_EXPR_EXTRA(e);

  if(i > 2) {
    *result = pmath_is_null(expected_item);
    return TRUE;
  }
  
  if(i == 0) {
    *result = pmath_same(expected_item, pmath_Language_SourceLocation);
    return TRUE;
  }
  
  if(i == 1) {
    *result = pmath_equals(e->internals.items[0], expected_item);
    return TRUE;
  }
  
  if(i == 2) {
    if(!pmath_is_expr_of_len(expected_item, pmath_System_Range, 2)) {
      *result = FALSE;
      return TRUE;
    }
    
    if(data->startcol == -1 && data->endcol == -1) {
      *result  = pmath_expr_item_equals(expected_item, 1, PMATH_FROM_INT32(data->start))
              && pmath_expr_item_equals(expected_item, 2, PMATH_FROM_INT32(data->end));
      return TRUE;
    }
    else {
      pmath_t range_start = pmath_expr_get_item(expected_item, 1);
      pmath_t range_end   = pmath_expr_get_item(expected_item, 2);
      
      *result  = pmath_is_expr_of_len(range_start, pmath_System_List, 2)
              && pmath_expr_item_equals(range_start, 1, PMATH_FROM_INT32(data->start))
              && pmath_expr_item_equals(range_start, 2, PMATH_FROM_INT32(data->startcol))
              && pmath_is_expr_of_len(range_end, pmath_System_List, 2)
              && pmath_expr_item_equals(range_end, 1, PMATH_FROM_INT32(data->end))
              && pmath_expr_item_equals(range_end, 2, PMATH_FROM_INT32(data->endcol));
      
      pmath_unref(range_start);
      pmath_unref(range_end);
      return TRUE;
    }
  }

  return FALSE;
}

static pmath_bool_t source_location_try_set_item_copy(struct _pmath_custom_expr_t *e, size_t i, pmath_t new_item, pmath_expr_t *result) {
  pmath_bool_t same = FALSE;
  if(source_location_try_item_equals(e, i, new_item, &same) && same) {
    pmath_unref(new_item);
    *result = pmath_ref(PMATH_FROM_PTR(e));
    return TRUE;
  }
  
  return FALSE;
}
