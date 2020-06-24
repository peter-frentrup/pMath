#ifndef RICHMATH__GUI__MESSAGEBOX_H__INCLUDED
#define RICHMATH__GUI__MESSAGEBOX_H__INCLUDED

#include <gui/document.h>

namespace richmath {
  enum class YesNoCancel {
    No,
    Yes,
    Cancel
  };
  
  YesNoCancel ask_save(Document *doc);
  YesNoCancel ask_remove_private_style_definitions(Document *doc);
  Expr ask_interrupt(Expr stack);
}

#endif // RICHMATH__GUI__MESSAGEBOX_H__INCLUDED
