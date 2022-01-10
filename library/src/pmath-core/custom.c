#include <pmath-core/custom-private.h>
#include <pmath-core/custom.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/memory.h>

#include <stdio.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


struct _pmath_custom_t {
  struct _pmath_t    inherited;
  void              *data;
  pmath_callback_t   destructor;
  pmath_t            attached_object;
};

PMATH_API pmath_custom_t pmath_custom_new(
  void              *data,
  pmath_callback_t   destructor
) {
  return pmath_custom_new_with_object(data, destructor, PMATH_NULL);
}

PMATH_API pmath_custom_t pmath_custom_new_with_object(
  void              *data,
  pmath_callback_t   destructor,
  pmath_t            obj
) {
  struct _pmath_custom_t *custom;

  if(PMATH_UNLIKELY(!destructor)) {
    pmath_unref(obj);
    return PMATH_NULL;
  }

  custom = (struct _pmath_custom_t *)PMATH_AS_PTR(_pmath_create_stub(
             PMATH_TYPE_SHIFT_CUSTOM,
             sizeof(struct _pmath_custom_t)));

  if(PMATH_UNLIKELY(!custom)) {
    destructor(data);
    pmath_unref(obj);
    return PMATH_NULL;
  }

  custom->data = data;
  custom->destructor = destructor;
  custom->attached_object = obj;
  return PMATH_FROM_PTR(custom);
}

PMATH_API void *pmath_custom_get_data(pmath_custom_t custom) {
  if(PMATH_UNLIKELY(pmath_is_null(custom)))
    return NULL;

  assert(pmath_is_custom(custom));

  return ((struct _pmath_custom_t *)PMATH_AS_PTR(custom))->data;
}

PMATH_API pmath_t pmath_custom_get_attached_object(pmath_custom_t custom) {
  if(PMATH_UNLIKELY(pmath_is_null(custom)))
    return PMATH_NULL;

  assert(pmath_is_custom(custom));

  return pmath_ref(((struct _pmath_custom_t *)PMATH_AS_PTR(custom))->attached_object);
}

PMATH_API pmath_bool_t pmath_custom_has_destructor(
  pmath_custom_t    custom,
  pmath_callback_t  dtor
) {
  if(PMATH_UNLIKELY(pmath_is_null(custom)))
    return FALSE;

  assert(pmath_is_custom(custom));

  return ((struct _pmath_custom_t *)PMATH_AS_PTR(custom))->destructor == dtor;
}

//{ pMath object functions ...

static int compare_custom(pmath_t a, pmath_t b) {
  return (uintptr_t)PMATH_AS_PTR(a) < (uintptr_t)PMATH_AS_PTR(b) ? -1 : 1;
}

static unsigned int hash_custom(pmath_t a) {
  return _pmath_hash_pointer(PMATH_AS_PTR(a));
}

static void destroy_custom(pmath_t a) {
  struct _pmath_custom_t *custom = (struct _pmath_custom_t *)PMATH_AS_PTR(a);

  PMATH_OBJECT_MARK_DELETION_TRAP(&custom->inherited);
  
  custom->destructor(custom->data);
  pmath_unref(custom->attached_object);
  pmath_mem_free(custom);
}

static void write_custom(struct pmath_write_ex_t *info, pmath_t a) {
  struct _pmath_custom_t *custom = (void *)PMATH_AS_PTR(a);
  char s[100];

  snprintf(s, sizeof(s), "(/\\/ /* custom 16^^%"PRIxPTR" 16^^%"PRIxPTR" */)",
           (uintptr_t)custom->destructor,
           (uintptr_t)custom->data);

  _pmath_write_cstr(s, info->write, info->user);
}

//} ... pMath object functions

//{ module handling functions ...

PMATH_PRIVATE pmath_bool_t _pmath_custom_objects_init(void) {
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_CUSTOM,
    compare_custom,
    hash_custom,
    destroy_custom,
    NULL,
    write_custom);

  return TRUE;
}

PMATH_PRIVATE void _pmath_custom_objects_done(void) {
}

//} ... module handling functions
