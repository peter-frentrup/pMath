#ifndef RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/messagebox.h>


namespace richmath {
  YesNoCancel win32_ask_save(Document *doc, String question);
  YesNoCancel win32_ask_remove_private_style_definitions(Document *doc);
  bool win32_ask_open_suspicious_system_file(String path);
  Expr win32_ask_interrupt(Expr stack);
}

#endif // RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED
