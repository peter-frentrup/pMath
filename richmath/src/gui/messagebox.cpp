#include <gui/messagebox.h>

#include <gui/native-widget.h>


#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-messagebox.h>
#endif
#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-messagebox.h>
#endif


using namespace richmath;

YesNoCancel richmath::ask_save(Document *doc) {
  String text;
  
  if(doc) {
    text = "Do you want to save changes to ";
    text+= String::FromChar(0x201C);
    text+= doc->native()->window_title();
    text+= String::FromChar(0x201D);
    text+= "?";
  }
  else
    text = "Do you want to save changes?";
  
#ifdef RICHMATH_USE_GTK_GUI
  return mgtk_ask_save(doc, PMATH_CPP_MOVE(text));
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_save(doc, PMATH_CPP_MOVE(text));
#endif
  
  return YesNoCancel::Yes;
}

YesNoCancel richmath::ask_remove_private_style_definitions(Document *doc) {
#ifdef RICHMATH_USE_GTK_GUI
  return mgtk_ask_remove_private_style_definitions(doc);
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_remove_private_style_definitions(doc);
#endif
  return YesNoCancel::Yes;
}

bool richmath::ask_open_suspicious_system_file(String path) {
#ifdef RICHMATH_USE_GTK_GUI
  return mgtk_ask_open_suspicious_system_file(path);
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_open_suspicious_system_file(path);
#endif
  return false;
}

Expr richmath::ask_interrupt(Expr stack) {
#ifdef RICHMATH_USE_GTK_GUI
  return mgtk_ask_interrupt(PMATH_CPP_MOVE(stack));
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_interrupt(PMATH_CPP_MOVE(stack));
#endif
  return Expr();
}

