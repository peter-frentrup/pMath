#ifndef RICHMATH__GUI__WIN32__WIN32_COLORDIALOG_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_COLORDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class Win32ColorDialog {
    public:
      Win32ColorDialog() = delete;

      static pmath::Expr show(int initialcolor = -1); // 0xrrggbb
  };
}


#endif // RICHMATH__GUI__WIN32__WIN32_COLORDIALOG_H__INCLUDED
