#ifndef __GUI__WIN32__WIN32_FILEDIALOG_H__
#define __GUI__WIN32__WIN32_FILEDIALOG_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class Win32FileDialog: public Base {
    public:
      static pmath::Expr show(
        bool           save,
        pmath::String  initialfile,
        pmath::Expr    filter,
        pmath::String  title);
  };
}


#endif // __GUI__WIN32__WIN32_FILEDIALOG_H__
