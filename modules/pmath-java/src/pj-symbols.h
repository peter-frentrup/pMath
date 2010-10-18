#ifndef __PJ_SYMBOLS_H__
#define __PJ_SYMBOLS_H__

#include <pmath.h>


#define PJ_SYMBOLS_COUNT   5
extern pmath_symbol_t _pj_symbols[PJ_SYMBOLS_COUNT];

#define PJ_SYMBOL_JAVA          (_pj_symbols[0])
#define PJ_SYMBOL_JAVASTARTVM   (_pj_symbols[1])
#define PJ_SYMBOL_JAVAKILLVM    (_pj_symbols[2])
#define PJ_SYMBOL_JAVACLASS     (_pj_symbols[3])
#define PJ_SYMBOL_JAVAOBJECT    (_pj_symbols[4])


extern pmath_bool_t pj_symbols_init(void);
extern void         pj_symbols_done(void);


#endif // __PJ_SYMBOLS_H__
