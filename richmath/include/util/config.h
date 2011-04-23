#ifndef __UTIL__CONFIG_H__
#define __UTIL__CONFIG_H__


//#define  RICHMATH_USE_WIN32_FONT
//#define  RICHMATH_USE_FT_FONT

#if defined( RICHMATH_USE_WIN32_FONT ) + defined( RICHMATH_USE_FT_FONT ) != 1
  #error either RICHMATH_USE_WIN32_FONT or RICHMATH_USE_FT_FONT must be defined
#endif


//#define  RICHMATH_USE_WIN32_GUI
//#define  RICHMATH_USE_GTK_GUI    // not yet supported
//#define  RICHMATH_USE_NO_GUI     // not yet supported

#if defined( RICHMATH_USE_WIN32_GUI ) != 1
  #error RICHMATH_USE_WIN32_GUI must be defined
#endif


#endif // __UTIL__CONFIG_H__
