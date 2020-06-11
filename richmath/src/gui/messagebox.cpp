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
  return mgtk_ask_save(doc, std::move(text));
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_save(doc, std::move(text));
#endif
  
  return YesNoCancel::Yes;
}

Expr richmath::ask_interrupt() {
#ifdef RICHMATH_USE_GTK_GUI
  return mgtk_ask_interrupt();
#endif
#ifdef RICHMATH_USE_WIN32_GUI
  return win32_ask_interrupt();
#endif
  return Expr();
}

