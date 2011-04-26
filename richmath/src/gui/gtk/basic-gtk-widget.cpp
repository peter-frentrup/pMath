#include <gui/gtk/basic-gtk-widget.h>


using namespace richmath;

const char widget_key[] = "Richmath C++ Pointer";

static void add_remove_window(int count){
  static int global_window_count = 0;

  global_window_count+= count;
}

//{ class BasicGtkWidget ...

BasicGtkWidget::BasicGtkWidget()
: Base(),
  _widget(0),
  init_data(new InitData),
  _initializing(true)
{
  add_remove_window(+1);
}

void BasicGtkWidget::after_construction(){
  if(!_widget){
    _widget = gtk_drawing_area_new();
  }
  
  g_object_set_data(
    G_OBJECT(_widget), 
    widget_key, 
    this);
  
  signal_connect<BasicGtkWidget, &BasicGtkWidget::on_delete>("delete-event");
               
  delete init_data;
}

BasicGtkWidget::~BasicGtkWidget(){
  if(_widget){
    gtk_widget_destroy(_widget);
    _widget = 0;
  }
  
  add_remove_window(-1);
}

BasicGtkWidget *BasicGtkWidget::parent(){
  if(!_widget)
    return 0;
  
  GtkWidget *p = gtk_widget_get_parent(_widget);
  
  if(p)
    return (BasicGtkWidget*)g_object_get_data(G_OBJECT(p), widget_key);
  
  return 0;
}

BasicGtkWidget *BasicGtkWidget::from_widget(GtkWidget *wid){
  if(!wid)
    return 0;
  
  return (BasicGtkWidget*)g_object_get_data(G_OBJECT(wid), widget_key);
}

bool BasicGtkWidget::on_delete(GdkEvent *e){
  delete this;
  return true;
}

//} ... class BasicGtkWidget
