#ifndef RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class MathFltkFileDialog: public Base {
      class Impl;
    public:
      MathFltkFileDialog(bool to_save);
      ~MathFltkFileDialog();
      
      void set_title(pmath::String title);
      void set_filter(pmath::Expr filter);
      void set_initial_file(pmath::String initialfile);
      
      pmath::Expr show_dialog();
      
    private:
      bool _to_save;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED
