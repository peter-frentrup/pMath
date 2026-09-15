#include <gui/fltk/mfltk-document-window.h>

#include <FL/FL.H>
#include <FL/FL_Window.H>


namespace richmath {
  class MathFltkDocumentWindow::Impl: public Fl_Window {
      using base = Fl_Window;
      friend class MathFltkDocumentWindow;
    public:
      Impl(MathFltkDocumentWindow *publ, int w, int h);
      ~Impl();
    private:
      MathFltkDocumentWindow *publ;
  };
}

using namespace richmath;

//{ class MathFltkDocumentWindow ...

MathFltkDocumentWindow::MathFltkDocumentWindow()
: _impl{nullptr}
{
}

MathFltkDocumentWindow::~MathFltkDocumentWindow() {
  if(_impl && _impl->publ) {
    _impl->publ = nullptr;
    Fl::delete_widget(_impl);
    _impl = nullptr;
  }
}

//void MathFltkDocumentWindow::after_construction() {
//
//}

//} ... class MathFltkDocumentWindow

//{ class MathFltkDocumentWindow::Impl ...

MathFltkDocumentWindow::Impl::Impl(MathFltkDocumentWindow *publ, int w, int h)
: base{w, h},
  publ{publ}
{
}

MathFltkDocumentWindow::Impl::~Impl() {
  if(publ) {
    publ->_impl = nullptr;
    delete publ;
  }
}

//} ... class MathFltkDocumentWindow::Impl

