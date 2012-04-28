#ifndef __GUI__WIN32__WIN32_COLORDIALOG_H__
#define __GUI__WIN32__WIN32_COLORDIALOG_H__


#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class Win32ColorDialog: public Base {
    public:
      static pmath::Expr show(int intialcolor = -1); // 0xrrggbb
  };
}


#endif // __GUI__WIN32__WIN32_COLORDIALOG_H__
