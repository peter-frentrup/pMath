#include <gui/gtk/mgtk-control-painter.h>


using namespace richmath;

//{ class MathGtkControlPainter ...

MathGtkControlPainter MathGtkControlPainter::gtk_painter;

MathGtkControlPainter::MathGtkControlPainter()
: ControlPainter(),
  button_context(0)
{
  ControlPainter::std = this;
}

MathGtkControlPainter::~MathGtkControlPainter() {
  if(ControlPainter::std == this)
    ControlPainter::std = &ControlPainter::generic_painter;
  
  if(button_context)
    g_object_unref(button_context);
}

#if GTK_MAJOR_VERSION >= 3

void MathGtkControlPainter::calc_container_size(
  Canvas        *canvas,
  ContainerType  type,
  BoxSize       *extents
) {
  ControlPainter::calc_container_size(canvas, type, extents);
}

int MathGtkControlPainter::control_font_color(ContainerType type, ControlState state) {
  return ControlPainter::control_font_color(type, state);
}

bool MathGtkControlPainter::is_very_transparent(ContainerType type, ControlState state) {
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
  GtkStyleContext *context = get_control_theme(type);
  if(context) {
    canvas->save();
    gtk_style_context_save(context);
    
    canvas->set_color(0x000000);
    gtk_style_context_set_state(context, get_state_flags(state));
    gtk_render_background(context, canvas->cairo(), x, y, width, height);
    gtk_render_frame(context, canvas->cairo(), x, y, width, height);
    
    gtk_style_context_restore(context);
    canvas->restore();
    return;
  }
  
  ControlPainter::draw_container(canvas, type, state, x, y, width, height);
}

GtkStyleContext *MathGtkControlPainter::get_control_theme(ContainerType type) {
  switch(type) {
    case FramelessButton:
    case GenericButton:
      break;
      
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
      if(!button_context){
        button_context = gtk_style_context_new();
        
        GtkWidgetPath *path;

        path = gtk_widget_path_new ();
        gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
        gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
        
        gtk_style_context_set_path(  button_context, path);
        gtk_style_context_set_screen(button_context, gdk_screen_get_default());
        gtk_style_context_add_provider(button_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
      }
      return button_context;
      
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
