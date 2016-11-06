#include <gui/gtk/mgtk-filedialog.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <gui/gtk/mgtk-widget.h>


using namespace richmath;
using namespace pmath;


namespace richmath {
  class MathGtkFileDialogImpl {
    private:
      MathGtkFileDialog &self;
      
    public:
      MathGtkFileDialogImpl(MathGtkFileDialog &_self): self(_self) {}
    
    public:
      void add_filter(Expr caption, Expr extensions) {
        if(!caption.is_string())
          return;
        
        if(extensions.is_string()) 
          extensions = List(extensions);
        
        if(extensions.expr_length() >= 1 && extensions[0] == PMATH_SYMBOL_LIST) {
          bool all_strings = true;
          
          for(size_t j = extensions.expr_length(); j > 0; --j) {
            if(!extensions[j].is_string()) {
              all_strings = false;
              break;
            }
          }
          
          if(all_strings) {
            GtkFileFilter *next_filter = gtk_file_filter_new();
            
            String str = String(caption);
            char *utf8 = pmath_string_to_utf8(str.get_as_string(), nullptr);
            if(utf8) {
              gtk_file_filter_set_name(next_filter, utf8);
              pmath_mem_free(utf8);
            }
            
            for(size_t j = 1; j <= extensions.expr_length(); ++j) {
              str = String(extensions[j]);
              
              if(str.equals("*.*")) {
                gtk_file_filter_add_pattern(next_filter, "*");
              }
              else {
                utf8 = pmath_string_to_utf8(str.get_as_string(), nullptr);
                if(utf8) {
                  gtk_file_filter_add_pattern(next_filter, utf8);
                  pmath_mem_free(utf8);
                }
              }
            }
            
            gtk_file_chooser_add_filter(self._chooser, next_filter);
          }
        }
      }
    
    public:
      GtkWindow *get_parent_window() {
        Box *box = Application::get_evaluation_box();
        if(!box)
          box = get_current_document();
          
        if(!box)
          return nullptr;
          
        Document *doc = box->find_parent<Document>(true);
        if(!doc)
          return nullptr;
          
        MathGtkWidget *widget = dynamic_cast<MathGtkWidget *>(doc->native());
        if(!widget)
          return nullptr;
          
        GtkWidget *wid = widget->widget();
        if(!wid)
          return nullptr;
          
        return GTK_WINDOW(gtk_widget_get_ancestor(wid, GTK_TYPE_WINDOW));
      }
  };
}

//{ class MathGtkFileDialog ...

MathGtkFileDialog::MathGtkFileDialog(bool to_save)
  : Base(),
    _dialog(nullptr),
    _chooser(nullptr)
{
  _dialog = GTK_FILE_CHOOSER_DIALOG(
              gtk_file_chooser_dialog_new(
                to_save ? "Save" : "Open",
                nullptr,
                to_save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
                GTK_STOCK_CANCEL,                          GTK_RESPONSE_CANCEL,
                to_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                nullptr));
                
  _chooser = GTK_FILE_CHOOSER(_dialog);
  gtk_file_chooser_set_select_multiple(          _chooser, !to_save);
  gtk_file_chooser_set_do_overwrite_confirmation(_chooser, to_save);
}

MathGtkFileDialog::~MathGtkFileDialog() {
  gtk_widget_destroy(GTK_WIDGET(_dialog));
}

void MathGtkFileDialog::set_title(String title) {
  if(title.is_valid()) {
    char *utf8 = pmath_string_to_utf8(title.get_as_string(), nullptr);
    
    if(utf8) {
      gtk_window_set_title(GTK_WINDOW(_dialog), utf8);
      pmath_mem_free(utf8);
    }
  }
}

void MathGtkFileDialog::set_filter(Expr filter) {
  if(filter.expr_length() == 0 || filter[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= filter.expr_length(); ++i) {
    Expr rule = filter[i];
    
    if(rule.is_rule()) 
      MathGtkFileDialogImpl(*this).add_filter(rule[1], rule[2]);
  }
}

void MathGtkFileDialog::set_initial_file(String initialfile) {
  if(!initialfile.is_valid()) 
    return;
    
  // TODO: This should only be used when the file already exists.
  // gtk_file_chooser_set_current_folder() + gtk_file_chooser_set_current_name() should be used for not-yet existing files!
  
  char *utf8 = pmath_string_to_utf8(initialfile.get_as_string(), nullptr);
  if(utf8) {
    gtk_file_chooser_set_filename(_chooser, utf8);
    pmath_mem_free(utf8);
  }
}

Expr MathGtkFileDialog::show_dialog() {
  GtkWindow *parent = MathGtkFileDialogImpl(*this).get_parent_window();
  if(parent)
    gtk_window_set_transient_for(GTK_WINDOW(_dialog), parent);
  
  // TODO: goto working directory when no initialfile was given
  int result = gtk_dialog_run(GTK_DIALOG(_dialog));
  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        GSList *list = gtk_file_chooser_get_filenames(_chooser);
        
        Gather g;
        for(GSList *cur = list; cur; cur = cur->next) {
          char *utf8_filename = (char *)cur->data;
          Gather::emit(String::FromUtf8(utf8_filename));
          g_free(utf8_filename);
        }
        
        g_slist_free(list);
        
        Expr result = g.end();
        if(result.expr_length() == 1)
          return result[1];
          
        return result;
      }
  }
  
  return Symbol(PMATH_SYMBOL_CANCELED);
}

//} ... class MathGtkFileDialog
