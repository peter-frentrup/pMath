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
  Expr ask_interrupt();
}

#endif // RICHMATH__GUI__MESSAGEBOX_H__INCLUDED
