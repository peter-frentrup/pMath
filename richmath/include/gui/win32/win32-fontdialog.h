#ifndef __GUI__WIN32__WIN32_FONTDIALOG_H__
#define __GUI__WIN32__WIN32_FONTDIALOG_H__


#include <util/style.h>


namespace richmath {
  class Win32FontDialog: public Base {
    public:
      static pmath::Expr show(SharedPtr<Style> initial_style = 0);
  };
}


#endif // __GUI__WIN32__WIN32_FONTDIALOG_H__
