#ifndef RICHMATH__GUI__WIN32__WIN32_FILEDIALOG_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_FILEDIALOG_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <pmath-cpp.h>
#include <util/base.h>


namespace richmath {
  class Win32FileDialog: public Base {
      class Impl;
    public:
      Win32FileDialog(bool to_save);
      ~Win32FileDialog();
      
      void set_title(pmath::String title);
      void set_filter(pmath::Expr filter);
      void set_initial_file(pmath::String initialfile);
      
      pmath::Expr show_dialog();
      
    private:
      pmath::String _title_z;
      pmath::String _filters_z;
      pmath::String _default_extension_z;
      pmath::String _initialfile;
      bool          _to_save;
  };
}


#endif // RICHMATH__GUI__WIN32__WIN32_FILEDIALOG_H__INCLUDED
