#ifndef RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <gui/common-document-windows.h>
#include <gui/native-widget.h>


namespace richmath {
  class MathFltkDocumentWindow final : public CommonDocumentWindow {
      class Impl;
    protected:
//      virtual void after_construction() override;
      
    public:
      MathFltkDocumentWindow();
      virtual ~MathFltkDocumentWindow();
    
    private:
      Impl *_impl;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED
