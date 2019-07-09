#ifndef RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/messagebox.h>


namespace richmath {
  YesNoCancel win32_ask_save(Document *doc, String question);
}

#endif // RICHMATH__GUI__WIN32__WIN32_MESSAGEBOX_H__INCLUDED