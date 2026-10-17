#include <gui/fltk/mfltk-messagebox.h>

#include <FL/fl_ask.H>

#include <gui/fltk/mfltk-widget.h>


using namespace richmath;

YesNoCancel richmath::mfltk_ask_save(Document *doc, String question) {
  int len;
  char *question_utf8 = pmath_string_to_utf8(question.get(), &len);
  
  if(auto wid = dynamic_cast<MathFltkWidget*>(doc->native())) {
    if(Fl_Widget *owner_wid = wid->widget())
      fl_message_position(owner_wid);
  }
  fl_message_title("Richmath");
  int answer = fl_choice(
    "%s",
    "Cancel",     // 0 (ESC key/close)
    "Save",       // 1 (default: return key)
    "Don't save", // 2
    question_utf8 ? question_utf8 : "");
  
  pmath_mem_free(question_utf8);
  
  switch(answer) {
    case 0: return YesNoCancel::Cancel;
    case 1: return YesNoCancel::Yes;
    case 2: return YesNoCancel::No;
  }
  return YesNoCancel::Cancel;
}

YesNoCancel richmath::mfltk_ask_remove_private_style_definitions(Document *doc) {
  if(auto wid = dynamic_cast<MathFltkWidget*>(doc->native())) {
    if(Fl_Widget *owner_wid = wid->widget())
      fl_message_position(owner_wid);
  }
  fl_message_title("Richmath");
  
  int answer = fl_choice(
    "This document has private style definitions.\nAre you sure you want to replace them with a shared stylesheet?",
    "Keep private style definitions",                                // 0 (ESC key/close)
    0,                                                               // 1 (default: return key)
    "Delete priv. style definitions. Use shared stylesheet instead"  // 2
  );
  
  switch(answer) {
    case 0: return YesNoCancel::No;
    case 2: return YesNoCancel::Yes;
  }
  return YesNoCancel::Cancel;
}

bool richmath::mfltk_ask_open_suspicious_system_file(String path) {
  // TODO
  return false;
}

Expr richmath::mfltk_ask_interrupt(Expr stack) {
  // TODO
  return Expr();
}
