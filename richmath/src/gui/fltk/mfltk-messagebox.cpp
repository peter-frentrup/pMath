#include <gui/fltk/mfltk-messagebox.h>

#include <FL/fl_ask.H>

#include <gui/fltk/mfltk-widget.h>
#include <gui/documents.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String Head;
  extern String Location;
}}

extern pmath_symbol_t richmath_System_Abort;
extern pmath_symbol_t richmath_System_Dialog;
extern pmath_symbol_t richmath_System_List;


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
    "&Cancel",     // 0 (ESC key/close)
    "&Save",       // 1 (default: return key)
    "Do&n't save", // 2
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
    ( "This document has private style definitions.\n"
      "Are you sure you want to replace them with a shared stylesheet?"),
    "&Keep private style definitions",                                // 0 (ESC key/close)
    0,                                                                // 1 (default: return key)
    "&Delete priv. style definitions. Use shared stylesheet instead"  // 2
  );
  
  switch(answer) {
    case 0: return YesNoCancel::No;
    case 2: return YesNoCancel::Yes;
  }
  return YesNoCancel::Cancel;
}

bool richmath::mfltk_ask_open_suspicious_system_file(String path) {
  int len;
  char *path_utf8 = pmath_string_to_utf8(path.get(), &len);
  if(!path_utf8)
    return false;
  
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(!doc)
    doc = Documents::selected_document();
  
  if(doc) {  
    if(auto wid = dynamic_cast<MathFltkWidget*>(doc->native())) {
      if(Fl_Widget *owner_wid = wid->widget())
        fl_message_position(owner_wid);
    }
  }
  
  fl_message_title("Richmath");
  int answer = fl_choice(
    ( "Open suspicous file?\n"
      "The file %s has an unrecognized file extension.\n"
      "Do you really want to open it?"),
    "Do &not open suspicious file", // 0 (ESC key/close)
    0,                              // 1 (default: return key)
    "&Open anyway",                 // 2
    path_utf8);
  
  pmath_mem_free(path_utf8);
  
  return answer == 2;
}

Expr richmath::mfltk_ask_interrupt(Expr stack) {
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    doc = Documents::selected_document();
  
  if(doc) {  
    if(auto wid = dynamic_cast<MathFltkWidget*>(doc->native())) {
      if(Fl_Widget *owner_wid = wid->widget())
        fl_message_position(owner_wid);
    }
  }
  
  String content;
  if(box) {
    if(Section *sect = box->find_parent<Section>(true)) {
      content = "During evaluation of ";
      content+= sect->get_own_style(SectionLabel, "?");
    }
  }
  
  String details;
  if(stack.item_equals(0, richmath_System_List) && stack.expr_length() > 1) {
    Expr default_name = String("?");
    
    details = "Stack trace:";
    for(size_t i = stack.expr_length() - 1; i > 0; --i) {
      Expr frame = stack[i];
      
      details+= "\n";
      String name = frame.lookup(strings::Head, default_name).to_string();
      bool have_link = false;
      details+= "- ";
//      Expr location {};
//      if(frame.try_lookup(strings::Location, location)) {
//        location = Application::interrupt_wait(Call(Symbol(richmath_Developer_SourceLocationOpenerFunction), PMATH_CPP_MOVE(location)));
//        
//        if(location.item_equals(0, richmath_System_Function)) {
//          if(location.expr_length() == 1)
//            location = location[1];
//          else
//            location = Call(location);
//          
//          details+= "<a href=\"";
//          details+= config.register_hyperlink_action(location);
//          details+= "\">";
//          have_link = true;
//        }
//      }
      details+= name;
//      if(have_link)
//        details+= "</a>";
    }
  }
  
  int content_utf8_len;
  char *content_utf8 = pmath_string_to_utf8(content.get(), &content_utf8_len);
  
  int details_utf8_len;
  char *details_utf8 = pmath_string_to_utf8(details.get(), &details_utf8_len);
  
  fl_message_title("Richmath");
  int answer = fl_choice(
    ( "An interrupt occurred.\n"
      "%s\n"
      "%s\n"
      "* You can exit the subsession with Return() to continue the \n"
      "  current evaluation or Return(Abort()) to abort it."),
    "&Continue evaluation",      // 0 (ESC key/close)
    "&Enter subsession *",       // 1 (default: return key) 
    "&Abort current evaluation", // 2
    content_utf8 ? content_utf8 : "",
    details_utf8 ? details_utf8 : "");
  
  pmath_mem_free(content_utf8);
  pmath_mem_free(details_utf8);
  
  switch(answer) {
    case 0: return Expr();
    case 1: return Call(Symbol(richmath_System_Dialog));
    case 2: return Call(Symbol(richmath_System_Abort));
  }
  
  return Expr();
}
