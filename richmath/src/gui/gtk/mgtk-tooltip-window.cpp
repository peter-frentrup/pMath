#include <gui/gtk/mgtk-tooltip-window.h>
#include <gui/gtk/mgtk-control-painter.h>
#include <gui/common-tooltips.h>

#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <cmath>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#  include <gdk/gdkwayland.h>
#endif


namespace richmath {
  class MathGtkTooltipWindow::Impl {
    public:
      Impl(MathGtkTooltipWindow &self) : self{self} {}
      
      void resize(bool just_move);
      void update_shape();
      
    private:
      MathGtkTooltipWindow &self;
  };
}

using namespace richmath;

extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonFrame;

static MathGtkTooltipWindow *tooltip_window = nullptr;

//{ class MathGtkTooltipWindow ...

MathGtkTooltipWindow::MathGtkTooltipWindow()
  : base(new Document())
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void MathGtkTooltipWindow::after_construction() {
  if(!_widget)
    _widget = gtk_window_new(GTK_WINDOW_POPUP);
    
  gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_TOOLTIP);
  gtk_window_set_default_size(GTK_WINDOW(_widget), 1, 1);
  gtk_window_set_resizable(GTK_WINDOW(_widget), FALSE);
//  gtk_widget_set_events(  _widget, GDK_EXPOSURE_MASK);
  gtk_widget_set_can_focus(_widget, FALSE);
  
  document()->style.set(Editable,           false);
  document()->style.set(Selectable,         AutoBoolFalse);
  document()->style.set(ShowSectionBracket, AutoBoolFalse);
  document()->select(nullptr, 0, 0);
  
  base::after_construction();
}

MathGtkTooltipWindow::~MathGtkTooltipWindow() {
  if(this == tooltip_window)
    tooltip_window = 0;
}

void MathGtkTooltipWindow::move_global_tooltip() {
  if(!tooltip_window || !gtk_widget_get_visible(tooltip_window->_widget))
    return;
    
  Impl(*tooltip_window).resize(true);
}

void MathGtkTooltipWindow::show_global_tooltip(Box *source, Expr boxes, SharedPtr<Stylesheet> stylesheet) {
  if(!tooltip_window) {
    tooltip_window = new MathGtkTooltipWindow();
    tooltip_window->init();
  }
  
  tooltip_window->source_box(source);
  if(tooltip_window->_content_expr != boxes) {
    tooltip_window->_content_expr = boxes;
    
    CommonTooltips::load_content(
      tooltip_window->document(), 
      PMATH_CPP_MOVE(boxes), 
      PMATH_CPP_MOVE(stylesheet));
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

Vector2F MathGtkTooltipWindow::page_size() {
  Vector2F size = base::page_size();
  size.x = HUGE_VAL;
  return size;
}

int MathGtkTooltipWindow::dpi() {
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(_widget));
  double dpi = gdk_screen_get_resolution(screen);
  if(dpi <= 0)
    return 96;
  return (int)dpi;
}

void MathGtkTooltipWindow::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  best_height = (int)round(document()->extents().height() * scale_factor());
  best_width  = (int)round(document()->unfilled_width     * scale_factor());
  
  if(best_height < 1)
    best_height = 1;
    
  if(best_width < 1)
    best_width = 1;

  if(old_bw != best_width || old_bh != best_height) {
    Impl(*this).resize(false);
  }
}

//} ... class MathGtkTooltipWindow

//{ class MathGtkTooltipWindow::Impl ...

void MathGtkTooltipWindow::Impl::resize(bool just_move) {
  GdkDisplay *display = gtk_widget_get_display(self._widget);
  
  int              ix, iy;
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
  
  if(ix + cx + self.best_width > monitor_rect.x + monitor_rect.width) {
    ix -= cx + self.best_width;
    align_left = false;
    
    if(ix < monitor_rect.x)
      ix = monitor_rect.x;
  }
  else
    ix += cx;
    
  if(iy + cy + self.best_height > monitor_rect.y + monitor_rect.height) {
    iy -= cy + self.best_height;
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
    
  if(!just_move) {
    gtk_widget_set_size_request(self._widget, self.best_width, self.best_height);
    update_shape();
  }
  
  gtk_window_set_gravity(GTK_WINDOW(self._widget), gravity);
  gtk_window_move(GTK_WINDOW(self._widget), ix, iy);
}

void MathGtkTooltipWindow::Impl::update_shape() {
#if GTK_MAJOR_VERSION >= 3
  if(!gtk_widget_is_drawable(self._widget))
    return;
  
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(self._widget)))
    return;
#endif
  
  GdkWindow *win = gtk_widget_get_window(self._widget);
  
  int w = self.best_width; //gdk_window_get_width(win);
  int h = self.best_height;//gdk_window_get_height(win);

  if(w <= 1 || h <= 1)
    return;

  cairo_surface_t *surface = gdk_window_create_similar_surface(win, CAIRO_CONTENT_COLOR_ALPHA, w, h);
  
  cairo_t *cr = cairo_create(surface);
  {
    Canvas canvas(cr);
    MathGtkControlPainter::gtk_painter.draw_container(self, canvas, ContainerType::TooltipWindow, ControlState::Normal, RectangleF(0, 0, w, h));
  }
  cairo_destroy(cr);
  
  cairo_region_t *region = gdk_cairo_region_create_from_surface(surface);
  cairo_surface_destroy(surface);
  
  gtk_widget_shape_combine_region(self._widget, nullptr);
  if(!cairo_region_is_empty(region))
    gtk_widget_shape_combine_region(self._widget, region);
  cairo_region_destroy(region);
  //gdk_window_set_child_shapes(gtk_widget_get_parent_window(self._widget));
#endif
}

//} ... class MathGtkTooltipWindow::Impl
