#ifndef RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/messagebox.h>


namespace richmath {
  YesNoCancel mgtk_ask_save(Document *doc, String question);
}

#endif // RICHMATH__GUI__GTK__MGTK_MESSAGEBOX_H__INCLUDED
