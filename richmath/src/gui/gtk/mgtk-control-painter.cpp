#include <gui/gtk/mgtk-control-painter.h>

#include <eval/observable.h>
#include <util/style.h>
#include <graphics/shapers.h>

#include <algorithm>
#include <cmath>


using namespace richmath;

static Observable style_observations;

#if GTK_MAJOR_VERSION >= 3
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
  
#if GTK_MAJOR_VERSION >= 3
  push_button_context         = nullptr;
  default_push_button_context = nullptr;
  expander_arrow_context      = nullptr;
  tool_button_context         = nullptr;
  input_field_context         = nullptr;
  slider_channel_context      = nullptr;
  slider_thumb_context        = nullptr;
  list_item_context           = nullptr;
  list_item_selected_context  = nullptr;
  panel_context               = nullptr;
  progress_bar_trough_context = nullptr;
  progress_bar_context        = nullptr;
  checkbox_context            = nullptr;
  radio_button_context        = nullptr;
  tooltip_context             = nullptr;
#endif
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
    switch(type) {
      case DefaultPushButton:
      case PushButton:
      case PaletteButton:
        if(extents->ascent < canvas->get_font_size() * 0.75f)
          extents->ascent = canvas->get_font_size() * 0.75f;
          
        if(extents->descent < canvas->get_font_size() * 0.25f)
          extents->descent = canvas->get_font_size() * 0.25f;
        break;
        
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate:
      case RadioButtonUnchecked:
      case RadioButtonChecked: {
          int size;
          gtk_style_context_get_style(gtk_ctx, "indicator-size", &size, nullptr);
          
          extents->width   = 0.75f * size;
          extents->ascent  = 0.75f * extents->width;
          extents->descent = 0.25f * extents->width;
        }
        return;
      
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
          float h = 4 * 0.75f;
          extents->ascent  = h * 0.75;
          extents->descent = h * 0.25;
        }
        return;
        
      case SliderHorzThumb: {
          int w = 0;
          gtk_style_context_get_style(gtk_ctx, "min-width", &w, nullptr); // GTK >= 3.20.0
          if(w <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-width", &w, nullptr);
          
          int h = 0;
          gtk_style_context_get_style(gtk_ctx, "min-height", &h, nullptr); // GTK >= 3.20.0
          if(h <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-length", &h, nullptr);
          
          extents->ascent  = h * 0.75f * 0.75f;
          extents->descent = h * 0.75f * 0.25f;
          extents->width  = w * 0.75f;
        }
        return;
        
      case ProgressIndicatorBackground: {
          //extents->ascent *= 0.5;
          //extents->descent *= 0.5;
          
          int h = 6;
          gtk_style_context_get_style(gtk_ctx, "min-horizontal-bar-height", &h, nullptr);
          
          GtkBorder padding;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
          h+= padding.top + padding.bottom;
          
          extents->ascent = 0.75 * h;
          extents->descent = 0;
          extents->width = extents->height() * 15;
        }
        return;
        
      case ProgressIndicatorBar: {
          GtkBorder padding;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
          
          extents->ascent -=  padding.top    * 0.75;
          extents->descent -= padding.bottom * 0.75;
          extents->width -=   (padding.left + padding.right) * 0.75;
        }
        return;
        
      case NavigationBack:
      case NavigationForward: {
          int w, h;
          gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
          
          GtkBorder padding;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
          
          GtkBorder border;
          gtk_style_context_get_border(gtk_ctx, GTK_STATE_FLAG_NORMAL, &border);
          
          extents->width   = std::max((w + padding.left + padding.right) * 0.75, (double)extents->width);
          extents->ascent  = std::max((h * 0.75 + padding.top) * 0.75,           (double)extents->ascent);
          extents->descent = std::max((h * 0.25 + padding.bottom) * 0.75,        (double)extents->descent);
        } 
        return;
      
      default:
        break;
    }
    
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
  if(GtkStyleContext *gsc = get_control_theme(context, type)) {
    GdkRGBA color;
    gtk_style_context_get_color(gsc, get_state_flags(context, type, state), &color);
    
    // ignoring color.alpha
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
  if(type == PaletteButton && state == Normal)
    return;
    
  if(GtkStyleContext *gsc = get_control_theme(context, type)) {
    canvas->save();
    gtk_style_context_save(gsc);
    
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
    
    gtk_style_context_set_state(gsc, get_state_flags(context, type, state));
    
    switch(type) {
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate:
        gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
        gtk_render_check(     gsc, canvas->cairo(), x, y, width, height);
        break;
        
      case RadioButtonUnchecked:
      case RadioButtonChecked:
        gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
        gtk_render_option(    gsc, canvas->cairo(), x, y, width, height);
        break;
        
      case OpenerTriangleClosed:
      case OpenerTriangleOpened:
        //gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
        gtk_render_expander(  gsc, canvas->cairo(), x, y, width, height);
        break;
        
      case SliderHorzThumb: {
          gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
          gtk_render_frame(gsc, canvas->cairo(), x, y, width, height);
                         
          // gtk_render_slider() draws to the wrong location (coordinates scaled by ~ 2/3)
          //gtk_render_slider(gsc, canvas->cairo(), x, y, width, height, GTK_ORIENTATION_HORIZONTAL);
        } break;
      
      case NavigationBack:
      case NavigationForward: {
          float cx = x + width/2;
          float cy = y + height/2;
          width = height = std::min(width, height);
          x = cx - width/2;
          y = cy - height/2;
        
          canvas->align_point(&x, &y, false);
    
          gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
          gtk_render_frame(gsc, canvas->cairo(), x, y, width, height);
          
          GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
          int w, h;
          gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
          int icon_size = std::min(w, h);
          
          GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(icon_theme, type == NavigationBack ? "go-previous" : "go-next", icon_size, (GtkIconLookupFlags)0);
          GdkPixbuf *pixbuf = gtk_icon_info_load_symbolic_for_context(icon_info, gsc, nullptr, nullptr);
          g_object_unref(icon_info);
          
          canvas->save();
          //gtk_render_icon(gsc, canvas->cairo(), pixbuf, cx - w * 0.5 * 0.75, cy - h * 0.5 * 0.75);
          gtk_render_icon(gsc, canvas->cairo(), pixbuf, cx - w * 0.5, cy - h * 0.5);
          canvas->restore();
          gdk_pixbuf_unref(pixbuf);
          
//          if(GtkIconSet *icon = gtk_icon_factory_lookup_default(type == NavigationBack ? "go-previous" : "go-next")) {
//            GdkPixbuf *pixbuf = gtk_icon_set_render_icon_pixbuf(icon, gsc, GTK_ICON_SIZE_SMALL_TOOLBAR);
//            
//            gtk_render_icon(gsc, canvas->cairo(), pixbuf, cx - w * 0.75, cy - h * 0.75);
//            
//            gdk_pixbuf_unref(pixbuf);
//          }
        } break;
        
      default:
        gtk_render_background(gsc, canvas->cairo(), x, y, width, height);
        gtk_render_frame(     gsc, canvas->cairo(), x, y, width, height);
        break;
    }
    
    gtk_style_context_restore(gsc);
    canvas->restore();
    return;
  }
  
  ControlPainter::draw_container(context, canvas, type, state, x, y, width, height);
}
 
void MathGtkControlPainter::container_content_move(
  ContainerType  type,
  ControlState   state,
  float         *x,
  float         *y)
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
  
  ControlPainter::container_content_move(type, state, x, y);
}
 
bool MathGtkControlPainter::container_hover_repaint(ControlContext *context, ContainerType type) {
  switch(type) {
    case NoContainerType:
    case GenericButton:
    case TooltipWindow:
    case PanelControl:
      return false;
  }
  
  return get_control_theme(context, type) != nullptr;
}
 
void MathGtkControlPainter::system_font_style(ControlContext *context, Style *style) {
  GtkStyleContext *gsc = get_control_theme(context, PushButton);
  
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
 
GtkStyleContext *MathGtkControlPainter::get_control_theme(ControlContext *context, ContainerType type) {
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
      
    case PushButton:
      if(!push_button_context) {
        push_button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        gtk_style_context_set_path(    push_button_context, path);
        gtk_style_context_set_screen(  push_button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(push_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   push_button_context, GTK_STYLE_CLASS_BUTTON);
      }
      return push_button_context;
      
    case DefaultPushButton:
      if(!default_push_button_context) {
        default_push_button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        gtk_style_context_set_path(    default_push_button_context, path);
        gtk_style_context_set_screen(  default_push_button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(default_push_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   default_push_button_context, GTK_STYLE_CLASS_BUTTON);
        gtk_style_context_add_class(   default_push_button_context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
      }
      return default_push_button_context;
      
    case NavigationBack:
    case NavigationForward:
    case PaletteButton:
      if(!tool_button_context) {
        tool_button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        int pos;
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOLBAR);
        //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLBAR);
        
        //pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOL_BUTTON);
        //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        pos = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        gtk_style_context_set_path(    tool_button_context, path);
        gtk_style_context_set_screen(  tool_button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(tool_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        //gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_TOOLBAR);
        gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_BUTTON);
        gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_FLAT);
      }
      return tool_button_context;
      
    case InputField:
      if(!input_field_context) {
        input_field_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_ENTRY);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_ENTRY);
        
        gtk_style_context_set_path(    input_field_context, path);
        gtk_style_context_set_screen(  input_field_context, gdk_screen_get_default());
        gtk_style_context_add_provider(input_field_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   input_field_context, GTK_STYLE_CLASS_ENTRY);
      }
      return input_field_context;
      
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
      if(!checkbox_context) {
        checkbox_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_CHECK_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_CHECK);
        
        gtk_style_context_set_path(    checkbox_context, path);
        gtk_style_context_set_screen(  checkbox_context, gdk_screen_get_default());
        gtk_style_context_add_provider(checkbox_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   checkbox_context, GTK_STYLE_CLASS_CHECK);
      }
      return checkbox_context;
      
    case RadioButtonUnchecked:
    case RadioButtonChecked:
      if(!radio_button_context) {
        radio_button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_RADIO_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_RADIO);
        
        gtk_style_context_set_path(    radio_button_context, path);
        gtk_style_context_set_screen(  radio_button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(radio_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   radio_button_context, GTK_STYLE_CLASS_RADIO);
      }
      return radio_button_context;
    
    case PanelControl:
      if(!panel_context) {
        panel_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        pos = gtk_widget_path_append_type(path, GTK_TYPE_FRAME);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_FRAME);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BACKGROUND);
        
        gtk_style_context_set_path(    panel_context, path);
        gtk_style_context_set_screen(  panel_context, gdk_screen_get_default());
        gtk_style_context_add_provider(panel_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   panel_context, GTK_STYLE_CLASS_BACKGROUND);
        gtk_style_context_add_class(   panel_context, GTK_STYLE_CLASS_FRAME);
      }
      return panel_context;
    
    case ProgressIndicatorBackground:
      if(!progress_bar_trough_context) {
        progress_bar_trough_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TROUGH);
        
        gtk_style_context_set_path(    progress_bar_trough_context, path);
        gtk_style_context_set_screen(  progress_bar_trough_context, gdk_screen_get_default());
        gtk_style_context_add_provider(progress_bar_trough_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   progress_bar_trough_context, GTK_STYLE_CLASS_TROUGH);
      }
      return progress_bar_trough_context;
      
    case ProgressIndicatorBar:
      if(!progress_bar_context) {
        progress_bar_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_PROGRESS_BAR);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_PROGRESSBAR);
        
        gtk_style_context_set_path(    progress_bar_context, path);
        gtk_style_context_set_screen(  progress_bar_context, gdk_screen_get_default());
        gtk_style_context_add_provider(progress_bar_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_PROGRESSBAR);
        
        // We do not know, where the bar will be drawn. Usually it touches the left 
        // end of the through only, unless it is 100% full.
        // Pretending it touches both ends is correct for 100% full only, but lokks not too bad fot < 100%
        // (the corner radii will usually be adapted)
        gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_LEFT);
        gtk_style_context_add_class(   progress_bar_context, GTK_STYLE_CLASS_RIGHT);
      }
      return progress_bar_context;
      
    case SliderHorzChannel:
      if(!slider_channel_context) {
        slider_channel_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
        //int pos = gtk_widget_path_append_type(path, GTK_TYPE_HSCALE);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
        
        gtk_style_context_set_path(    slider_channel_context, path);
        gtk_style_context_set_screen(  slider_channel_context, gdk_screen_get_default());
        gtk_style_context_add_provider(slider_channel_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_SCALE);
        gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_HORIZONTAL);
        gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_TROUGH);
      }
      return slider_channel_context;
      
    case SliderHorzThumb:
      if(!slider_thumb_context) {
        slider_thumb_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
        //int pos = gtk_widget_path_append_type(path, GTK_TYPE_HSCALE);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
        
        gtk_style_context_set_path(    slider_thumb_context, path);
        gtk_style_context_set_screen(  slider_thumb_context, gdk_screen_get_default());
        gtk_style_context_add_provider(slider_thumb_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SCALE);
        gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_HORIZONTAL);
        gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SLIDER);
      }
      return slider_thumb_context;
      
    case TooltipWindow:
      if(!tooltip_context) {
        tooltip_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_TOOLTIP);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLTIP);
        
        gtk_style_context_set_path(    tooltip_context, path);
        gtk_style_context_set_screen(  tooltip_context, gdk_screen_get_default());
        gtk_style_context_add_provider(tooltip_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   tooltip_context, GTK_STYLE_CLASS_TOOLTIP);
        gtk_style_context_add_class(   tooltip_context, GTK_STYLE_CLASS_BACKGROUND);
      }
      return tooltip_context;
      
    case ListViewItem:
      if(!list_item_context) {
        list_item_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_LIST);
        
        gtk_style_context_set_path(    list_item_context, path);
        gtk_style_context_set_screen(  list_item_context, gdk_screen_get_default());
        gtk_style_context_add_provider(list_item_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   list_item_context, GTK_STYLE_CLASS_LIST_ROW);
      }
      return list_item_context;
      
    case ListViewItemSelected:
      if(!list_item_selected_context) {
        list_item_selected_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_LIST);
        
        gtk_style_context_set_path(    list_item_selected_context, path);
        gtk_style_context_set_screen(  list_item_selected_context, gdk_screen_get_default());
        gtk_style_context_add_provider(list_item_selected_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   list_item_selected_context, GTK_STYLE_CLASS_LIST_ROW);
        gtk_style_context_add_class(   list_item_selected_context, "activatable");
      }
      return list_item_selected_context;
    
    case OpenerTriangleClosed:
    case OpenerTriangleOpened:
      if(!expander_arrow_context) {
        expander_arrow_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
        //int pos = gtk_widget_path_append_type(path, GTK_TYPE_EXPANDER);
        //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_EXPANDER);
        int pos = gtk_widget_path_append_type(path, GTK_TYPE_TREE_VIEW);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_VIEW);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_EXPANDER);
        
        gtk_style_context_set_path(    expander_arrow_context, path);
        gtk_style_context_set_screen(  expander_arrow_context, gdk_screen_get_default());
        gtk_style_context_add_provider(expander_arrow_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   expander_arrow_context, GTK_STYLE_CLASS_VIEW);
        gtk_style_context_add_class(   expander_arrow_context, GTK_STYLE_CLASS_EXPANDER);
      }
      return expander_arrow_context;
    
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
    case GenericButton:
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
    
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked: 
    
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
  }
  
  switch(state) {
    case Hovered:
    case Hot:
      result|= (int)GTK_STATE_FLAG_PRELIGHT;
      break;
      
    case Pressed:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_FOCUSED;
      break;
      
    case PressedHovered:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_FOCUSED | (int)GTK_STATE_FLAG_PRELIGHT;
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
  if(push_button_context)
    g_object_unref(push_button_context);
    
  if(default_push_button_context)
    g_object_unref(default_push_button_context);
  
  if(expander_arrow_context)
    g_object_unref(expander_arrow_context);
  
  if(tool_button_context)
    g_object_unref(tool_button_context);
    
  if(input_field_context)
    g_object_unref(input_field_context);
    
  if(slider_channel_context)
    g_object_unref(slider_channel_context);
    
  if(slider_thumb_context)
    g_object_unref(slider_thumb_context);
    
  if(list_item_context)
    g_object_unref(list_item_context);
    
  if(list_item_selected_context)
    g_object_unref(list_item_selected_context);
    
  if(panel_context)
    g_object_unref(panel_context);
    
  if(progress_bar_trough_context)
    g_object_unref(progress_bar_trough_context);
    
  if(progress_bar_context)
    g_object_unref(progress_bar_context);
    
  if(checkbox_context)
    g_object_unref(checkbox_context);
    
  if(radio_button_context)
    g_object_unref(radio_button_context);
    
  if(tooltip_context)
    g_object_unref(tooltip_context);
    
  push_button_context         = nullptr;
  default_push_button_context = nullptr;
  expander_arrow_context      = nullptr;
  tool_button_context         = nullptr;
  input_field_context         = nullptr;
  slider_channel_context      = nullptr;
  slider_thumb_context        = nullptr;
  list_item_context           = nullptr;
  list_item_selected_context  = nullptr;
  panel_context               = nullptr;
  progress_bar_trough_context = nullptr;
  progress_bar_context        = nullptr;
  checkbox_context            = nullptr;
  radio_button_context        = nullptr;
  tooltip_context        = nullptr;
}
 
#endif
 
//} ... class MathGtkControlPainter
 
 
 
 
 
 
 
 
