#include <gui/gtk/mgtk-control-painter.h>

#include <eval/observable.h>
#include <util/style.h>
#include <graphics/shapers.h>

#include <algorithm>
#include <cmath>

/* FIXME: GtkStyleContext API changed several times until 3.20. 
   
   To work around this, we should extract each GtkStyleContext from a corresponding GtkWidget, like Firefox does nowadays.
   See relevant bug https://bugzilla.mozilla.org/show_bug.cgi?id=1234158
   and source code https://searchfox.org/mozilla-central/source/widget/gtk/WidgetStyleCache.cpp
   
   On the other hand, LibreOffice seems to construct the GtkStyleContext themselves, 
   see https://cgit.freedesktop.org/libreoffice/core/tree/vcl/unx/gtk3/gtk3salnativewidgets-gtk.cxx
   
   Note that the GTK devs seem to abandon foreign drawing,
   see "stylecontext: Remove gtk_style_context_new()" commit (https://gitlab.gnome.org/GNOME/gtk/-/commit/31713ab5ef473fec3d90d925a5fbb2ceeb246e3b).
 */


using namespace richmath;

static Observable style_observations;

#if GTK_MAJOR_VERSION >= 3
static class MathGtkStyleContextCache {
  public:
    MathGtkStyleContextCache();
    ~MathGtkStyleContextCache();
    void clear();
    
    GtkStyleContext *checkbox_context() {             return init_once(_checkbox_context,             make_checkbox_context); }
    GtkStyleContext *default_push_button_context() {  return init_once(_default_push_button_context,  make_default_push_button_context); }
    GtkStyleContext *expander_arrow_context() {       return init_once(_expander_arrow_context,       make_expander_arrow_context); }
    GtkStyleContext *input_field_button_context() {   return init_once(_input_field_button_context,   make_input_field_button_context); }
    GtkStyleContext *input_field_context() {          return init_once(_input_field_context,          make_input_field_context); }
    GtkStyleContext *list_item_context() {            return init_once(_list_item_context,            make_list_item_context); }
    GtkStyleContext *list_item_selected_context() {   return init_once(_list_item_selected_context,   make_list_item_selected_context); }
    GtkStyleContext *panel_context() {                return init_once(_panel_context,                make_panel_context); }
    GtkStyleContext *progress_bar_context() {         return init_once(_progress_bar_context,         make_progress_bar_context); }
    GtkStyleContext *progress_bar_trough_context() {  return init_once(_progress_bar_trough_context,  make_progress_bar_trough_context); }
    GtkStyleContext *push_button_context() {          return init_once(_push_button_context,          make_push_button_context); }
    GtkStyleContext *radio_button_context() {         return init_once(_radio_button_context,         make_radio_button_context); }
    GtkStyleContext *slider_channel_context() {       return init_once(_slider_channel_context,       make_slider_channel_context); }
    GtkStyleContext *slider_thumb_context() {         return init_once(_slider_thumb_context,         make_slider_thumb_context); }
    GtkStyleContext *tab_body_context() {             return init_once(_tab_body_context,             make_tab_body_context); }
    GtkStyleContext *tab_head_background_context() {  return init_once(_tab_head_background_context,  make_tab_head_background_context); }
    GtkStyleContext *tab_head_context() {             return init_once(_tab_head_context,             []() { return make_tab_head_context(false, false); }); }
    GtkStyleContext *tab_head_label_context() {       return init_once(_tab_head_label_context,       make_tab_head_label_context); }
    GtkStyleContext *tool_button_context() {          return init_once(_tool_button_context,          make_tool_button_context); }
    GtkStyleContext *tooltip_context() {              return init_once(_tooltip_context,              make_tooltip_context); }
    
    static void render_all_common(GtkStyleContext *ctx, Canvas *canvas, float x, float y, float width, float height);
    
    static GtkStyleContext *make_context_from_path_and_free(GtkWidgetPath *path, GtkStyleContext *parent = nullptr);
    
  private:
    static GtkStyleContext *make_checkbox_context();
    static GtkStyleContext *make_default_push_button_context();
    static GtkStyleContext *make_expander_arrow_context();
    static GtkStyleContext *make_input_field_button_context();
    static GtkStyleContext *make_input_field_context();
    static GtkStyleContext *make_list_item_context();
    static GtkStyleContext *make_list_item_selected_context();
    static GtkStyleContext *make_panel_context();
    static GtkStyleContext *make_progress_bar_context();
    static GtkStyleContext *make_progress_bar_trough_context();
    static GtkStyleContext *make_push_button_context();
    static GtkStyleContext *make_radio_button_context();
    static GtkStyleContext *make_slider_channel_context();
    static GtkStyleContext *make_slider_thumb_context();
    static GtkStyleContext *make_tab_body_context();
    static GtkStyleContext *make_tab_head_background_context();
    static GtkStyleContext *make_tab_head_context(bool has_left_sibling, bool has_right_sibling);
    static GtkStyleContext *make_tab_head_label_context();
    static GtkStyleContext *make_tool_button_context();
    static GtkStyleContext *make_tooltip_context();
    
    template<typename Init>
    static GtkStyleContext *init_once(GtkStyleContext *&obj, Init init) {
      if(!obj)
        obj = init();
      return obj;
    }
    
    void unref_and_null(GtkStyleContext *&style_context);
    
  private:
    GtkStyleContext *_checkbox_context;
    GtkStyleContext *_default_push_button_context;
    GtkStyleContext *_expander_arrow_context;
    GtkStyleContext *_input_field_button_context;
    GtkStyleContext *_input_field_context;
    GtkStyleContext *_list_item_context;
    GtkStyleContext *_list_item_selected_context;
    GtkStyleContext *_panel_context;
    GtkStyleContext *_progress_bar_context;
    GtkStyleContext *_progress_bar_trough_context;
    GtkStyleContext *_push_button_context;
    GtkStyleContext *_radio_button_context;
    GtkStyleContext *_slider_channel_context;
    GtkStyleContext *_slider_thumb_context;
    GtkStyleContext *_tab_body_context;
    GtkStyleContext *_tab_head_background_context;
    GtkStyleContext *_tab_head_context;
    GtkStyleContext *_tab_head_label_context;
    GtkStyleContext *_tool_button_context;
    GtkStyleContext *_tooltip_context;
    
} mgtk_painter_cache;
  
static bool initialized_change_notifications = false;
static void on_theme_changed(GObject*, GParamSpec*) {
  MathGtkControlPainter::gtk_painter.clear_cache();
  style_observations.notify_all();
}
#endif

//{ class MathGtkControlPainter ...

MathGtkControlPainter MathGtkControlPainter::gtk_painter;

void MathGtkControlPainter::done() {
#if GTK_MAJOR_VERSION >= 3
  gtk_painter.clear_cache();
#endif
  style_observations.notify_all(); // to clear the observers array
}

MathGtkControlPainter::MathGtkControlPainter()
  : ControlPainter()
{
  ControlPainter::std = this;
}

MathGtkControlPainter::~MathGtkControlPainter() {
  if(ControlPainter::std == this)
    ControlPainter::std = &ControlPainter::generic_painter;
    
#if GTK_MAJOR_VERSION >= 3
  clear_cache();
#endif
}

#if GTK_MAJOR_VERSION >= 3

void MathGtkControlPainter::calc_container_size(
  ControlContext *context,
  Canvas         *canvas,
  ContainerType   type,
  BoxSize        *extents
) {
  if(GtkStyleContext *gtk_ctx = get_control_theme(context, type)) {
    int min_width = 0;
    int min_height = 0;
    
    switch(type) {
      case DefaultPushButton:
      case PushButton:
      case PaletteButton:
        if(extents->ascent < canvas->get_font_size() * 0.75f)
          extents->ascent = canvas->get_font_size() * 0.75f;
          
        if(extents->descent < canvas->get_font_size() * 0.25f)
          extents->descent = canvas->get_font_size() * 0.25f;
        break;
      
      case AddressBandInputField: {
          GtkBorder border;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
          extents->ascent +=  0.75f * border.top;
          extents->descent += 0.75f * border.bottom;
          extents->width +=   0.75f * (border.left + border.right);
          
          //gtk_style_context_get_border(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
          //extents->ascent +=  0.75f * border.top;
          //extents->descent += 0.75f * border.bottom;
          //extents->width +=   0.75f * (border.left + border.right);
        } 
        return;
        
      case AddressBandBackground: {
          GtkBorder border;
          //gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
          //extents->ascent +=  0.75f * border.top;
          //extents->descent += 0.75f * border.bottom;
          //extents->width +=   0.75f * (border.left + border.right);
          
          gtk_style_context_get_border(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
          extents->ascent +=  0.75f * border.top;
          extents->descent += 0.75f * border.bottom;
          extents->width +=   0.75f * (border.left + border.right);
        } 
        return;
        
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate:
      case RadioButtonUnchecked:
      case RadioButtonChecked: {
//          int size;
//          gtk_style_context_get_style(gtk_ctx, "indicator-size", &size, nullptr);
//          
//          extents->width   = 0.75f * size;
//          extents->ascent  = 0.75f * extents->width;
//          extents->descent = 0.25f * extents->width;
//          return;
          extents->width = 0;
          extents->ascent = 0;
          extents->descent = 0;
        } break;
      
      case OpenerTriangleClosed:
      case OpenerTriangleOpened: {
          int size;
          gtk_style_context_get_style(gtk_ctx, "expander-size", &size, nullptr);
          
          extents->width   = 0.75f * size;
          extents->ascent  = 0.75f * extents->width;
          extents->descent = 0.25f * extents->width;
        }
        return;
      
      case PanelControl: {
          extents->width +=   12.0;
          extents->ascent +=  6.0;
          extents->descent += 6.0;
        }
        return;
      
      case SliderHorzChannel: {
          // TODO: calculate implicitly from minimum scale>contents>trough>slider height (18+2 for border), taking margins (-9 top and bottom) into account and add trough border (1 top and bottom)
          float h = 4 * 0.75f;
          extents->ascent  = h * 0.75;
          extents->descent = h * 0.25;
        }
        return;
        
      case SliderHorzThumb: {
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-width", &min_width, nullptr); // GTK >= 3.20.0
          if(min_width <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-width", &min_width, nullptr);
          
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-height", &min_height, nullptr); // GTK >= 3.20.0
          if(min_height <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-length", &min_height, nullptr);
          
          extents->ascent  = min_height * 0.75f * 0.75f;
          extents->descent = min_height * 0.75f * 0.25f;
          extents->width  = min_width * 0.75f;
        }
        break;
        
//      case ProgressIndicatorBackground: {
//          //extents->ascent *= 0.5;
//          //extents->descent *= 0.5;
//          
//          int h = 6;
//          gtk_style_context_get_style(gtk_ctx, "min-horizontal-bar-height", &h, nullptr);
//          
//          GtkBorder padding;
//          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
//          h+= padding.top + padding.bottom;
//          
//          extents->ascent = 0.75 * h;
//          extents->descent = 0;
//          extents->width = extents->height() * 15;
//        }
//        return;
        
      case ProgressIndicatorBar: {
          GtkBorder padding;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
          
          extents->ascent -=  padding.top    * 0.75;
          extents->descent -= padding.bottom * 0.75;
          extents->width -=   (padding.left + padding.right) * 0.75;
        }
        return;
        
      case NavigationBack:
      case NavigationForward:  {
          int w, h;
          gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
          
          extents->width   = std::max(w * 0.75,        (double)extents->width);
          extents->ascent  = std::max(h * 0.75 * 0.75, (double)extents->ascent);
          extents->descent = std::max(h * 0.25 * 0.75, (double)extents->descent);
        } 
        break;
      
      case TabHeadBackground: return;
      
      default:
        break;
    }
    
    gtk_style_context_get(
      gtk_ctx, GTK_STATE_FLAG_NORMAL, 
      "min-width",  &min_width, 
      "min-height", &min_height, 
      nullptr); // GTK >= 3.20.0
    
    extents->ascent  = std::max(extents->ascent,  min_height * 0.75f * 0.5f + 0.25f * canvas->get_font_size());
    extents->descent = std::max(extents->descent, min_height * 0.75f * 0.5f - 0.25f * canvas->get_font_size());
    extents->width   = std::max(extents->width,   min_width * 0.75f);
    
    GtkBorder border;
    gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
    extents->ascent +=  0.75f * border.top;
    extents->descent += 0.75f * border.bottom;
    extents->width +=   0.75f * (border.left + border.right);
    
    gtk_style_context_get_border(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
    extents->ascent +=  0.75f * border.top;
    extents->descent += 0.75f * border.bottom;
    extents->width +=   0.75f * (border.left + border.right);
    
    return;
  }
  
  ControlPainter::calc_container_size(context, canvas, type, extents);
}
 
Color MathGtkControlPainter::control_font_color(ControlContext *context, ContainerType type, ControlState state) {
  if(GtkStyleContext *gsc = get_control_theme(context, type, true)) {
    GtkStateFlags old_state = gtk_style_context_get_state(gsc);
    GtkStateFlags new_state = get_state_flags(context, type, state);
    if(old_state != new_state)
      gtk_style_context_set_state(gsc, new_state);
    
    GdkRGBA color = {0,0,0,0};
    gtk_style_context_get_color(gsc, new_state, &color);
    
    if(old_state != new_state)
      gtk_style_context_set_state(gsc, old_state);
    
    if(color.alpha == 0)
      return Color::None;
      
    // ignoring color.alpha intensity
    return Color::from_rgb(color.red, color.green, color.blue);
  }
  
  return ControlPainter::control_font_color(context, type, state);
}
 
bool MathGtkControlPainter::is_very_transparent(ControlContext *context, ContainerType type, ControlState state) {
  switch(type) {
    case PaletteButton:
      return state == Normal;
      
    default:
      break;
  }
  return ControlPainter::is_very_transparent(context, type, state);
}
 
void MathGtkControlPainter::draw_container(
  ControlContext *context, 
  Canvas         *canvas,
  ContainerType   type,
  ControlState    state,
  float           x,
  float           y,
  float           width,
  float           height
) {
  switch(type) {
    case PaletteButton:
      if(state == Normal)
        return;
      break;
    
    case AddressBandInputField: return;
    
    default: break;
  }
  
  if(GtkStyleContext *gsc = get_control_theme(context, type)) {
    canvas->save();
    if(canvas->pixel_device) {
      canvas->align_point(&x, &y, false);
      canvas->user_to_device_dist(&width, &height);
      width  = floor(width  + 0.5);
      height = floor(height + 0.5);
      canvas->device_to_user_dist(&width, &height);
    }
    
    canvas->translate(x, y);
    x = y = 0;
    canvas->scale(0.75, 0.75);
    width/= 0.75;
    height/= 0.75;

    canvas->move_to(x, y);
    canvas->rel_line_to(width, 0);
    canvas->rel_line_to(0, height);
    canvas->rel_line_to(-width, 0);
    canvas->close_path();
    canvas->clip();
    
    //gtk_style_context_save(gsc); // changes the widget path and thus does not always work (e.g. with Adwaita notebook tabs)
    GtkStateFlags flags = get_state_flags(context, type, state);
    gtk_style_context_set_state(gsc, flags);
    
    switch(type) {
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate:
        MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y, width, height);
        gtk_render_check(gsc, canvas->cairo(), x, y, width, height);
        break;
        
      case RadioButtonUnchecked:
      case RadioButtonChecked:
        MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y, width, height);
        gtk_render_option(gsc, canvas->cairo(), x, y, width, height);
        break;
        
      case OpenerTriangleClosed:
      case OpenerTriangleOpened:
        //gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
        gtk_render_expander(  gsc, canvas->cairo(), x, y, width, height);
        break;
      
      case NavigationBack:
      case NavigationForward: {
          float cx = x + width/2;
          float cy = y + height/2;
          width = height = std::min(width, height);
          x = cx - width/2;
          y = cy - height/2;
        
          canvas->align_point(&x, &y, false);
          
          MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y, width, height);
          
          GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
          int w, h;
          gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
          int icon_size = std::min(w, h);
          
          GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(icon_theme, type == NavigationBack ? "go-previous-symbolic" : "go-next-symbolic", icon_size, (GtkIconLookupFlags)0);
          GdkPixbuf *pixbuf = gtk_icon_info_load_symbolic_for_context(icon_info, gsc, nullptr, nullptr);
          g_object_unref(icon_info);
          
          canvas->save();
          gtk_render_icon(gsc, canvas->cairo(), pixbuf, cx - w * 0.5, cy - h * 0.5);
          canvas->restore();
          gdk_pixbuf_unref(pixbuf);
          
//          if(GtkIconSet *icon = gtk_icon_factory_lookup_default(type == NavigationBack ? "go-previous" : "go-next")) {
//            GdkPixbuf *pixbuf = gtk_icon_set_render_icon_pixbuf(icon, gsc, GTK_ICON_SIZE_SMALL_TOOLBAR);
//            
//            gtk_render_icon(gsc, canvas->cairo(), pixbuf, cx - w, cy - h);
//            
//            gdk_pixbuf_unref(pixbuf);
//          }
        } break;
      
      case TabBodyBackground: {
          GtkBorder border;
          gtk_style_context_get_border(gsc, GTK_STATE_FLAG_NORMAL, &border);
          
          float top_hide = border.top + canvas->get_font_size() / 0.75; // add font size for possible border radius
          MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y - top_hide, width, height + top_hide);
        } break;
      
      case TabHead:
      case TabHeadAbuttingRight:
      case TabHeadAbuttingLeftRight:
      case TabHeadAbuttingLeft: {
          GtkBorder margin;
          gtk_style_context_get_margin(gsc, flags, &margin);
          
          //x-=      margin.left;
          y+=      margin.top;
          height-= margin.top;
          //width+=  margin.left + margin.right;
          
          MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y, width, height);
        } break;
      
      default:
        MathGtkStyleContextCache::render_all_common(gsc, canvas, x, y, width, height);
        break;
    }
    
    //gtk_style_context_restore(gsc); 
    canvas->restore();
    return;
  }
  
  ControlPainter::draw_container(context, canvas, type, state, x, y, width, height);
}
 
void MathGtkControlPainter::container_content_move(
  ControlContext *context, 
  ContainerType   type,
  ControlState    state,
  float          *x,
  float          *y)
{
  switch(type) {
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
      if(state == PressedHovered) {
        if(GtkStyleContext *context = get_control_theme(ControlContext::dummy, type)) {
          int dx, dy;
          
          gtk_style_context_get_style(context,
                                      "child-displacement-x", &dx,
                                      "child-displacement-y", &dy,
                                      nullptr);
                                      
          *x += 0.75f * dx;
          *y += 0.75f * dy;
          
          return;
        }
      }
      break;
      
    default: break;
  }
  
  ControlPainter::container_content_move(context, type, state, x, y);
}
 
bool MathGtkControlPainter::container_hover_repaint(ControlContext *context, ContainerType type) {
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
    case TooltipWindow:
    case PanelControl:
    case TabHeadBackground:
    case TabBodyBackground:
      return false;
    
    default:
      if(get_control_theme(context, type))
        return true;
      break;
  }
  
  return ControlPainter::container_hover_repaint(context, type);
}
 
void MathGtkControlPainter::system_font_style(ControlContext *context, Style *style) {
  GtkStyleContext *gsc = get_control_theme(context, PushButton, true);
  
  if(!gsc) {
    ControlPainter::system_font_style(context, style);
    return;
  }
  
  const PangoFontDescription *desc = gtk_style_context_get_font(gsc, GTK_STATE_FLAG_NORMAL);
  const char *family = pango_font_description_get_family(desc);
  if(family)
    style->set(FontFamilies, String::FromUtf8(family));
    
  PangoWeight weight = pango_font_description_get_weight(desc);
  if(weight >= PANGO_WEIGHT_BOLD)
    style->set(FontWeight, FontWeightBold);
  else
    style->set(FontWeight, FontWeightPlain);
    
  PangoStyle slant = pango_font_description_get_style(desc);
  if(slant == PANGO_STYLE_NORMAL)
    style->set(FontSlant, FontSlantPlain);
  else
    style->set(FontSlant, FontSlantItalic);
    
  double size = pango_units_to_double(pango_font_description_get_size(desc));
  if(pango_font_description_get_size_is_absolute(desc))
    size *= 0.75;
    
  if(size > 0)
    style->set(FontSize, size);
}
 
GtkStyleContext *MathGtkControlPainter::get_control_theme(ControlContext *context, ContainerType type, bool foreground) {
  if(gtk_check_version(3, 20, 0) != nullptr) { // older than GTK 3.20
    return nullptr;
  }
  
  if(!initialized_change_notifications) {
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect(settings, "notify::gtk-theme-name",   G_CALLBACK(on_theme_changed), 0);
    g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(on_theme_changed), 0);
    initialized_change_notifications = true;
  }
  
  style_observations.register_observer();
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      break;
      
    case PushButton:                  return mgtk_painter_cache.push_button_context();
    case DefaultPushButton:           return mgtk_painter_cache.default_push_button_context();
      
    case NavigationBack:
    case NavigationForward:
    case PaletteButton:               return mgtk_painter_cache.tool_button_context();
    
    case AddressBandGoButton:         return mgtk_painter_cache.input_field_button_context();
      
    case InputField:
    case AddressBandInputField:
    case AddressBandBackground:       return mgtk_painter_cache.input_field_context();
      
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:       return mgtk_painter_cache.checkbox_context();
      
    case RadioButtonUnchecked:
    case RadioButtonChecked:          return mgtk_painter_cache.radio_button_context();
    
    case PanelControl:                return mgtk_painter_cache.panel_context();
    case ProgressIndicatorBackground: return mgtk_painter_cache.progress_bar_trough_context();
    case ProgressIndicatorBar:        return mgtk_painter_cache.progress_bar_context();
    case SliderHorzChannel:           return mgtk_painter_cache.slider_channel_context();
    case SliderHorzThumb:             return mgtk_painter_cache.slider_thumb_context();
    case TooltipWindow:               return mgtk_painter_cache.tooltip_context();
    case ListViewItem:                return mgtk_painter_cache.list_item_context();
    case ListViewItemSelected:        return mgtk_painter_cache.list_item_selected_context();
    
    case OpenerTriangleClosed:
    case OpenerTriangleOpened:        return mgtk_painter_cache.expander_arrow_context();
    
    case TabBodyBackground:           return mgtk_painter_cache.tab_body_context();
    case TabHeadBackground:           return foreground ? mgtk_painter_cache.tab_head_label_context() : mgtk_painter_cache.tab_head_background_context();
    
    case TabHead:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:         return foreground ? mgtk_painter_cache.tab_head_label_context() : mgtk_painter_cache.tab_head_context();
    
    default:
      break;
  }
  
  return nullptr;
}
 
GtkStateFlags MathGtkControlPainter::get_state_flags(ControlContext *context, ContainerType type, ControlState state) {
  int result = 0;
  if(!context->is_foreground_window())
    result = GTK_STATE_FLAG_BACKDROP;
  
  switch(type) {
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked:
      break;
    
    case ListViewItem: {
        switch(state) {
          case Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_INSENSITIVE );
          case PressedHovered: return (GtkStateFlags)( (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case Hovered:        
          case Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_PRELIGHT );
          case Pressed:        
          case Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case CheckboxChecked:
    case OpenerTriangleOpened:
    case RadioButtonChecked: {
        switch(state) {
          case Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_INSENSITIVE );
          case PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case Hovered:        
          case Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_PRELIGHT );
          case Pressed:        
          case Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case ListViewItemSelected: {
        switch(state) {
          case Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_INSENSITIVE );
          case PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case Hovered:        
          case Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_PRELIGHT );
          case Pressed:        
          case Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case TabHead:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft: {
        switch(state) {
          case Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_INSENSITIVE );
          case PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_PRELIGHT );
          case Hovered:        
          case Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_PRELIGHT );
          case Pressed:        return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED );
          case Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    default:
      break;
  }
  
  switch(state) {
    case Hovered:
    case Hot:
      result|= (int)GTK_STATE_FLAG_PRELIGHT;
      break;
      
    case Pressed:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED;
      if(context->is_focused_widget())
        result|= (int)GTK_STATE_FLAG_FOCUSED;
      break;
      
    case PressedHovered:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_PRELIGHT;
      if(context->is_focused_widget())
        result|= (int)GTK_STATE_FLAG_FOCUSED;
      break;
      
    case Disabled:
      result|= (int)GTK_STATE_FLAG_INSENSITIVE;
      break;
      
    case Normal:
    default:
      result|= (int)GTK_STATE_FLAG_NORMAL;
      break;
  }
  
  switch(type) {
    case InputField:
    case AddressBandInputField:
    case AddressBandBackground:
    case TabBodyBackground:
    case TabHeadBackground:
    //case TabHead:
    //case TabHeadAbuttingRight:
    //case TabHeadAbuttingLeftRight:
    //case TabHeadAbuttingLeft: 
      result &= ~(int)GTK_STATE_FLAG_SELECTED;
      break;
      
    case CheckboxIndeterminate:
      result |= (int)GTK_STATE_FLAG_INCONSISTENT;
      break;
      
    default: break;
  }
  
  return (GtkStateFlags)result;
}
 
void MathGtkControlPainter::clear_cache() {
  mgtk_painter_cache.clear();
}
 
#endif
 
//} ... class MathGtkControlPainter

#if GTK_MAJOR_VERSION >= 3

//{ class MathGtkStyleContextCache ...

MathGtkStyleContextCache::MathGtkStyleContextCache() {
  _checkbox_context            = nullptr;
  _default_push_button_context = nullptr;
  _expander_arrow_context      = nullptr;
  _input_field_button_context  = nullptr;
  _input_field_context         = nullptr;
  _list_item_context           = nullptr;
  _list_item_selected_context  = nullptr;
  _panel_context               = nullptr;
  _progress_bar_context        = nullptr;
  _progress_bar_trough_context = nullptr;
  _push_button_context         = nullptr;
  _radio_button_context        = nullptr;
  _slider_channel_context      = nullptr;
  _slider_thumb_context        = nullptr;
  _tab_body_context            = nullptr;
  _tab_head_background_context = nullptr;
  _tab_head_context            = nullptr;
  _tab_head_label_context      = nullptr;
  _tool_button_context         = nullptr;
  _tooltip_context             = nullptr;
}

MathGtkStyleContextCache::~MathGtkStyleContextCache() {
  clear();
}

void MathGtkStyleContextCache::clear() {
  unref_and_null(_checkbox_context);
  unref_and_null(_default_push_button_context);
  unref_and_null(_expander_arrow_context);
  unref_and_null(_input_field_button_context);
  unref_and_null(_input_field_context);
  unref_and_null(_list_item_context);
  unref_and_null(_list_item_selected_context);
  unref_and_null(_panel_context);
  unref_and_null(_progress_bar_context);
  unref_and_null(_progress_bar_trough_context);
  unref_and_null(_push_button_context);
  unref_and_null(_radio_button_context);
  unref_and_null(_slider_channel_context);
  unref_and_null(_slider_thumb_context);
  unref_and_null(_tab_body_context);
  unref_and_null(_tab_head_background_context);
  unref_and_null(_tab_head_context);
  unref_and_null(_tab_head_label_context);
  unref_and_null(_tool_button_context);
  unref_and_null(_tooltip_context);
}

void MathGtkStyleContextCache::render_all_common(GtkStyleContext *ctx, Canvas *canvas, float x, float y, float width, float height) {
  if(!ctx)
    return;
  
  render_all_common(gtk_style_context_get_parent(ctx), canvas, x, y, width, height);
  gtk_render_background(ctx, canvas->cairo(), x, y, width, height);
  gtk_render_frame(     ctx, canvas->cairo(), x, y, width, height);
}

GtkStyleContext *MathGtkStyleContextCache::make_context_from_path_and_free(GtkWidgetPath *path, GtkStyleContext *parent) {
  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_widget_path_unref(path);
  if(parent) {
    gtk_style_context_set_parent(context, parent);
    g_object_unref(parent);
  }
  return context;
}

GtkStyleContext *MathGtkStyleContextCache::make_checkbox_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_CHECK_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "checkbutton");

  GtkStyleContext *button_context = make_context_from_path_and_free(path);
  
  path = gtk_widget_path_copy(gtk_style_context_get_path(button_context));
  gtk_widget_path_append_type(path, GTK_TYPE_CHECK_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "check");
  
  return make_context_from_path_and_free(path, button_context);
//  GtkStyleContext *checkbox_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_CHECK_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_CHECK);
//  
//  gtk_style_context_set_path(    checkbox_context, path);
//  gtk_style_context_set_screen(  checkbox_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(checkbox_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   checkbox_context, GTK_STYLE_CLASS_CHECK);
//  
//  gtk_widget_path_unref(path);
//  return checkbox_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_expander_arrow_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_TREE_VIEW);
  gtk_widget_path_iter_set_object_name(path, -1, "treeview");
  gtk_widget_path_iter_add_class(path, -1, GTK_STYLE_CLASS_VIEW);
  gtk_widget_path_iter_add_class(path, -1, GTK_STYLE_CLASS_EXPANDER);
  return make_context_from_path_and_free(path);
//  GtkStyleContext *expander_arrow_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  //int pos = gtk_widget_path_append_type(path, GTK_TYPE_EXPANDER);
//  //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_EXPANDER);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_TREE_VIEW);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_VIEW);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_EXPANDER);
//  
//  gtk_style_context_set_path(    expander_arrow_context, path);
//  gtk_style_context_set_screen(  expander_arrow_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(expander_arrow_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   expander_arrow_context, GTK_STYLE_CLASS_VIEW);
//  gtk_style_context_add_class(   expander_arrow_context, GTK_STYLE_CLASS_EXPANDER);
//  
//  gtk_widget_path_unref(path);
//  return expander_arrow_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_default_push_button_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "button");
  GtkStyleContext *button_context = make_context_from_path_and_free(path);
  gtk_style_context_add_class(button_context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
  return button_context;
  
//  GtkStyleContext *default_push_button_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path;
//  
//  path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  gtk_style_context_set_path(    default_push_button_context, path);
//  gtk_style_context_set_screen(  default_push_button_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(default_push_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   default_push_button_context, GTK_STYLE_CLASS_BUTTON);
//  gtk_style_context_add_class(   default_push_button_context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
//
//  gtk_widget_path_unref(path);
//  return default_push_button_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_input_field_button_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_ENTRY);
  gtk_widget_path_iter_set_object_name(path, -1, "entry");
  
  gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "button");
  
  GtkStyleContext *button_context = make_context_from_path_and_free(path);
  gtk_style_context_add_class(button_context, GTK_STYLE_CLASS_FLAT);
  return button_context;
  
//  GtkStyleContext *input_field_button_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path;
//  
//  int pos;
//  path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOLBAR);
//  //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLBAR);
//  
//  //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOL_BUTTON);
//  //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  pos = gtk_widget_path_append_type(path, GTK_TYPE_ENTRY);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_ENTRY);
//  
//  pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  gtk_style_context_set_path(    input_field_button_context, path);
//  gtk_style_context_set_screen(  input_field_button_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(input_field_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   input_field_button_context, GTK_STYLE_CLASS_ENTRY);
//  gtk_style_context_add_class(   input_field_button_context, GTK_STYLE_CLASS_BUTTON);
//  gtk_style_context_add_class(   input_field_button_context, GTK_STYLE_CLASS_FLAT);
//
//  gtk_widget_path_unref(path);
//  return input_field_button_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_input_field_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_ENTRY);
  gtk_widget_path_iter_set_object_name(path, -1, "entry");
  return make_context_from_path_and_free(path);

//  GtkStyleContext *input_field_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path;
//  
//  path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_ENTRY);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_ENTRY);
//  
//  gtk_style_context_set_path(    input_field_context, path);
//  gtk_style_context_set_screen(  input_field_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(input_field_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   input_field_context, GTK_STYLE_CLASS_ENTRY);
//
//  gtk_widget_path_unref(path);
//  return input_field_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_list_item_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
  gtk_widget_path_iter_set_object_name(path, -1, "list");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "row");
  gtk_widget_path_iter_add_class(path, -1, "background");
  gtk_widget_path_iter_add_class(path, -1, "activatable");
  
//  gtk_widget_path_append_type(path, GTK_TYPE_LABEL);
//  gtk_widget_path_iter_set_object_name(path, -1, "label");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *list_item_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_LIST);
//  
//  gtk_style_context_set_path(    list_item_context, path);
//  gtk_style_context_set_screen(  list_item_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(list_item_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   list_item_context, GTK_STYLE_CLASS_LIST_ROW);
//  
//  gtk_widget_path_unref(path);
//  return list_item_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_list_item_selected_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
  gtk_widget_path_iter_set_object_name(path, -1, "list");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "row");
  gtk_widget_path_iter_add_class(path, -1, "activatable");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *list_item_selected_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_LIST);
//  
//  gtk_style_context_set_path(    list_item_selected_context, path);
//  gtk_style_context_set_screen(  list_item_selected_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(list_item_selected_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   list_item_selected_context, GTK_STYLE_CLASS_LIST_ROW);
//  gtk_style_context_add_class(   list_item_selected_context, "activatable");
//
//  gtk_widget_path_unref(path);
//  return list_item_selected_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_panel_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_FRAME);
  gtk_widget_path_iter_set_object_name(path, -1, "frame");
  gtk_widget_path_iter_add_class(path, -1, "background");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  
  GtkStyleContext *frame_context = make_context_from_path_and_free(path);
  
  path = gtk_widget_path_copy(gtk_style_context_get_path(frame_context));
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "border");
  
  return make_context_from_path_and_free(path, frame_context);

//  GtkStyleContext *panel_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path;
//  
//  path = gtk_widget_path_new();
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  pos = gtk_widget_path_append_type(path, GTK_TYPE_FRAME);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_FRAME);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BACKGROUND);
//  
//  gtk_style_context_set_path(    panel_context, path);
//  gtk_style_context_set_screen(  panel_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(panel_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   panel_context, GTK_STYLE_CLASS_BACKGROUND);
//  gtk_style_context_add_class(   panel_context, GTK_STYLE_CLASS_FRAME);
//
//  gtk_widget_path_unref(path);
//  return panel_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_progress_bar_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
  gtk_widget_path_iter_set_object_name(path, -1, "progressbar");
  gtk_widget_path_iter_add_class(path, -1, "horizontal");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "trough");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "progress");
  gtk_widget_path_iter_add_class(path, -1, "left");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *progress_bar_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_PROGRESSBAR);
//  
//  gtk_style_context_set_path(    progress_bar_context, path);
//  gtk_style_context_set_screen(  progress_bar_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(progress_bar_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_PROGRESSBAR);
//  
//  // We do not know, where the bar will be drawn. Usually it touches the left 
//  // end of the through only, unless it is 100% full.
//  // Pretending it touches both ends is correct for 100% full only, but lokks not too bad fot < 100%
//  // (the corner radii will usually be adapted)
//  gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_LEFT);
//  gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_RIGHT);
//
//  gtk_widget_path_unref(path);
//  return progress_bar_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_progress_bar_trough_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
  gtk_widget_path_iter_set_object_name(path, -1, "progressbar");
  gtk_widget_path_iter_add_class(path, -1, "horizontal");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "trough");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *progress_bar_trough_context = gtk_style_context_new();
//        
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TROUGH);
//  
//  gtk_style_context_set_path(    progress_bar_trough_context, path);
//  gtk_style_context_set_screen(  progress_bar_trough_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(progress_bar_trough_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   progress_bar_trough_context, GTK_STYLE_CLASS_TROUGH);
//  
//  gtk_widget_path_unref(path);
//  return progress_bar_trough_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_push_button_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "button");
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *push_button_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path;
//  
//  path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  gtk_style_context_set_path(    push_button_context, path);
//  gtk_style_context_set_screen(  push_button_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(push_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   push_button_context, GTK_STYLE_CLASS_BUTTON);
//  
//  gtk_widget_path_unref(path);
//  return push_button_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_radio_button_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_RADIO_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "radiobutton");

  GtkStyleContext *button_context = make_context_from_path_and_free(path);
  
  path = gtk_widget_path_copy(gtk_style_context_get_path(button_context));
  gtk_widget_path_append_type(path, GTK_TYPE_RADIO_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "radio");
  
  return make_context_from_path_and_free(path, button_context);
//  GtkStyleContext *radio_button_context = gtk_style_context_new();
//  
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_RADIO_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_RADIO);
//  
//  gtk_style_context_set_path(    radio_button_context, path);
//  gtk_style_context_set_screen(  radio_button_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(radio_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   radio_button_context, GTK_STYLE_CLASS_RADIO);
//  
//  gtk_widget_path_unref(path);
//  return radio_button_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_slider_channel_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "scale");
  gtk_widget_path_iter_add_class(path, -1, "horizontal");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "contents");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "trough");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *slider_channel_context = gtk_style_context_new();
//        
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
//  //int pos = gtk_widget_path_append_type(path, GTK_TYPE_HSCALE);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
//  
//  gtk_style_context_set_path(    slider_channel_context, path);
//  gtk_style_context_set_screen(  slider_channel_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(slider_channel_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_SCALE);
//  gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_HORIZONTAL);
//  gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_TROUGH);
//
//  gtk_widget_path_unref(path);
//  return slider_channel_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_slider_thumb_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "scale");
  gtk_widget_path_iter_add_class(path, -1, "horizontal");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "contents");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "trough");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "slider");
  
  return make_context_from_path_and_free(path);

//  GtkStyleContext *slider_thumb_context = gtk_style_context_new();
//
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
//  //int pos = gtk_widget_path_append_type(path, GTK_TYPE_HSCALE);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
//
//  gtk_style_context_set_path(    slider_thumb_context, path);
//  gtk_style_context_set_screen(  slider_thumb_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(slider_thumb_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SCALE);
//  gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_HORIZONTAL);
//  gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SLIDER);
//
//  gtk_widget_path_unref(path);
//  return slider_thumb_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_body_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  gtk_widget_path_iter_set_object_name(path, -1, "window");
//  gtk_widget_path_iter_add_class(path, -1, "background");
  
  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  gtk_widget_path_iter_add_class(path, -1, "background");
  return make_context_from_path_and_free(path);
//  GtkStyleContext *notebook = make_context_from_path_and_free(path);
//  return notebook;
//  
//  path = gtk_widget_path_copy(gtk_style_context_get_path(notebook));
//  gtk_widget_path_append_type(path, G_TYPE_NONE);
//  gtk_widget_path_iter_set_object_name(path, -1, "stack");
//  return make_context_from_path_and_free(path, notebook);
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_head_background_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");

  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  //gtk_widget_path_iter_add_class(path, -1, "background");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "header");
  gtk_widget_path_iter_add_class(path, -1, "top");
  return make_context_from_path_and_free(path);
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_head_context(bool has_left_sibling, bool has_right_sibling) {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "header");
  gtk_widget_path_iter_add_class(path, -1, "top");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "tabs");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "tab");
  
  // TODO: recognize has_left_sibling and has_right_sibling, find a theme to test
  
  return make_context_from_path_and_free(path);
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_head_label_context() {
  // TODO: it should really be notebook>header>tabs>tab>label, but then Adwaita and Clearlooks-Phenix would give a white color. Why???
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
//  gtk_widget_path_append_type(path, G_TYPE_NONE);
//  gtk_widget_path_iter_set_object_name(path, -1, "header");
//  gtk_widget_path_iter_add_class(path, -1, "top");
//  
//  gtk_widget_path_append_type(path, G_TYPE_NONE);
//  gtk_widget_path_iter_set_object_name(path, -1, "tabs");
//  
//  gtk_widget_path_append_type(path, G_TYPE_NONE);
//  gtk_widget_path_iter_set_object_name(path, -1, "tab");
//  
//  gtk_widget_path_append_type(path, GTK_TYPE_LABEL);
//  gtk_widget_path_iter_set_object_name(path, -1, "label");
  
  return make_context_from_path_and_free(path);
}

GtkStyleContext *MathGtkStyleContextCache::make_tool_button_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_object_name(path, -1, "button");
  gtk_widget_path_iter_add_class(path, -1, "flat");
  
  return make_context_from_path_and_free(path);
  
//  GtkStyleContext *tool_button_context = gtk_style_context_new();
//  GtkWidgetPath *path;
//  
//  int pos;
//  path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOLBAR);
//  //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLBAR);
//  
//  //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOL_BUTTON);
//  //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
//  
//  gtk_style_context_set_path(    tool_button_context, path);
//  gtk_style_context_set_screen(  tool_button_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(tool_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  //gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_TOOLBAR);
//  gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_BUTTON);
//  gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_FLAT);
//  
//  gtk_widget_path_unref(path);
//  return tool_button_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_tooltip_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
  gtk_widget_path_append_type(path, GTK_TYPE_TOOLTIP);
  gtk_widget_path_iter_set_object_name(path, -1, "tooltip");
  gtk_widget_path_iter_add_class(path, -1, "background");

  GtkStyleContext *tooltip_context = make_context_from_path_and_free(path);
  gtk_style_context_add_class(tooltip_context, GTK_STYLE_CLASS_BACKGROUND);
  
  path = gtk_widget_path_copy(gtk_style_context_get_path(tooltip_context));
  gtk_widget_path_append_type(path, GTK_TYPE_BOX);
  gtk_widget_path_iter_set_object_name(path, -1, "box");
  gtk_widget_path_append_type(path, GTK_TYPE_LABEL);
  gtk_widget_path_iter_set_object_name(path, -1, "label");
  
  return make_context_from_path_and_free(path, tooltip_context);

//  GtkStyleContext *tooltip_context = gtk_style_context_new();
//
//  GtkWidgetPath *path = gtk_widget_path_new();
//  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
//  int pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOLTIP);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLTIP);
//
//  gtk_style_context_set_path(    tooltip_context, path);
//  gtk_style_context_set_screen(  tooltip_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(tooltip_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   tooltip_context, GTK_STYLE_CLASS_TOOLTIP);
//  gtk_style_context_add_class(   tooltip_context, GTK_STYLE_CLASS_BACKGROUND);
//
//  gtk_widget_path_unref(path);
//  return tooltip_context;
}

void MathGtkStyleContextCache::unref_and_null(GtkStyleContext *&style_context) {
  if(style_context) {
    g_object_unref(style_context);
    style_context = nullptr;
  }
}

//} ... class MathGtkStyleContextCache

#endif
