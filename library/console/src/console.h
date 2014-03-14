#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <pmath.h>

extern pmath_threadlock_t read_lock;

pmath_bool_t search_editline(void);
void cleanup_input(void);

pmath_string_t readline_pmath(const char *prompt, pmath_bool_t  with_completion);


#endif // __CONSOLE_H__
