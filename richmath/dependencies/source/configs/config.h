#ifndef RICHMATH__DEPENDENCIES__CONFIGS__CONFIG_H__INCLUDED
#define RICHMATH__DEPENDENCIES__CONFIGS__CONFIG_H__INCLUDED


#if defined(RICHMATH_USE_FLTK_GUI)
// If FLTK is used, it is built in-source and then it searches for an internal <config.h>


// FLTK_HAVE_CAIRO=1 already set by built options


#define U16 uint16_t
#define U32 uint32_t

#if PMATH_BYTE_ORDER > 0
#  define WORDS_BIGENDIAN 1
#else
#  define WORDS_BIGENDIAN 0
#endif // PMATH_BYTE_ORDER


// Thickness of FL_UP_BOX and FL_DOWN_BOX.  Current 1,2, and 3 are supported.
#define BORDER_WIDTH 2


#endif // defined(RICHMATH_USE_FLTK_GUI)


#endif // RICHMATH__DEPENDENCIES__CONFIGS__CONFIG_H__INCLUDED
