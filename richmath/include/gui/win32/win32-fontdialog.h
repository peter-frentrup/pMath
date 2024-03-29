#ifndef RICHMATH__GUI__WIN32__WIN32_FONTDIALOG_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_FONTDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/style.h>


namespace richmath {
  class Win32FontDialog {
    public:
      Win32FontDialog() = delete;

      static pmath::Expr show(Style initial_style = nullptr);
  };
}


#endif // RICHMATH__GUI__WIN32__WIN32_FONTDIALOG_H__INCLUDED
