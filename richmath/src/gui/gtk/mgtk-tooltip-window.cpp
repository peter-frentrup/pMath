#include <gui/gtk/mgtk-tooltip-window.h>

#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <cmath>


using namespace richmath;

static MathGtkTooltipWindow *tooltip_window = 0;

//{ class MathGtkTooltipWindow ...

MathGtkTooltipWindow::MathGtkTooltipWindow()
  : MathGtkWidget(new Document())
{
}

void MathGtkTooltipWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_POPUP);
    
  gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_TOOLTIP);
  gtk_window_set_default_size(GTK_WINDOW(_widget), 1, 1);
  gtk_window_set_resizable(GTK_WINDOW(_widget), FALSE);
//  gtk_widget_set_events(  _widget, GDK_EXPOSURE_MASK);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  if(!document()->style)
    document()->style = new Style;
  document()->style->set(Editable,           false);
  document()->style->set(Selectable,         false);
  document()->style->set(ShowSectionBracket, false);
  document()->select(0, 0, 0);
  
  MathGtkWidget::after_construction();
}

MathGtkTooltipWindow::~MathGtkTooltipWindow() {
  if(this == tooltip_window)
    tooltip_window = 0;
}

void MathGtkTooltipWindow::move_global_tooltip() {
  if(!tooltip_window || !gtk_widget_get_visible(tooltip_window->_widget))
    return;
    
  tooltip_window->resize(true);
}

void MathGtkTooltipWindow::show_global_tooltip(Expr boxes) {
  if(!tooltip_window) {
    tooltip_window = new MathGtkTooltipWindow();
    tooltip_window->init();
  }
  
  if(tooltip_window->_content_expr != boxes) {
    tooltip_window->_content_expr = boxes;
    
    Document *doc = tooltip_window->document();
    doc->remove(0, doc->length());
    
    Style *style = new Style;
    style->set(BaseStyleName,       "ControlStyle");
    style->set(SectionMarginLeft,   0);
    style->set(SectionMarginTop,    0);
    style->set(SectionMarginRight,  0);
    style->set(SectionMarginBottom, 0);
    
    boxes = Call(Symbol(PMATH_SYMBOL_BUTTONBOX),
                 boxes,
                 Rule(Symbol(PMATH_SYMBOL_BUTTONFRAME),
                      String("TooltipWindow")));
                      
    MathSection *section = new MathSection(style);
    section->content()->load_from_object(boxes, BoxOptionFormatNumbers);
    doc->insert(0, section);
  }
  
  if(!gtk_widget_get_visible(tooltip_window->widget()))
    gtk_widget_show(tooltip_window->widget());
    
  move_global_tooltip();
}

void MathGtkTooltipWindow::hide_global_tooltip() {
  if(tooltip_window) {
    gtk_widget_hide(tooltip_window->widget());
    tooltip_window->_content_expr = Expr(PMATH_UNDEFINED);
  }
}

void MathGtkTooltipWindow::delete_global_tooltip() {
  if(tooltip_window)
    tooltip_window->destroy();
}

void MathGtkTooltipWindow::page_size(float *w, float *h) {
  MathGtkWidget::page_size(w, h);
  *w = HUGE_VAL;
}

void MathGtkTooltipWindow::resize(bool just_move) {
  GdkDisplay *display = gtk_widget_get_display(_widget);
  
  gint             ix, iy;
  GdkScreen       *screen;
  GdkModifierType  mod;
  gdk_display_get_pointer(display, &screen, &ix, &iy, &mod);
  
  int monitor = gdk_screen_get_monitor_at_point(screen, ix, iy);
  
  GdkRectangle monitor_rect;
  gdk_screen_get_monitor_geometry(screen, monitor, &monitor_rect);
  
  /* Ubuntu 11.04: When there is a screen update near the mouse (radius ~25px), it is hidden :-(
     Otherwise we would use a value of 16 or better the actual cursor_size
     I don't know where that value comes from or where one could set it.
  */
  const int cx = 26;
  const int cy = 26;
  
  bool align_left = true;
  bool align_top  = true;
  
  if(ix + cx + best_width > monitor_rect.x + monitor_rect.width) {
    ix -= cx + best_width;
    align_left = false;
    
    if(ix < monitor_rect.x)
      ix = monitor_rect.x;
  }
  else
    ix += cx;
    
  if(iy + cy + best_height > monitor_rect.y + monitor_rect.height) {
    iy -= cy + best_height;
    align_top = false;
    
    if(iy < monitor_rect.y)
      iy = monitor_rect.y;
  }
  else
    iy += cy;
    
  GdkGravity gravity;
  if(align_left) {
    if(align_top)
      gravity = GDK_GRAVITY_NORTH_WEST;
    else
      gravity = GDK_GRAVITY_SOUTH_WEST;
  }
  else if(align_top)
    gravity = GDK_GRAVITY_NORTH_EAST;
  else
    gravity = GDK_GRAVITY_SOUTH_EAST;
    
  if(!just_move)
    gtk_widget_set_size_request(_widget, best_width, best_height);
    
  gtk_window_set_gravity(GTK_WINDOW(_widget), gravity);
  gtk_window_move(GTK_WINDOW(_widget), ix, iy);
}

void MathGtkTooltipWindow::paint_canvas(Canvas *canvas, bool resize_only) {
  MathGtkWidget::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  best_height = (int)floorf(document()->extents().height() * scale_factor() + 0.5);
  best_width  = (int)floorf(document()->unfilled_width     * scale_factor() + 0.5);
  
  if(best_height < 1)
    best_height = 1;
    
  if(best_width < 1)
    best_width = 1;
    
  if(old_bw != best_width || old_bh != best_height) {
    resize(false);
  }
}

//} ... class MathGtkTooltipWindow
