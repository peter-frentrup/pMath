#ifndef RICHMATH__GUI__FLTK__MFLTK_MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_MESSAGEBOX_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <gui/messagebox.h>

namespace richmath {
  YesNoCancel mfltk_ask_save(Document *doc, String question);
  YesNoCancel mfltk_ask_remove_private_style_definitions(Document *doc);
  bool mfltk_ask_open_suspicious_system_file(String path);
  Expr mfltk_ask_interrupt(Expr stack);
}

#endif // RICHMATH__GUI__FLTK__MFLTK_MESSAGEBOX_H__INCLUDED
