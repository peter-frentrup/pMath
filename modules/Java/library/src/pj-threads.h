#ifndef __PJ_THREADS_H__
#define __PJ_THREADS_H__

#include <pmath.h>
#include <jvmti.h>

extern void pj_thread_message(
  pmath_messages_t mq, // wont be freed
  pmath_symbol_t sym,  // wont be freed
  const char *tag,
  size_t argcount,
  ...);                // pmath_t[argcount]   will all be freed

extern pmath_messages_t pj_thread_get_companion(jthread *out_jthread);

extern pmath_t pj_builtin_internal_stoppedcothread(pmath_expr_t expr);

extern pmath_bool_t pj_threads_init(void);
extern void         pj_threads_done(void);

#endif // __PJ_THREADS_H__
