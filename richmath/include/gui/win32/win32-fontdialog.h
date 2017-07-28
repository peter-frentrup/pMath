#ifndef __GUI__WIN32__WIN32_FONTDIALOG_H__
#define __GUI__WIN32__WIN32_FONTDIALOG_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/style.h>


namespace richmath {
  class Win32FontDialog {
    public:
      Win32FontDialog() = delete;

      static pmath::Expr show(SharedPtr<Style> initial_style = nullptr);
  };
}


#endif // __GUI__WIN32__WIN32_FONTDIALOG_H__
