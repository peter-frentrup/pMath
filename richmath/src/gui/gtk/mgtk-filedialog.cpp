#include <gui/gtk/mgtk-filedialog.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <gui/gtk/mgtk-widget.h>


using namespace richmath;
using namespace pmath;


//{ class MathGtkFileDialog ...

Expr MathGtkFileDialog::show(
  bool    save,
  String  initialfile,
  Expr    filter,
  String  title
) {
  GtkFileChooserDialog *dialog;
  GtkFileChooser       *chooser;
  
  GtkWindow *parent_window = 0;
  Box *box = Application::get_evaluation_box();
  if(!box)
    box = get_current_document();
    
  if(box) {
    Document *doc = box->find_parent<Document>(true);
    
    if(doc) {
      MathGtkWidget *widget = dynamic_cast<MathGtkWidget *>(doc->native());
      
      if(widget) {
        GtkWidget *wid = widget->widget();
        
        if(wid)
          parent_window = GTK_WINDOW(gtk_widget_get_ancestor(wid, GTK_TYPE_WINDOW));
      }
    }
  }
  
  
  const char *utf8_title      = NULL;
  char       *utf8_title_data = NULL;
  if(title.is_valid()){
    utf8_title = utf8_title_data = pmath_string_to_utf8(title.get_as_string(), NULL);
  }
  else{
//    GtkStockItem item;
//    gtk_stock_lookup(save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, &item);
//    utf8_title = item.label;
    if(save)
      utf8_title = "Save";
    else
      utf8_title = "Open";
  }
  
  dialog = GTK_FILE_CHOOSER_DIALOG(
             gtk_file_chooser_dialog_new(
               utf8_title,
               parent_window,
               save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
               GTK_STOCK_CANCEL,                       GTK_RESPONSE_CANCEL,
               save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
               NULL));
               
  pmath_mem_free(utf8_title_data);
  
  chooser = GTK_FILE_CHOOSER(dialog);
  gtk_file_chooser_set_select_multiple(          chooser, !save);
  gtk_file_chooser_set_do_overwrite_confirmation(chooser, save);
  
  if(filter.expr_length() > 0 && filter[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= filter.expr_length(); ++i) {
      Expr rule = filter[i];
      
      if(rule.is_rule()) {
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
        if(!lhs.is_string())
          continue;
          
        if(rhs.is_string())
          rhs = List(rhs);
          
        if(rhs.expr_length() >= 1 && rhs[0] == PMATH_SYMBOL_LIST) {
          bool all_strings = true;
          
          for(size_t j = rhs.expr_length(); j > 0; --j) {
            if(!rhs[j].is_string()) {
              all_strings = false;
              break;
            }
          }
          
          if(all_strings) {
            GtkFileFilter *next_filter = gtk_file_filter_new();
            
            String str = String(lhs);
            char *utf8 = pmath_string_to_utf8(str.get_as_string(), NULL);
            gtk_file_filter_set_name(next_filter, utf8);
            pmath_mem_free(utf8);
            
            for(size_t j = 1; j <= rhs.expr_length(); ++j) {
              str = String(rhs[j]);
              
              if(str.equals("*.*")) {
                gtk_file_filter_add_pattern(next_filter, "*");
              }
              else {
                utf8 = pmath_string_to_utf8(str.get_as_string(), NULL);
                gtk_file_filter_add_pattern(next_filter, utf8);
                pmath_mem_free(utf8);
              }
            }
            
            gtk_file_chooser_add_filter(chooser, next_filter);
          }
        }
      }
    }
  }
  
  if(initialfile.is_valid()) {
    // TODO: This should only be used when the file already exists.
    // gtk_file_chooser_set_current_folder() + gtk_file_chooser_set_current_name() should be used for not-yet existing files!
    
    char *utf8_filename = pmath_string_to_utf8(initialfile.get_as_string(), NULL);
    if(utf8_filename) {
      gtk_file_chooser_set_filename(chooser, utf8_filename);
      pmath_mem_free(utf8_filename);
    }
  }
  // TODO: goto working directory when no initialfile is given
  
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        GSList *list = gtk_file_chooser_get_filenames(chooser);
        
        Gather g;
        
        for(GSList *cur = list; cur; cur = cur->next) {
          char *utf8_filename = (char *)cur->data;
          
          Gather::emit(String::FromUtf8(utf8_filename));
          
          g_free(utf8_filename);
        }
        
        g_slist_free(list);
        
        gtk_widget_destroy(GTK_WIDGET(dialog));
        
        Expr result = g.end();
        if(result.expr_length() == 1)
          return result[1];
          
        return result;
      }
  }
  
  //if(err)
  //  return Symbol(PMATH_SYMBOL_ABORTED);
  
  gtk_widget_destroy(GTK_WIDGET(dialog));
  
  return Symbol(PMATH_SYMBOL_CANCELED);
}

//} ... class MathGtkFileDialog
