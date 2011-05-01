#include <gui/gtk/mgtk-document-window.h>

#include <boxes/section.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/gtk/mgtk-menu-builder.h>


using namespace richmath;

class richmath::MathGtkWorkingArea: public MathGtkWidget{
  friend class MathGtkDocumentWindow;
  public:
    MathGtkWorkingArea(MathGtkDocumentWindow *parent)
    : MathGtkWidget(new Document()),
      _parent(parent)
    {
    }
    
    MathGtkDocumentWindow *parent(){ return _parent; }
    
    virtual void close(){
      if(_parent){
        MathGtkDocumentWindow *p = _parent;
        _parent = 0;
        delete p;
      }
    }
    
    virtual void running_state_changed(){
      _parent->title(_parent->title());
    }
    
    int width(){
      int h = (int)(document()->extents().width * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
    int height(){
      int h = (int)(document()->extents().height() * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
  protected:
    void rearrange(){
      if(_parent->is_palette()){
        GtkAllocation rect;
        gtk_widget_get_allocation(_widget, &rect);
        
        int h = height();
        int w = width();
        if(h != rect.height || w != rect.width){
          gtk_widget_set_size_request(_widget, w, h);
        }
      }
    }
    
    virtual bool on_expose(GdkEvent *e){
      bool result = MathGtkWidget::on_expose(e);
      rearrange();
      return result;
    }

  private:
    MathGtkDocumentWindow *_parent;
};

class richmath::MathGtkDock: public MathGtkWidget{
  friend class MathGtkDocumentWindow;
  public:
    MathGtkDock(MathGtkDocumentWindow *parent)
    : MathGtkWidget(new Document()),
      _parent(parent)
    {
    }
    
    MathGtkDocumentWindow *parent(){ return _parent; }
    
    virtual bool is_scrollable(){ return false; }
    
    virtual void invalidate(){
      if(document()->length() > 0){
        gtk_widget_set_size_request(_widget, 1, 1);
        if(!gtk_widget_get_visible(_widget))
          gtk_widget_set_visible(_widget, TRUE);
      }
      else{
        gtk_widget_set_size_request(_widget, -1, -1);
        gtk_widget_hide(_widget);
      }
      
      MathGtkWidget::invalidate();
    }
    
    virtual void close(){
      if(_parent){
        MathGtkDocumentWindow *p = _parent;
        _parent = 0;
        delete p;
      }
    }
    
    int height(){
      int h = (int)(document()->extents().height() * scale_factor() + 0.5f);
      if(h < 1)
        return 1;
      return h;
    }
    
    virtual int min_width(){
      return (int)(document()->unfilled_width * scale_factor() + 0.5f);
    }
    
    virtual void running_state_changed(){
      _parent->title(_parent->title());
    }
    
  protected:
    virtual void after_construction(){
      MathGtkWidget::after_construction();
      
      if(!document()->style)
        document()->style = new Style();
      
      document()->style->set(Selectable, false);
      
      document()->select(0,0,0);
      document()->border_visible = false;
    }
    
    virtual void paint_background(Canvas *canvas){
      canvas->set_color(0x808080);
      canvas->paint();
    }
    
    void rearrange(){
      GtkAllocation rect;
      gtk_widget_get_allocation(_widget, &rect);
      
      int h = height();
      if(h != rect.height){
        gtk_widget_set_size_request(_widget, -1, h);
      }
    }
    
    virtual bool on_expose(GdkEvent *e){
      bool result = MathGtkWidget::on_expose(e);
      rearrange();
      return result;
    }

  private:
    MathGtkDocumentWindow *_parent;
};

//{ class MathGtkDocumentWindow ...

  static void adjustment_changed_callback(
    GtkAdjustment *adjustment,
    void          *user_data
  ){
    MathGtkDocumentWindow *self = (MathGtkDocumentWindow*)user_data;
    
    self->adjustment_changed(adjustment);
  }

static MathGtkDocumentWindow *_first_window = NULL;

MathGtkDocumentWindow::MathGtkDocumentWindow()
: BasicGtkWidget(),
  _is_palette(false),
  _menu_bar(0),
  _hadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0,0,0,0))),
  _vadjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0,0,0,0))),
  _hscrollbar(0),
  _vscrollbar(0),
  _table(0)
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
  
  GtkAccelGroup *accel_group = gtk_accel_group_new();
  
  _menu_bar = gtk_menu_bar_new();
  MathGtkMenuBuilder::main_menu.append_to(GTK_MENU_SHELL(_menu_bar), accel_group, document()->id());
  
  gtk_window_add_accel_group(GTK_WINDOW(_widget), accel_group);
  
  _table = gtk_table_new(2, 5, FALSE);
  gtk_container_add(GTK_CONTAINER(_widget), _table);
  
  gtk_table_attach(GTK_TABLE(_table), _menu_bar,               0, 2, 0, 1, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  gtk_table_attach(GTK_TABLE(_table), _top_area->widget(),     0, 2, 1, 2, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  gtk_table_attach(GTK_TABLE(_table), _working_area->widget(), 0, 1, 2, 3, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 0, 0);
  gtk_table_attach(GTK_TABLE(_table), _bottom_area->widget(),  0, 2, 4, 5, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  
  g_object_ref(_hadjustment); 
  g_object_ref(_vadjustment);
  _hscrollbar = gtk_hscrollbar_new(_hadjustment);
  _vscrollbar = gtk_vscrollbar_new(_vadjustment);
  gtk_table_attach(GTK_TABLE(_table), _vscrollbar, 1, 2, 2, 3, (GtkAttachOptions)0,                                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), 0, 0);
  gtk_table_attach(GTK_TABLE(_table), _hscrollbar, 0, 1, 3, 4, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL | GTK_SHRINK), (GtkAttachOptions)0,                                    0, 0);
  
  g_signal_connect(_hadjustment, "changed", G_CALLBACK(adjustment_changed_callback), this);
  g_signal_connect(_vadjustment, "changed", G_CALLBACK(adjustment_changed_callback), this);
  
  g_object_ref(_hadjustment); 
  g_object_ref(_vadjustment);
  _working_area->hadjustment(_hadjustment);
  _working_area->vadjustment(_vadjustment);
  
  gtk_widget_show_all(_table);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  GList *focus_chain = NULL;
  focus_chain = g_list_prepend(focus_chain, _working_area->widget());
  gtk_container_set_focus_chain(GTK_CONTAINER(_table), focus_chain);
  g_list_free(focus_chain);
  
  gtk_window_set_default_size(GTK_WINDOW(_widget), 500, 550);
  
  top()->invalidate();
  bottom()->invalidate();
  
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
  
  g_object_unref(_hadjustment);
  g_object_unref(_vadjustment);
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
  
  gtk_widget_set_visible(_menu_bar, !value);
  
  _working_area->_autohide_vertical_scrollbar = value;
  _working_area->invalidate();
  
  if(value)
    gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_UTILITY);
  else
    gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_NORMAL);
}

void MathGtkDocumentWindow::run_menucommand(Expr cmd){
  String cmd_str(cmd);
  
  if(cmd_str.starts_with(         "@shaper=")){
    cmd_str = cmd_str.part(sizeof("@shaper=") - 1, -1);
    
    SharedPtr<MathShaper> ms = MathShaper::available_shapers[cmd];
    
    if(ms.is_valid()){
      _top_area->document_context()->math_shaper     = ms;
      _bottom_area->document_context()->math_shaper  = ms;
      _working_area->document_context()->math_shaper = ms;
      
      _top_area->document()->invalidate_all();
      _bottom_area->document()->invalidate_all();
      _working_area->document()->invalidate_all();
    }
    else
      MessageBeep(0);
  }
  else    
    Application::run_menucommand(cmd);
}

void MathGtkDocumentWindow::adjustment_changed(GtkAdjustment *adjustment){
  GtkWidget *scrollbar = 0;
  if(adjustment == _hadjustment)
    scrollbar = _hscrollbar;
  else if(adjustment == _vadjustment)
    scrollbar = _vscrollbar;
  
  if(!scrollbar)
    return;
    
  double lo, page, hi;
  g_object_get(adjustment,
    "lower",     &lo,
    "page-size", &page,
    "upper",     &hi,
    NULL);
  
  gtk_widget_set_visible(scrollbar, lo + page < hi);
}

MathGtkDocumentWindow *MathGtkDocumentWindow::first_window(){
  return _first_window;
}

//} ... class MathGtkDocumentWindow
