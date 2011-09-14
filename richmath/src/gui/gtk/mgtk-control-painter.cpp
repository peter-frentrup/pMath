#include <gui/gtk/mgtk-control-painter.h>

#include <util/style.h>
#include <graphics/shapers.h>
 
 
using namespace richmath;
 
//{ class MathGtkControlPainter ...
 
MathGtkControlPainter MathGtkControlPainter::gtk_painter;
 
MathGtkControlPainter::MathGtkControlPainter()
  : ControlPainter(),
  button_context(0),
  tool_button_context(0)
{
  ControlPainter::std = this;
}
 
MathGtkControlPainter::~MathGtkControlPainter() {
  if(ControlPainter::std == this)
    ControlPainter::std = &ControlPainter::generic_painter;
    
  if(button_context)
    g_object_unref(button_context);
    
  if(tool_button_context)
    g_object_unref(tool_button_context);
}
 
#if GTK_MAJOR_VERSION >= 3
 
void MathGtkControlPainter::calc_container_size(
  Canvas        *canvas,
  ContainerType  type,
  BoxSize       *extents
) {
  GtkStyleContext *context = get_control_theme(type);
  
  if(context) {
    GtkBorder border;
    
    gtk_style_context_get_padding(context, GTK_STATE_FLAG_NORMAL, &border);
    extents->ascent +=  0.75f * border.top;
    extents->descent += 0.75f * border.bottom;
    extents->width +=   0.75f * (border.left + border.right);
    
    gtk_style_context_get_border(context, GTK_STATE_FLAG_NORMAL, &border);
    extents->ascent +=  0.75f * border.top;
    extents->descent += 0.75f * border.bottom;
    extents->width +=   0.75f * (border.left + border.right);
    
    return;
  }
  
  ControlPainter::calc_container_size(canvas, type, extents);
}
 
int MathGtkControlPainter::control_font_color(ContainerType type, ControlState state) {
  GtkStyleContext *context = get_control_theme(type);
  
  if(context) {
    GdkRGBA color;
    gtk_style_context_get_color(context, get_state_flags(state), &color);
    
    // ignoring color.alpha
    int red   = (int)(color.red   * 255);
    int green = (int)(color.green * 255);
    int blue  = (int)(color.blue  * 255);
    
    return (red << 16) | (green << 8) | blue;
  }
  
  return ControlPainter::control_font_color(type, state);
}
 
bool MathGtkControlPainter::is_very_transparent(ContainerType type, ControlState state) {
  switch(type) {
    case PaletteButton:
      return state == Normal;
      
    default:
      break;
  }
  return ControlPainter::is_very_transparent(type, state);
}
 
void MathGtkControlPainter::draw_container(
  Canvas        *canvas,
  ContainerType  type,
  ControlState   state,
  float          x,
  float          y,
  float          width,
  float          height
) {
  if(type == PaletteButton && state == Normal)
    return;
    
  GtkStyleContext *context = get_control_theme(type);
  
  if(context) {
    canvas->save();
    gtk_style_context_save(context);
    
    gtk_style_context_set_state(context, get_state_flags(state));
    gtk_render_background(context, canvas->cairo(), x, y, width, height);
    gtk_render_frame(context, canvas->cairo(), x, y, width, height);
    
    gtk_style_context_restore(context);
    canvas->restore();
    return;
  }
  
  ControlPainter::draw_container(canvas, type, state, x, y, width, height);
}
 
bool MathGtkControlPainter::container_hover_repaint(ContainerType type) {
  return get_control_theme(type) != 0;
}
 
void MathGtkControlPainter::system_font_style(Style *style) {
  GtkStyleContext *context = get_control_theme(PushButton);
  
  if(!context) {
    ControlPainter::system_font_style(style);
    return;
  }
  
  const PangoFontDescription *desc = gtk_style_context_get_font(context, GTK_STATE_FLAG_NORMAL);
  const char *family = pango_font_description_get_family(desc);
  if(family)
    style->set(FontFamily, String::FromUtf8(family));
    
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
    size*= 0.75; 
  
  if(size > 0)
    style->set(FontSize, size);
}
 
GtkStyleContext *MathGtkControlPainter::get_control_theme(ContainerType type) {
  switch(type) {
    case FramelessButton:
    case GenericButton:
      break;
      
    case PushButton:
    case DefaultPushButton:
      if(!button_context) {
        button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        path = gtk_widget_path_new ();
        gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
        int pos = gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        gtk_style_context_set_path(    button_context, path);
        gtk_style_context_set_screen(  button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class(   button_context, GTK_STYLE_CLASS_BUTTON);
      }
      return button_context;
      
    case PaletteButton:
      if(!tool_button_context) {
        tool_button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;
        
        int pos;
        path = gtk_widget_path_new ();
        gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
        //pos = gtk_widget_path_append_type (path, GTK_TYPE_TOOLBAR);
        //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_TOOLBAR);
        
        pos = gtk_widget_path_append_type (path, GTK_TYPE_TOOL_BUTTON);
        //gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        pos = gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_BUTTON);
        
        gtk_style_context_set_path(    tool_button_context, path);
        gtk_style_context_set_screen(  tool_button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(tool_button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        //gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_TOOLBAR);
        gtk_style_context_add_class(   tool_button_context, GTK_STYLE_CLASS_BUTTON);
      }
      return tool_button_context;
      
    case InputField:
    case TooltipWindow:
    case ListViewItem:
    case ListViewItemSelected:
    case SliderHorzChannel:
    case SliderHorzThumb:
    case ProgressIndicatorBackground:
    case ProgressIndicatorBar:
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
    case RadioButtonUnchecked:
    case RadioButtonChecked:
    
    default:
      break;
  }
  
  return 0;
}
 
GtkStateFlags MathGtkControlPainter::get_state_flags(ControlState state) {
  switch(state) {
    case Normal:    return GTK_STATE_FLAG_NORMAL;
    case Hovered:
    case Hot:       return GTK_STATE_FLAG_PRELIGHT;
    case Pressed:   return GTK_STATE_FLAG_ACTIVE;
    case Disabled:  return GTK_STATE_FLAG_INSENSITIVE;
  }
  
  return GTK_STATE_FLAG_NORMAL;
}
 
#endif
 
//} ... class MathGtkControlPainter
 
 
 
 
 
