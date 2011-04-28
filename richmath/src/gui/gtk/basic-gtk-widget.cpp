#include <gui/gtk/basic-gtk-widget.h>


using namespace richmath;

//{ gobject class MathCppWidget ...

/*#define MATH_TYPE_CPP_WIDGET            (math_cpp_widget_get_type ())
#define MATH_CPP_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MATH_TYPE_CPP_WIDGET, MathCppWidget))
#define MATH_CPP_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MATH_TYPE_CPP_WIDGET, MathCppWidgetClass))
#define MATH_IS_CPP_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MATH_TYPE_CPP_WIDGET))
#define MATH_IS_CPP_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MATH_TYPE_CPP_WIDGET))
#define MATH_CPP_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MATH_TYPE_CPP_WIDGET, MathCppWidgetClass))

typedef struct{
  GtkDrawingArea darea;
} MathCppWidget;

typedef struct{
  GtkDrawingAreaClass parent_class;
  
  void (*set_scroll_adjustments)(MathCppWidget *cppwid, 
                                 GtkAdjustment *hadjustment, 
                                 GtkAdjustment *vadjustment);
} MathCppWidgetClass;

GType      math_cpp_widget_get_type(void) G_GNUC_CONST;
GtkWidget* math_cpp_widget_new     (void);

static void math_cpp_widget_class_init(MathCppWidgetClass *klass);
static void math_cpp_widget_init      (MathCppWidget      *widget);

GType math_cpp_widget_get_type(void){
  static GType cpp_widget_type = 0;

  if(!cpp_widget_type){
    static const GTypeInfo cpp_widget_info = {
      sizeof(MathCppWidgetClass),
      NULL,               // base_init
      NULL,               // base_finalize
      (GClassInitFunc)    math_cpp_widget_class_init,
      NULL,               // class_finalize
      NULL,               // class_data
      sizeof(MathCppWidget),
      0,                  // n_preallocs
      (GInstanceInitFunc) math_cpp_widget_init,
    };

    cpp_widget_type = g_type_register_static(
      GTK_TYPE_DRAWING_AREA, "MathCppWidget", &cpp_widget_info, (GTypeFlags)0);
  }

  return cpp_widget_type;
}

static void math_cpp_widget_class_init(MathCppWidgetClass *klass){
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  
  klass->set_scroll_adjustments = math_cpp_widget_set_scroll_adjustments;
}

static void math_cpp_widget_init(MathCppWidget *widget){
}

GtkWidget *math_cpp_widget_new(void){
  return (GtkWidget*)g_object_new(MATH_TYPE_CPP_WIDGET, NULL);
}*/

//} ... gobject class MathCppWidget

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
    _widget = gtk_drawing_area_new();//math_cpp_widget_new();//
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
  
  GtkWidget *wid = gtk_widget_get_parent(_widget);
  
  while(wid){
    BasicGtkWidget *parent = (BasicGtkWidget*)g_object_get_data(G_OBJECT(wid), widget_key);
    
    if(parent)
      return parent;
    
    wid = gtk_widget_get_parent(wid);
  }
  
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
