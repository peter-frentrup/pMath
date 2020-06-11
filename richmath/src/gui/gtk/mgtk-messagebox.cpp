#include <gui/gtk/mgtk-messagebox.h>

#include <gui/gtk/mgtk-widget.h>

#include <eval/binding.h>


using namespace richmath;


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
  gtk_container_add(GTK_CONTAINER(content_box), label);
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

Expr richmath::mgtk_ask_interrupt() {
  GtkWindow *owner_window = nullptr;
  
  Document *doc = nullptr;
  Box *box = Application::find_current_job();
  if(box)
    doc = box->find_parent<Document>(true);
  
  if(!doc)
    doc = get_current_document();
  
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
  
  GtkWidget *label = gtk_label_new("An interrupt occurred.");
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  //gtk_box_pack_end(GTK_BOX(content_box), label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), label);
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case ResponseAbort:           return Call(Symbol(PMATH_SYMBOL_ABORT));
    case ResponseEnterSubsession: return Call(Symbol(PMATH_SYMBOL_DIALOG));
  }
  
  return Expr();
}
