#ifndef RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  using namespace pmath; // bad style!!!
  
  class MathFltkFileDialog: public Base {
      class Impl;
    public:
      MathFltkFileDialog(bool to_save);
      ~MathFltkFileDialog();
      
      void set_title(String title) { _title = title; }
      void set_filter(Expr filter) { _filter = filter; }
      void set_initial_file(String initialfile) { _initialfile = initialfile; }
      
      Expr show_dialog();
      
    private:
      String _title;
      Expr _filter;
      String _initialfile;
      bool _to_save;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_FILEDIALOG_H__INCLUDED
