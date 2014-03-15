#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <pmath.h>

extern pmath_threadlock_t read_lock;

pmath_string_t readline_pmath(
  const char     *prompt, 
  pmath_bool_t    with_completion,
  pmath_string_t  previous_input); 

void cleanup_input_cache(void);


#endif // __CONSOLE_H__
