#ifndef __PJ_SYMBOLS_H__
#define __PJ_SYMBOLS_H__

#include <pmath.h>


#define PJ_SYMBOLS_COUNT   24
extern pmath_symbol_t _pj_symbols[PJ_SYMBOLS_COUNT];

#define PJ_SYMBOL_JAVA               (_pj_symbols[0])
#define PJ_SYMBOL_JAVASTARTVM        (_pj_symbols[1])
#define PJ_SYMBOL_JAVAKILLVM         (_pj_symbols[2])
#define PJ_SYMBOL_JAVACLASS          (_pj_symbols[3])
#define PJ_SYMBOL_JAVAVMLIBRARYNAME  (_pj_symbols[4])
#define PJ_SYMBOL_TYPE_BOOLEAN       (_pj_symbols[5])
#define PJ_SYMBOL_TYPE_BYTE          (_pj_symbols[6])
#define PJ_SYMBOL_TYPE_CHAR          (_pj_symbols[7])
#define PJ_SYMBOL_TYPE_SHORT         (_pj_symbols[8])
#define PJ_SYMBOL_TYPE_INT           (_pj_symbols[9])
#define PJ_SYMBOL_TYPE_LONG          (_pj_symbols[10])
#define PJ_SYMBOL_TYPE_FLOAT         (_pj_symbols[11])
#define PJ_SYMBOL_TYPE_DOUBLE        (_pj_symbols[12])
#define PJ_SYMBOL_TYPE_ARRAY         (_pj_symbols[13])
#define PJ_SYMBOL_JAVAEXCEPTION      (_pj_symbols[14])
#define PJ_SYMBOL_JAVANEW            (_pj_symbols[15])
#define PJ_SYMBOL_JAVACALL           (_pj_symbols[16])
#define PJ_SYMBOL_JAVAFIELD          (_pj_symbols[17])
#define PJ_SYMBOL_CLASSNAME          (_pj_symbols[18])
#define PJ_SYMBOL_GETCLASS           (_pj_symbols[19])
#define PJ_SYMBOL_PARENTCLASS        (_pj_symbols[20])
#define PJ_SYMBOL_INSTANCEOF         (_pj_symbols[21])
#define PJ_SYMBOL_ISJAVAOBJECT       (_pj_symbols[22])
#define PJ_SYMBOL_DEFAULTCLASSPATH   (_pj_symbols[23])


extern pmath_bool_t pj_symbols_init(void);
extern void         pj_symbols_done(void);


#endif // __PJ_SYMBOLS_H__
