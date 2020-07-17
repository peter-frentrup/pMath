#ifndef __PJ_THREADS_H__
#define __PJ_THREADS_H__

#include <pmath.h>
#include <jvmti.h>

PMATH_PRIVATE void pj_thread_message(
  pmath_messages_t mq, // wont be freed
  pmath_symbol_t sym,  // wont be freed
  const char *tag,
  size_t argcount,
  ...);                // pmath_t[argcount]   will all be freed

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_messages_t pj_thread_get_companion(jthread *out_jthread);

PMATH_PRIVATE pmath_bool_t pj_threads_init(void);
PMATH_PRIVATE void         pj_threads_done(void);

#endif // __PJ_THREADS_H__
