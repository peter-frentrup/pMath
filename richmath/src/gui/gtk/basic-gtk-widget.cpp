#include <gui/gtk/basic-gtk-widget.h>

#include <pmath-cpp.h>


using namespace richmath;

const char widget_key[] = "Richmath C++ Pointer";

static void add_remove_window(int count) {
  static int global_window_count = 0;
  
  global_window_count += count;
}

//{ class BasicGtkWidget ...

BasicGtkWidget::BasicGtkWidget()
  : Base(),
  _widget(0),
  _initializing(true),
  _destroying(false)
{
  add_remove_window(+1);
}

void BasicGtkWidget::after_construction() {
  if(!_widget) {
    _widget = gtk_drawing_area_new();
  }
  
  g_object_set_data_full(
    G_OBJECT(_widget),
    widget_key,
    this,
    (GDestroyNotify)BasicGtkWidget::destroy_widget_key_callback);
    
  signal_connect<BasicGtkWidget, GdkEvent*, &BasicGtkWidget::on_event>("event");
}

void BasicGtkWidget::destroy_widget_key_callback(BasicGtkWidget *_this) {
  //g_object_set_data(G_OBJECT(_this->widget()), widget_key, nullptr);
  if(!_this->destroying()) {
    _this->_widget = 0;
    _this->destroy();
  }
}

BasicGtkWidget::~BasicGtkWidget() {
  if(!_destroying) {
    assert(_destroying);
  }
  
  if(_widget) {
    g_signal_handlers_disconnect_matched(
      _widget,
      G_SIGNAL_MATCH_DATA,
      0, 0, 0, 0,
      this);
      
    if(g_object_get_data(G_OBJECT(_widget), widget_key))
      gtk_widget_destroy(_widget);
    _widget = 0;
  }
  
  add_remove_window(-1);
}

BasicGtkWidget *BasicGtkWidget::parent() {
  if(!_widget)
    return 0;
    
  GtkWidget *wid = gtk_widget_get_parent(_widget);
  
  while(wid) {
    if(auto parent = static_cast<BasicGtkWidget*>(g_object_get_data(G_OBJECT(wid), widget_key)))
      return parent;
      
    wid = gtk_widget_get_parent(wid);
  }
  
  return 0;
}

BasicGtkWidget *BasicGtkWidget::from_widget(GtkWidget *wid) {
  if(!wid)
    return 0;
    
  return (BasicGtkWidget*)g_object_get_data(G_OBJECT(wid), widget_key);
}

bool BasicGtkWidget::on_event(GdkEvent *e) {
//  pmath_debug_print("[%d]", e->type);
  return false;
}

//} ... class BasicGtkWidget
