#include <gui/gtk/mgtk-messagebox.h>

#include <gui/gtk/mgtk-widget.h>

#include <boxes/section.h>

#include <gui/documents.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String Head;
  extern String Location;
}}

extern pmath_symbol_t richmath_System_Abort;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Dialog;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_Developer_DebugInfoOpenerFunction;
extern pmath_symbol_t richmath_FE_CallFrontEnd;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

namespace {
  class MathGtkHyperlinks {
    public:
      MathGtkHyperlinks();
      
      String register_hyperlink_action(Expr action);
      
      void connect(GtkWidget *label);
      void disconnect(GtkWidget *label);
    
    private:
      bool on_activate_link(GtkLabel *label, const char *uri);
    
    private:
      Expr hyperlink_actions;
    
    private:
      static gboolean activate_link_callback(GtkLabel *label, gchar *uri, gpointer user_data);
  };
}

static GtkWidget *make_left_aligned(GtkWidget *widget) {
  GtkWidget *alignment = gtk_alignment_new(0.0f, 0.0f, 0.0f, 0.0f);
  gtk_container_add(GTK_CONTAINER(alignment), widget);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 5, 0, 10, 10);
  return alignment;
}

YesNoCancel richmath::mgtk_ask_save(Document *doc, String question) {
  GtkWindow *owner_window = nullptr;
  
  if(doc) {
    MathGtkWidget *mwid = dynamic_cast<MathGtkWidget*>(doc->native());
    GtkWidget *wid = nullptr;
    if(mwid) 
      wid = mwid->widget();
    
    if(wid) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(wid);
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    GTK_STOCK_SAVE, // "_Save", 
    GTK_RESPONSE_YES,
    "Do_n't save", // GTK_STOCK_DISCARD,
    GTK_RESPONSE_NO,
    GTK_STOCK_CANCEL, // "_Cancel",
    GTK_RESPONSE_CANCEL,
    nullptr);
  
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
  
  char *utf8_question = pmath_string_to_utf8(question.get_as_string(), nullptr);
  GtkWidget *label = gtk_label_new(utf8_question ? utf8_question : "Save changes?");
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  //gtk_box_pack_end(GTK_BOX(content_box), label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), make_left_aligned(label));
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  pmath_mem_free(utf8_question);
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case GTK_RESPONSE_YES: return YesNoCancel::Yes;
    case GTK_RESPONSE_NO:  return YesNoCancel::No;
    default:               return YesNoCancel::Cancel;
  }
}

YesNoCancel richmath::mgtk_ask_remove_private_style_definitions(Document *doc) {
  GtkWindow *owner_window = nullptr;
  
  if(doc) {
    MathGtkWidget *mwid = dynamic_cast<MathGtkWidget*>(doc->native());
    GtkWidget *wid = nullptr;
    if(mwid) 
      wid = mwid->widget();
    
    if(wid) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(wid);
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    "_Delete private style definitions",
    GTK_RESPONSE_YES,
    "_Keep private style definitions", // GTK_STOCK_DISCARD,
    GTK_RESPONSE_NO,
    nullptr);
  
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
  GtkWidget *label = gtk_label_new("This document has private style definitions.\nAre you sure you want to replace them with a shared stylesheet?");
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  //gtk_box_pack_end(GTK_BOX(content_box), label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), make_left_aligned(label));
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case GTK_RESPONSE_YES: return YesNoCancel::Yes;
    case GTK_RESPONSE_NO:  return YesNoCancel::No;
    default:               return YesNoCancel::Cancel;
  }
}

Expr richmath::mgtk_ask_interrupt(Expr stack) {
  MathGtkHyperlinks hyperlinks;
  GtkWindow *owner_window = nullptr;
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    doc = Documents::current();
  
  if(MathGtkWidget *mwid = doc ? dynamic_cast<MathGtkWidget*>(doc->native()) : nullptr) {
    GtkWidget *wid = nullptr;
    if(mwid) 
      wid = mwid->widget();
    
    if(wid) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(wid);
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
  
  const int ResponseAbort = 1;
  const int ResponseEnterSubsession = 2;
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    "_Abort", 
    ResponseAbort,
    "_Enter subsession", // GTK_STOCK_DISCARD,
    ResponseEnterSubsession,
    "_Continue", 
    GTK_RESPONSE_CANCEL,
    nullptr);
  
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  
  GtkWidget *caption_label = gtk_label_new("<big><b>An interrupt occurred</b></big>");
  gtk_label_set_use_markup(GTK_LABEL(caption_label), TRUE);
  GtkWidget *desctiption_label = nullptr;
  GtkWidget *details_section = nullptr;
  GtkWidget *details_label = nullptr;
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  //gtk_container_set_border_width(GTK_CONTAINER(content_box), 5);
  gtk_box_set_spacing(GTK_BOX(content_box), 5);
  
  //gtk_box_pack_end(GTK_BOX(content_box), caption_label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), make_left_aligned(caption_label));
  
  String content;
  if(box) {
    if(Section *sect = box->find_parent<Section>(true)) {
      content = "During evaluation of <a href=\"";
      content+= hyperlinks.register_hyperlink_action(
                  Call(Symbol(richmath_FE_CallFrontEnd),
                    Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
                    Symbol(richmath_System_Automatic),
                    sect->id().to_pmath())));
      content+= "\">";
      content+= sect->get_own_style(SectionLabel, "?");
      content+= "</a>";
    }
  }
  
  String details;
  if(stack[0] == richmath_System_List && stack.expr_length() > 1) {
    Expr default_name = String("?");
    
    for(size_t i = stack.expr_length() - 1; i > 0; --i) {
      Expr frame = stack[i];
      
      if(details.is_string())
        details+= "\n";
      
      String name = frame.lookup(strings::Head, default_name).to_string();
      bool have_link = false;
      Expr location {};
      if(frame.try_lookup(strings::Location, location)) {
        location = Application::interrupt_wait(Call(Symbol(richmath_Developer_DebugInfoOpenerFunction), std::move(location)));
        
        if(location[0] == richmath_System_Function) {
          if(location.expr_length() == 1)
            location = location[1];
          else
            location = Call(location);
          
          details+= "<a href=\"";
          details+= hyperlinks.register_hyperlink_action(location);
          details+= "\">";
          have_link = true;
        }
      }
      details+= name;
      if(have_link)
        details+= "</a>";
    }
  }
  
  if(content.is_string()) {
    content+= String::FromChar(0);
    if(char *utf8 = pmath_string_to_utf8(content.get(), nullptr)) {
      desctiption_label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(desctiption_label), utf8);
      pmath_mem_free(utf8);
      
      gtk_container_add(GTK_CONTAINER(content_box), make_left_aligned(desctiption_label));
      
      hyperlinks.connect(desctiption_label);
    }
  }
  
  if(details.is_string()) {
    details+= String::FromChar(0);
    if(char *utf8 = pmath_string_to_utf8(details.get(), nullptr)) {
      details_section = gtk_expander_new_with_mnemonic("Stack _trace:");
      GtkWidget *details_label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(details_label), utf8);
      pmath_mem_free(utf8);
      
      gtk_container_add(GTK_CONTAINER(details_section), make_left_aligned(details_label));
      gtk_container_add(GTK_CONTAINER(content_box), make_left_aligned(details_section));
      
      hyperlinks.connect(details_label);
    }
  }
  
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  hyperlinks.disconnect(details_label);
  hyperlinks.disconnect(desctiption_label);
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case ResponseAbort:           return Call(Symbol(richmath_System_Abort));
    case ResponseEnterSubsession: return Call(Symbol(richmath_System_Dialog));
  }
  
  return Expr();
}

//{ class MathGtkHyperlinks ...

MathGtkHyperlinks::MathGtkHyperlinks() {
}

String MathGtkHyperlinks::register_hyperlink_action(Expr action) {
  if(hyperlink_actions.is_null())
    hyperlink_actions = List(std::move(action));
  else
    hyperlink_actions.append(std::move(action));
  
  return Expr(hyperlink_actions.expr_length()).to_string();
}

void MathGtkHyperlinks::connect(GtkWidget *label) {
  if(!label)
    return;
    
  g_signal_connect(label, "activate-link", G_CALLBACK(activate_link_callback), this);
}

void MathGtkHyperlinks::disconnect(GtkWidget *label) {
  if(!label)
    return;
  
  g_signal_handlers_disconnect_by_data(label, this);
}

bool MathGtkHyperlinks::on_activate_link(GtkLabel *label, const char *uri) {
  size_t i = 0;
  const char *s = uri;
  while(*s >= '0' && *s <= '9' && i < hyperlink_actions.expr_length()) {
    i = 10*i + (*s - '0');
    ++s;
  }
  
  if(*s == '\0' && i >= 1 && i <= hyperlink_actions.expr_length()) {
    Expr action = hyperlink_actions[i];
    Application::interrupt_wait(action);
    return true;
  }
  return false;
}

gboolean MathGtkHyperlinks::activate_link_callback(GtkLabel *label, gchar *uri, gpointer user_data) {
  MathGtkHyperlinks *self = (MathGtkHyperlinks*)user_data;
  return self->on_activate_link(label, uri);
}

//} ... class MathGtkHyperlinks
