#include <gui/gtk/mgtk-document-window.h>

#include <eval/application.h>
#include <eval/binding.h>


using namespace richmath;

class richmath::MathGtkWorkingArea: public MathGtkWidget{
  public:
    MathGtkWorkingArea(MathGtkDocumentWindow *parent)
    : MathGtkWidget(new Document()),
      _parent(parent)
    {
    }
    
    MathGtkDocumentWindow *parent(){ return _parent; }
    
  private:
    MathGtkDocumentWindow *_parent;
};

class richmath::MathGtkDock: public MathGtkWidget{
  public:
    MathGtkDock(MathGtkDocumentWindow *parent)
    : MathGtkWidget(new Document()),
      _parent(parent)
    {
    }
    
    MathGtkDocumentWindow *parent(){ return _parent; }
    
  private:
    MathGtkDocumentWindow *_parent;
};

//{ class MathGtkDocumentWindow ...

static MathGtkDocumentWindow *_first_window = NULL;

MathGtkDocumentWindow::MathGtkDocumentWindow()
: BasicGtkWidget(),
  _is_palette(false)
{
  _working_area = new MathGtkWorkingArea(this);
  _top_area     = new MathGtkDock(this);
  _bottom_area  = new MathGtkDock(this);
  
  if(_first_window){
    _prev_window = _first_window->_prev_window;
    _prev_window->_next_window = this;
    _next_window = _first_window;
    _first_window->_prev_window = this;
  }
  else{
    _first_window = this;
    _prev_window = this;
    _next_window = this;
  }
}

void MathGtkDocumentWindow::after_construction(){
  _widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  
  BasicGtkWidget::after_construction();
  
  _working_area->init();
  _top_area->init();
  _bottom_area->init();
  
  top()->main_document    = document();
  bottom()->main_document = document();
  
  
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(_widget), vbox);
  
  gtk_box_pack_start(GTK_BOX(vbox), _top_area->widget(),     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), _working_area->widget(), TRUE,  TRUE,  0);
  gtk_box_pack_end(  GTK_BOX(vbox), _bottom_area->widget(),  FALSE, FALSE, 0);
  
  gtk_widget_show_all(vbox);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  GList *focus_chain = NULL;
  focus_chain = g_list_prepend(focus_chain, _working_area->widget());
  gtk_container_set_focus_chain(GTK_CONTAINER(vbox), focus_chain);
  g_list_free(focus_chain);
  
  all_document_ids.set(document()->id(), Void());
  if(get_current_document() == 0){
    set_current_document(document());
  }
  
  title("untitled");
}

MathGtkDocumentWindow::~MathGtkDocumentWindow(){
  all_document_ids.remove(_working_area->document()->id());
  
  static bool deleting_all = false;
  if(!deleting_all){
    bool have_only_palettes = true;
    for(MathGtkDocumentWindow *win = next_window();win != this;win = win->next_window()){
      if(!win->is_palette()){
        have_only_palettes = false;
        break;
      }
    }
    
    if(have_only_palettes){
      deleting_all = true;
      
      MathGtkDocumentWindow *other = next_window();
      while(other && other != this){
        MathGtkDocumentWindow *next = other->next_window();
        
        delete other;
        
        other = next;
      }
      
      deleting_all = false;
      
      gtk_main_quit();
    }
  }
  
  if(_first_window == this){
    _first_window = _next_window;
    if(_first_window == this)
      _first_window = 0;
  }
  
  _next_window->_prev_window = _prev_window;
  _prev_window->_next_window = _next_window;
  
  delete _top_area;
  delete _bottom_area;
  delete _working_area;
}

void MathGtkDocumentWindow::title(String text){
  _title = text;
  
  if(!Application::is_idle(document()->id()))
    text = String("Running... ") + text;
  
  char *str = pmath_string_to_utf8(text.get(), NULL);
  if(str)
    gtk_window_set_title(GTK_WINDOW(_widget), str);
  
  pmath_mem_free(str);
}

void MathGtkDocumentWindow::is_palette(bool value){
  _is_palette = value;
  
  gtk_window_set_resizable( GTK_WINDOW(_widget), !value);
  gtk_window_set_keep_above(GTK_WINDOW(_widget), value);
  
  if(value)
    gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_UTILITY);
  else
    gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_NORMAL);
}

MathGtkDocumentWindow *MathGtkDocumentWindow::first_window(){
  return _first_window;
}

//} ... class MathGtkDocumentWindow
