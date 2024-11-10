#include <pmath-language/source-location.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/symbols.h>

#include <pmath-util/helpers.h>


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


static const struct _pmath_custom_expr_api_t source_location_expr_api = {
  .destroy_data        = source_location_destroy_data,
  .get_length          = source_location_get_length,
  .get_item            = source_location_get_item,
  .get_extra_bytecount = source_location_get_extra_bytecount,
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
