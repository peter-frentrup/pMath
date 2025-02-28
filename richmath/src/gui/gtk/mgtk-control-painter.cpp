#include <gui/gtk/mgtk-control-painter.h>

#include <boxes/box.h>
#include <eval/observable.h>
#include <util/style.h>
#include <graphics/shapers.h>

#include <gui/gtk/mgtk-css.h>

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
class MathGtkStyleContextCache {
  public:
    MathGtkStyleContextCache(const char *theme_variant);
    MathGtkStyleContextCache(const MathGtkStyleContextCache &) = delete;
    ~MathGtkStyleContextCache();
    void clear();
    
    GtkStyleProvider *current_theme() {  return init_once(_current_theme,  [variant=_theme_variant]() { return make_current_theme_provider(variant); }); }
    
    GtkStyleContext *checkbox_context() {                return init_context_once(_checkbox_context,                make_checkbox_context); }
    GtkStyleContext *default_push_button_context() {     return init_context_once(_default_push_button_context,     make_default_push_button_context); }
    GtkStyleContext *expander_arrow_context() {          return init_context_once(_expander_arrow_context,          make_expander_arrow_context); }
    GtkStyleContext *input_field_button_context() {      return init_context_once(_input_field_button_context,      make_input_field_button_context); }
    GtkStyleContext *input_field_context() {             return init_context_once(_input_field_context,             make_input_field_context); }
    GtkStyleContext *list_item_context() {               return init_context_once(_list_item_context,               make_list_item_context); }
    GtkStyleContext *list_item_selected_context() {      return init_context_once(_list_item_selected_context,      make_list_item_selected_context); }
    GtkStyleContext *panel_context() {                   return init_context_once(_panel_context,                   make_panel_context); }
    GtkStyleContext *popup_panel_context() {             return init_context_once(_popup_panel_context,             make_popup_panel_context); }
    GtkStyleContext *progress_bar_context() {            return init_context_once(_progress_bar_context,            make_progress_bar_context); }
    GtkStyleContext *progress_bar_trough_context() {     return init_context_once(_progress_bar_trough_context,     make_progress_bar_trough_context); }
    GtkStyleContext *push_button_context() {             return init_context_once(_push_button_context,             make_push_button_context); }
    GtkStyleContext *radio_button_context() {            return init_context_once(_radio_button_context,            make_radio_button_context); }
    GtkStyleContext *horz_slider_channel_context() {           return init_context_once(_horz_slider_channel_context,           []() { return make_slider_channel_context(false); }); }
    GtkStyleContext *horz_slider_thumb_context() {             return init_context_once(_horz_slider_thumb_context,             []() { return make_slider_thumb_context(  false, false, false); }); }
    GtkStyleContext *horz_slider_thumb_down_arrow_context() {  return init_context_once(_horz_slider_thumb_down_arrow_context,  []() { return make_slider_thumb_context(  false, false, true); }); }
    GtkStyleContext *horz_slider_thumb_up_arrow_context() {    return init_context_once(_horz_slider_thumb_up_arrow_context,    []() { return make_slider_thumb_context(  false, true, false); }); }
    GtkStyleContext *vert_slider_channel_context() {           return init_context_once(_vert_slider_channel_context,           []() { return make_slider_channel_context(true); }); }
    GtkStyleContext *vert_slider_thumb_context() {             return init_context_once(_vert_slider_thumb_context,             []() { return make_slider_thumb_context(  true, false, false); }); }
    GtkStyleContext *vert_slider_thumb_left_arrow_context() {  return init_context_once(_vert_slider_thumb_left_arrow_context,  []() { return make_slider_thumb_context(  true, true,  false); }); }
    GtkStyleContext *vert_slider_thumb_right_arrow_context() { return init_context_once(_vert_slider_thumb_right_arrow_context, []() { return make_slider_thumb_context(  true, false, true); }); }
    GtkStyleContext *tab_body_context() {                      return init_context_once(_tab_body_context,                     make_tab_body_context); }
    GtkStyleContext *tab_head_top_background_context() {       return init_context_once(_tab_head_top_background_context,      []() { return make_tab_head_background_context(Side::Top); } ); }
    GtkStyleContext *tab_head_left_background_context() {      return init_context_once(_tab_head_left_background_context,     []() { return make_tab_head_background_context(Side::Left); } ); }
    GtkStyleContext *tab_head_right_background_context() {     return init_context_once(_tab_head_right_background_context,    []() { return make_tab_head_background_context(Side::Right); } ); }
    GtkStyleContext *tab_head_bottom_background_context() {    return init_context_once(_tab_head_bottom_background_context,   []() { return make_tab_head_background_context(Side::Bottom); } ); }
    GtkStyleContext *tab_head_other_background_context() {     return init_context_once(_tab_head_other_background_context,    []() { return make_tab_head_background_context((Side)(-1)); } ); }
    GtkStyleContext *tab_head_top_abutting_none_context() {     return init_context_once(_tab_head_top_abutting_none_context,      []() { return make_tab_head_context(Side::Top,     false, false); }); }
    GtkStyleContext *tab_head_top_abutting_right_context() {    return init_context_once(_tab_head_top_abutting_right_context,     []() { return make_tab_head_context(Side::Top,     false, true); }); }
    GtkStyleContext *tab_head_top_abutting_both_context() {     return init_context_once(_tab_head_top_abutting_both_context,      []() { return make_tab_head_context(Side::Top,     true,  true); }); }
    GtkStyleContext *tab_head_top_abutting_left_context() {     return init_context_once(_tab_head_top_abutting_left_context,      []() { return make_tab_head_context(Side::Top,     true,  false); }); }
    GtkStyleContext *tab_head_bottom_abutting_none_context() {  return init_context_once(_tab_head_bottom_abutting_none_context,   []() { return make_tab_head_context(Side::Bottom,  false, false); }); }
    GtkStyleContext *tab_head_bottom_abutting_right_context() { return init_context_once(_tab_head_bottom_abutting_right_context,  []() { return make_tab_head_context(Side::Bottom,  false, true); }); }
    GtkStyleContext *tab_head_bottom_abutting_both_context() {  return init_context_once(_tab_head_bottom_abutting_both_context,   []() { return make_tab_head_context(Side::Bottom,  true,  true); }); }
    GtkStyleContext *tab_head_bottom_abutting_left_context() {  return init_context_once(_tab_head_bottom_abutting_left_context,   []() { return make_tab_head_context(Side::Bottom,  true,  false); }); }
    GtkStyleContext *tab_head_left_abutting_none_context() {    return init_context_once(_tab_head_left_abutting_none_context,     []() { return make_tab_head_context(Side::Left,    false, false); }); }
    GtkStyleContext *tab_head_left_abutting_below_context() {   return init_context_once(_tab_head_left_abutting_below_context,    []() { return make_tab_head_context(Side::Left,    false, true); }); }
    GtkStyleContext *tab_head_left_abutting_both_context() {    return init_context_once(_tab_head_left_abutting_both_context,     []() { return make_tab_head_context(Side::Left,    true,  true); }); }
    GtkStyleContext *tab_head_left_abutting_above_context() {   return init_context_once(_tab_head_left_abutting_above_context,    []() { return make_tab_head_context(Side::Left,    true,  false); }); }
    GtkStyleContext *tab_head_right_abutting_none_context() {   return init_context_once(_tab_head_right_abutting_none_context,    []() { return make_tab_head_context(Side::Right,   false, false); }); }
    GtkStyleContext *tab_head_right_abutting_below_context() {  return init_context_once(_tab_head_right_abutting_below_context,   []() { return make_tab_head_context(Side::Right,   false, true); }); }
    GtkStyleContext *tab_head_right_abutting_both_context() {   return init_context_once(_tab_head_right_abutting_both_context,    []() { return make_tab_head_context(Side::Right,   true,  true); }); }
    GtkStyleContext *tab_head_right_abutting_above_context() {  return init_context_once(_tab_head_right_abutting_above_context,   []() { return make_tab_head_context(Side::Right,   true,  false); }); }
    GtkStyleContext *tab_head_label_context() {          return init_context_once(_tab_head_label_context,          make_tab_head_label_context); }
    GtkStyleContext *toggle_switch_channel_context() {   return init_context_once(_toggle_switch_channel_context,   make_toggle_switch_channel_context); }
    GtkStyleContext *toggle_switch_thumb_context() {     return init_context_once(_toggle_switch_thumb_context,     make_toggle_switch_thumb_context); }
    GtkStyleContext *tool_button_context() {             return init_context_once(_tool_button_context,             make_tool_button_context); }
    GtkStyleContext *tooltip_context() {                 return init_context_once(_tooltip_context,                 make_tooltip_context); }
    
    void render_container(
      GtkStyleContext *ctx, 
      GtkStateFlags    flags,
      Canvas          &canvas,
      ContainerType    type,
      ControlState     state,
      RectangleF       rect);
    static void render_all_common(            GtkStyleContext *ctx, Canvas &canvas, const RectangleF &rect);
    static void render_all_common_inset_const(GtkStyleContext *ctx, Canvas &canvas, RectangleF rect) { render_all_common_inset(ctx, canvas, rect); }
    static void render_all_common_inset(      GtkStyleContext *ctx, Canvas &canvas, RectangleF &rect);
    static GtkBorder get_all_border_padding(GtkStyleContext *ctx);
    
    static GtkStyleContext *make_context_from_path_and_free(GtkWidgetPath *path, GtkStyleContext *parent = nullptr);
    
  private:
    static GtkStyleProvider *make_current_theme_provider(const char *variant);
    
    static GtkStyleContext *make_checkbox_context();
    static GtkStyleContext *make_default_push_button_context();
    static GtkStyleContext *make_expander_arrow_context();
    static GtkStyleContext *make_input_field_button_context();
    static GtkStyleContext *make_input_field_context();
    static GtkStyleContext *make_list_item_context();
    static GtkStyleContext *make_list_item_selected_context();
    static GtkStyleContext *make_panel_context();
    static GtkStyleContext *make_popup_panel_context();
    static GtkStyleContext *make_progress_bar_context();
    static GtkStyleContext *make_progress_bar_trough_context();
    static GtkStyleContext *make_push_button_context();
    static GtkStyleContext *make_radio_button_context();
    static GtkStyleContext *make_slider_channel_context(bool vertical);
    static GtkStyleContext *make_slider_thumb_context(bool vertical, bool marks_before, bool marks_after);
    static GtkStyleContext *make_tab_body_context();
    static GtkStyleContext *make_tab_head_background_context(Side side);
    static GtkStyleContext *make_tab_head_context(Side side, bool has_sibling_before, bool has_sibling_after);
    static GtkStyleContext *make_tab_head_label_context();
    static GtkStyleContext *make_toggle_switch_channel_context();
    static GtkStyleContext *make_toggle_switch_thumb_context();
    static GtkStyleContext *make_tool_button_context();
    static GtkStyleContext *make_tooltip_context();
    
    template<typename T, typename Init>
    static T init_once(T &obj, Init init) {
      if(!obj)
        obj = init();
      return obj;
    }
    
    template<typename Init>
    GtkStyleContext *init_context_once(GtkStyleContext *&control, Init init) {
      if(!control) {
        control = init();
        gtk_style_context_add_provider(control, current_theme(), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      }
      return control;
    }
    
    template<typename T>
    void unref_and_null(T *&obj) {
      if(obj) {
        g_object_unref(obj);
        obj = nullptr;
      }
    }
    
  private:
    const char *_theme_variant;
    
    GtkStyleProvider *_current_theme;
    
    GtkStyleContext *_checkbox_context;
    GtkStyleContext *_default_push_button_context;
    GtkStyleContext *_expander_arrow_context;
    GtkStyleContext *_input_field_button_context;
    GtkStyleContext *_input_field_context;
    GtkStyleContext *_list_item_context;
    GtkStyleContext *_list_item_selected_context;
    GtkStyleContext *_panel_context;
    GtkStyleContext *_popup_panel_context;
    GtkStyleContext *_progress_bar_context;
    GtkStyleContext *_progress_bar_trough_context;
    GtkStyleContext *_push_button_context;
    GtkStyleContext *_radio_button_context;
    GtkStyleContext *_horz_slider_channel_context;
    GtkStyleContext *_horz_slider_thumb_context;
    GtkStyleContext *_horz_slider_thumb_down_arrow_context;
    GtkStyleContext *_horz_slider_thumb_up_arrow_context;
    GtkStyleContext *_vert_slider_channel_context;
    GtkStyleContext *_vert_slider_thumb_context;
    GtkStyleContext *_vert_slider_thumb_left_arrow_context;
    GtkStyleContext *_vert_slider_thumb_right_arrow_context;
    GtkStyleContext *_tab_body_context;
    GtkStyleContext *_tab_head_top_background_context;
    GtkStyleContext *_tab_head_left_background_context;
    GtkStyleContext *_tab_head_right_background_context;
    GtkStyleContext *_tab_head_bottom_background_context;
    GtkStyleContext *_tab_head_other_background_context;
    GtkStyleContext *_tab_head_top_abutting_none_context;
    GtkStyleContext *_tab_head_top_abutting_right_context;
    GtkStyleContext *_tab_head_top_abutting_both_context;
    GtkStyleContext *_tab_head_top_abutting_left_context;
    GtkStyleContext *_tab_head_bottom_abutting_none_context;
    GtkStyleContext *_tab_head_bottom_abutting_right_context;
    GtkStyleContext *_tab_head_bottom_abutting_both_context;
    GtkStyleContext *_tab_head_bottom_abutting_left_context;
    GtkStyleContext *_tab_head_left_abutting_none_context;
    GtkStyleContext *_tab_head_left_abutting_below_context;
    GtkStyleContext *_tab_head_left_abutting_both_context;
    GtkStyleContext *_tab_head_left_abutting_above_context;
    GtkStyleContext *_tab_head_right_abutting_none_context;
    GtkStyleContext *_tab_head_right_abutting_below_context;
    GtkStyleContext *_tab_head_right_abutting_both_context;
    GtkStyleContext *_tab_head_right_abutting_above_context;
    GtkStyleContext *_tab_head_label_context;
    GtkStyleContext *_toggle_switch_channel_context;
    GtkStyleContext *_toggle_switch_thumb_context;
    GtkStyleContext *_tool_button_context;
    GtkStyleContext *_tooltip_context;
    
};

static MathGtkStyleContextCache mgtk_painter_cache_light("light");
static MathGtkStyleContextCache mgtk_painter_cache_dark("dark");

static MathGtkStyleContextCache &painter_cache_for(ControlContext &control) {
  if(control.is_using_dark_mode())
    return mgtk_painter_cache_dark;
  else
    return mgtk_painter_cache_light;
}

static void on_theme_changed(GObject*, GParamSpec*) {
  MathGtkControlPainter::gtk_painter.clear_cache();
  style_observations.notify_all();
  
  char *theme_name;
  g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme_name, nullptr);
  pmath_debug_print("[on_theme_changed: gtk-theme-name = %s]\n", theme_name);
  g_free(theme_name);
}

static void need_change_notifications() {
  static bool initialized_change_notifications = false;

  if(!initialized_change_notifications) {
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect(settings, "notify::gtk-theme-name",   G_CALLBACK(on_theme_changed), nullptr);
    g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(on_theme_changed), nullptr);
    initialized_change_notifications = true;
    
    char *theme_name;
    g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme_name, nullptr);
    pmath_debug_print("[gtk-theme-name = %s]\n", theme_name);
    g_free(theme_name);
  }
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
  ControlContext &control,
  Canvas         &canvas,
  ContainerType   type,
  BoxSize        *extents
) {
  if(GtkStyleContext *gtk_ctx = get_control_theme(control, type)) {
    int min_width = 0;
    int min_height = 0;
    
    switch(type) {
      case ContainerType::DefaultPushButton:
      case ContainerType::PushButton:
      case ContainerType::PaletteButton:
        if(extents->ascent < canvas.get_font_size() * 0.75f)
          extents->ascent = canvas.get_font_size() * 0.75f;
          
        if(extents->descent < canvas.get_font_size() * 0.25f)
          extents->descent = canvas.get_font_size() * 0.25f;
        break;
      
      case ContainerType::AddressBandInputField: {
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
        
      case ContainerType::AddressBandBackground: {
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
        
      case ContainerType::CheckboxUnchecked:
      case ContainerType::CheckboxChecked:
      case ContainerType::CheckboxIndeterminate:
      case ContainerType::RadioButtonUnchecked:
      case ContainerType::RadioButtonChecked: {
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
      
      case ContainerType::OpenerTriangleClosed:
      case ContainerType::OpenerTriangleOpened: {
          int size;
          gtk_style_context_get_style(gtk_ctx, "expander-size", &size, nullptr);
          
          extents->width   = 0.75f * size;
          extents->ascent  = 0.75f * extents->width;
          extents->descent = 0.25f * extents->width;
        }
        return;
      
      case ContainerType::Panel:
      case ContainerType::PopupPanel: {
          extents->width +=   12.0;
          extents->ascent +=  6.0;
          extents->descent += 6.0;
        }
        return;
      
      case ContainerType::HorizontalSliderChannel: {
          // TODO: calculate implicitly from minimum scale>contents>trough>slider height (18+2 for border), taking margins (-9 top and bottom) into account and add trough border (1 top and bottom)
          extents->width   = 8 * canvas.get_font_size();
          extents->ascent  = 4.0 * 0.75f;
          extents->descent = 0;
        }
        return;
        
      case ContainerType::HorizontalSliderThumb:
      case ContainerType::HorizontalSliderDownArrowButton:
      case ContainerType::HorizontalSliderUpArrowButton: {
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-width", &min_width, nullptr); // GTK >= 3.20.0
          if(min_width <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-length", &min_width, nullptr);
          
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-height", &min_height, nullptr); // GTK >= 3.20.0
          if(min_height <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-width", &min_height, nullptr); 
          
          extents->ascent  = min_height * 0.75f * 0.75f;
          extents->descent = min_height * 0.75f * 0.25f;
          extents->width  = min_width * 0.75f;
        }
        break;
      
      case ContainerType::VerticalSliderChannel: {
          // TODO: calculate implicitly from minimum scale>contents>trough>slider size
          extents->ascent  = 8 * canvas.get_font_size();
          extents->descent = 0;
          extents->width   = 4.0 * 0.75f;
        }
        return;
        
      case ContainerType::VerticalSliderThumb:
      case ContainerType::VerticalSliderLeftArrowButton:
      case ContainerType::VerticalSliderRightArrowButton: {
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
        
//      case ContainerType::ProgressIndicatorBackground: {
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
        
      case ContainerType::ProgressIndicatorBar: {
          GtkBorder padding;
          gtk_style_context_get_padding(gtk_ctx, GTK_STATE_FLAG_NORMAL, &padding);
          
          extents->ascent -=  padding.top    * 0.75;
          extents->descent -= padding.bottom * 0.75;
          extents->width -=   (padding.left + padding.right) * 0.75;
        }
        return;
        
      case ContainerType::NavigationBack:
      case ContainerType::NavigationForward:  {
          int w, h;
          gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
          
          extents->width   = std::max(w * 0.75,        (double)extents->width);
          extents->ascent  = std::max(h * 0.75 * 0.75, (double)extents->ascent);
          extents->descent = std::max(h * 0.25 * 0.75, (double)extents->descent);
        } 
        break;
      
      case ContainerType::ToggleSwitchChannelChecked:
      case ContainerType::ToggleSwitchChannelUnchecked: {
          GtkBorder thumb_border = {};
          if(GtkStyleContext *thumb_ctx = get_control_theme(control, type == ContainerType::ToggleSwitchChannelChecked ? ContainerType::ToggleSwitchThumbChecked : ContainerType::ToggleSwitchThumbUnchecked)) {
            gtk_style_context_get(thumb_ctx, GTK_STATE_FLAG_NORMAL, "min-width", &min_width, nullptr); // GTK >= 3.20.0
            if(min_width <= 0)
              gtk_style_context_get_style(thumb_ctx, "slider-width", &min_width, nullptr);
            
            gtk_style_context_get(thumb_ctx, GTK_STATE_FLAG_NORMAL, "min-height", &min_height, nullptr); // GTK >= 3.20.0
            if(min_height <= 0)
              gtk_style_context_get_style(thumb_ctx, "slider-height", &min_height, nullptr);
              
            thumb_border = MathGtkStyleContextCache::get_all_border_padding(thumb_ctx);
          }
          
          int em_i = (int)ceil(canvas.get_font_size());
          min_width  = 2 * std::max(min_width, em_i);
          min_height = std::max(min_height, em_i);
          
          min_width  += thumb_border.left + thumb_border.right;
          min_height += thumb_border.top + thumb_border.bottom;
          
          thumb_border = MathGtkStyleContextCache::get_all_border_padding(gtk_ctx);
          min_width  += thumb_border.left + thumb_border.right;
          min_height += thumb_border.top + thumb_border.bottom;
          
          extents->ascent  = min_height * 0.75f;
          extents->descent = 0;
          extents->width   = min_width * 0.75f;
        }
        return;
      
      case ContainerType::ToggleSwitchThumbChecked:
      case ContainerType::ToggleSwitchThumbUnchecked: {
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-width", &min_width, nullptr); // GTK >= 3.20.0
          if(min_width <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-width", &min_width, nullptr);
          
          gtk_style_context_get(gtk_ctx, GTK_STATE_FLAG_NORMAL, "min-height", &min_height, nullptr); // GTK >= 3.20.0
          if(min_height <= 0)
            gtk_style_context_get_style(gtk_ctx, "slider-height", &min_height, nullptr);
          
          int em_i = (int)ceil(canvas.get_font_size());
          min_width  = std::max(min_width, em_i);
          min_height = std::max(min_height, em_i);
          
          GtkBorder thumb_border = MathGtkStyleContextCache::get_all_border_padding(gtk_ctx);
          min_width  += thumb_border.left + thumb_border.right;
          min_height += thumb_border.top + thumb_border.bottom;
          
          extents->ascent  = min_height * 0.75f;
          extents->descent = 0;
          extents->width   = min_width * 0.75f;
        }
        return;
        
      case ContainerType::TabHeadBackground: {
          if(auto center_ctx = painter_cache_for(control).tab_body_context()) {
            GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(center_ctx);
            
            if(extents->width < 0.75f * (border.left + border.right))
              extents->width  = 0.75f * (border.left + border.right);
            
            if(extents->height() < 0.75f * border.top) {
              extents->descent = 0;
              extents->ascent  = 0.75f * border.top;
            }
            return;
          }
        } break;
        
      case ContainerType::TabPanelTopCenter: {
          if(auto center_ctx = painter_cache_for(control).tab_body_context()) {
            GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(center_ctx);
            
            if(extents->height() < 0.75f * border.top) {
              extents->descent = 0;
              extents->ascent  = 0.75f * border.top;
            }
            
            return;
          }
        } break;
        
      case ContainerType::TabPanelCenterLeft: {
          if(auto center_ctx = painter_cache_for(control).tab_body_context()) {
            GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(center_ctx);
            
            if(extents->width < 0.75f * border.left)
              extents->width  = 0.75f * border.left;
            
            return;
          }
        } break;
        
      case ContainerType::TabPanelCenterRight: {
          if(auto center_ctx = painter_cache_for(control).tab_body_context()) {
            GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(center_ctx);
            
            if(extents->width < 0.75f * border.right)
              extents->width  = 0.75f * border.right;
            
            return;
          }
        } break;
        
      case ContainerType::TabPanelBottomCenter: {
          if(auto center_ctx = painter_cache_for(control).tab_body_context()) {
            GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(center_ctx);
            
            if(extents->height() < 0.75f * border.bottom) {
              extents->descent = 0;
              extents->ascent  = 0.75f * border.bottom;
            }
            
            return;
          }
        } break;
    
      default:
        break;
    }
    
    gtk_style_context_get(
      gtk_ctx, GTK_STATE_FLAG_NORMAL, 
      "min-width",  &min_width, 
      "min-height", &min_height, 
      nullptr); // GTK >= 3.20.0
    
    extents->ascent  = std::max(extents->ascent,  min_height * 0.75f * 0.5f + 0.25f * canvas.get_font_size());
    extents->descent = std::max(extents->descent, min_height * 0.75f * 0.5f - 0.25f * canvas.get_font_size());
    extents->width   = std::max(extents->width,   min_width * 0.75f);
    
    GtkBorder border = MathGtkStyleContextCache::get_all_border_padding(gtk_ctx);
    
    extents->ascent +=  0.75f * border.top;
    extents->descent += 0.75f * border.bottom;
    extents->width +=   0.75f * (border.left + border.right);
    
    return;
  }
  
  ControlPainter::calc_container_size(control, canvas, type, extents);
}
 
Color MathGtkControlPainter::control_font_color(ControlContext &control, ContainerType type, ControlState state) {
  if(GtkStyleContext *gsc = get_control_theme(control, type, true)) {
    GtkStateFlags old_state = gtk_style_context_get_state(gsc);
    GtkStateFlags new_state = get_state_flags(control, type, state);
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
  
  return ControlPainter::control_font_color(control, type, state);
}
 
bool MathGtkControlPainter::is_very_transparent(ControlContext &control, ContainerType type, ControlState state) {
  switch(type) {
    case ContainerType::PaletteButton:
      return state == ControlState::Normal;
      
    default:
      break;
  }
  return ControlPainter::is_very_transparent(control, type, state);
}
 
void MathGtkControlPainter::draw_container(
  ControlContext &control, 
  Canvas         &canvas,
  ContainerType   type,
  ControlState    state,
  RectangleF      rect
) {
  if(canvas.suppress_output)
    return;
  
  switch(type) {
    case ContainerType::PaletteButton:
      if(state == ControlState::Normal)
        return;
      break;
    
    case ContainerType::AddressBandInputField: return;
    
    default: break;
  }
  
  if(GtkStyleContext *gsc = get_control_theme(control, type)) {
    rect.pixel_align(canvas, false);
    
    canvas.save();
    
    canvas.translate(rect.x, rect.y);
    rect.x = rect.y = 0;
    canvas.scale(0.75, 0.75);
    rect.width/= 0.75;
    rect.height/= 0.75;
    
    rect.add_rect_path(canvas);
    canvas.clip();
    
    GtkStateFlags flags = get_state_flags(control, type, state);
    painter_cache_for(control).render_container(gsc, flags, canvas, type, state, rect);
    
    canvas.restore();
    return;
  }
  
  ControlPainter::draw_container(control, canvas, type, state, rect);
}

bool MathGtkControlPainter::enable_animations() {
  GtkSettings *settings = gtk_settings_get_default();
  if(!settings)
    return false;
  
  gboolean animations = FALSE;
  g_object_get(settings, "gtk-enable-animations", &animations, NULL);
  return !!animations;
}

SharedPtr<BoxAnimation> MathGtkControlPainter::control_transition(
  FrontEndReference  widget_id,
  Canvas            &canvas,
  ContainerType      type1,
  ContainerType      type2,
  ControlState       state1,
  ControlState       state2,
  RectangleF         rect
) {
  ControlContext &control = ControlContext::find(FrontEndObject::find_cast<Box>(widget_id));
  
  if(type1 == type2 && state1 == state2)
    return nullptr;
  
  SharedPtr<BoxAnimation> result = nullptr;
  
  if(GtkStyleContext *gsc = get_control_theme(control, type2)) {
    GtkStateFlags flags = get_state_flags(control, type2, state2);
    gtk_style_context_set_state(gsc, flags);
    
    char *css = gtk_style_context_to_string(gsc, GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE);
    
    if(const char *transition_css = strstr(css, "transition")) {
      double duration = MathGtkCss::parse_transition_duration(transition_css, 0.0);
      
      if(duration > 0) {
        //pmath_debug_print("[duration: %f; %s]\n", duration, transition_css);
        
        Point p0 = canvas.current_pos();
        
        SharedPtr<FadeAnimation> anim = new FadeAnimation(
          widget_id,
          canvas,
          rect,
          duration);
        
        rect.x-= p0.x;
        rect.y-= p0.y;
        
        if(anim->box_id.is_valid() && anim->buf1 && anim->buf2) {
          if(!MathGtkCss::parse_transition_timing_function(anim->transition_function, transition_css))
            anim->transition_function = CubicBezierEasingFunction::Linear;
          
//          pmath_debug_print("[%d,%d -> %d,%d duration: %f sec; cubic-bezier(%f,%f,%f,%f)]\n", 
//            type1, state1, type2, state2, duration,
//            anim->transition_function.x1(),
//            anim->transition_function.y1(),
//            anim->transition_function.x2(),
//            anim->transition_function.y2());
          
          draw_container(
            control,
            *anim->buf1->canvas(),
            type1,
            state1,
            rect);
            
          draw_container(
            control,
            *anim->buf2->canvas(),
            type2,
            state2,
            rect);
          
          result = PMATH_CPP_MOVE(anim);
        }
//        else
//          pmath_debug_print("[%d,%d -> %d,%d no buffer]\n", type1, state1, type2, state2);
      }
//      else
//        pmath_debug_print("[%d,%d -> %d,%d no duration: %s]\n", type1, state1, type2, state2, transition_css);
    }
//    else
//      pmath_debug_print("[%d,%d -> %d,%d no transition]\n", type1, state1, type2, state2);
    
    g_free(css);
  }
    else
      pmath_debug_print("[%d,%d -> %d,%d no theme]\n", type1, state1, type2, state2);

  return result;
} 

Vector2F MathGtkControlPainter::container_content_offset(
  ControlContext &control, 
  ContainerType   type,
  ControlState    state)
{
  switch(type) {
    case ContainerType::PushButton:
    case ContainerType::DefaultPushButton:
    case ContainerType::PaletteButton:
      if(state == ControlState::PressedHovered) {
        if(GtkStyleContext *control = get_control_theme(ControlContext::dummy, type)) {
          int dx, dy;
          
          gtk_style_context_get_style(control,
                                      "child-displacement-x", &dx,
                                      "child-displacement-y", &dy,
                                      nullptr);
                                      
          return {0.75f * dx, 0.75f * dy};
        }
      }
      break;
      
    default: break;
  }
  
  return ControlPainter::container_content_offset(control, type, state);
}
 
bool MathGtkControlPainter::container_hover_repaint(ControlContext &control, ContainerType type) {
  switch(type) {
    case ContainerType::FramelessButton:
    case ContainerType::None:
    case ContainerType::Panel:
    case ContainerType::PopupPanel:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabHeadBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight:
    case ContainerType::TooltipWindow:
      return false;
    
    default:
      if(get_control_theme(control, type))
        return true;
      break;
  }
  
  return ControlPainter::container_hover_repaint(control, type);
}
 
void MathGtkControlPainter::system_font_style(ControlContext &control, StyleData *style) {
  GtkStyleContext *gsc = get_control_theme(control, ContainerType::PushButton, true);
  
  if(!gsc) {
    ControlPainter::system_font_style(control, style);
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
    style->set(FontSize, Length(size));
}
 
GtkStyleContext *MathGtkControlPainter::get_control_theme(ControlContext &control, ContainerType type, bool foreground) {
  if(gtk_check_version(3, 20, 0) != nullptr) { // older than GTK 3.20
    return nullptr;
  }
  
  need_change_notifications();
  
  style_observations.register_observer();
  switch(type) {
    case ContainerType::None:
    case ContainerType::FramelessButton:
    case ContainerType::GenericButton:
      break;
      
    case ContainerType::PushButton:                  return painter_cache_for(control).push_button_context();
    case ContainerType::DefaultPushButton:           return painter_cache_for(control).default_push_button_context();
      
    case ContainerType::NavigationBack:
    case ContainerType::NavigationForward:
    case ContainerType::PaletteButton:               return painter_cache_for(control).tool_button_context();
    
    case ContainerType::AddressBandGoButton:         return painter_cache_for(control).input_field_button_context();
      
    case ContainerType::InputField:
    case ContainerType::AddressBandInputField:
    case ContainerType::AddressBandBackground:       return painter_cache_for(control).input_field_context();
      
    case ContainerType::CheckboxUnchecked:
    case ContainerType::CheckboxChecked:
    case ContainerType::CheckboxIndeterminate:       return painter_cache_for(control).checkbox_context();
      
    case ContainerType::RadioButtonUnchecked:
    case ContainerType::RadioButtonChecked:          return painter_cache_for(control).radio_button_context();
    
    case ContainerType::Panel:                           return painter_cache_for(control).panel_context();
    case ContainerType::PopupPanel:                      return painter_cache_for(control).popup_panel_context();
    case ContainerType::ProgressIndicatorBackground:     return painter_cache_for(control).progress_bar_trough_context();
    case ContainerType::ProgressIndicatorBar:            return painter_cache_for(control).progress_bar_context();
    case ContainerType::HorizontalSliderChannel:         return painter_cache_for(control).horz_slider_channel_context();
    case ContainerType::HorizontalSliderThumb:           return painter_cache_for(control).horz_slider_thumb_context();
    case ContainerType::HorizontalSliderDownArrowButton: return painter_cache_for(control).horz_slider_thumb_down_arrow_context();
    case ContainerType::HorizontalSliderUpArrowButton:   return painter_cache_for(control).horz_slider_thumb_up_arrow_context();
    case ContainerType::VerticalSliderChannel:           return painter_cache_for(control).vert_slider_channel_context();
    case ContainerType::VerticalSliderThumb:             return painter_cache_for(control).vert_slider_thumb_context();
    case ContainerType::VerticalSliderLeftArrowButton:   return painter_cache_for(control).vert_slider_thumb_left_arrow_context();
    case ContainerType::VerticalSliderRightArrowButton:  return painter_cache_for(control).vert_slider_thumb_right_arrow_context();
    case ContainerType::TooltipWindow:                   return painter_cache_for(control).tooltip_context();
    case ContainerType::ListViewItem:                    return painter_cache_for(control).list_item_context();
    case ContainerType::ListViewItemSelected:            return painter_cache_for(control).list_item_selected_context();
    
    case ContainerType::OpenerTriangleClosed:
    case ContainerType::OpenerTriangleOpened: return painter_cache_for(control).expander_arrow_context();
    
    case ContainerType::TabPanelCenter:
    case ContainerType::TabBodyBackground:    return painter_cache_for(control).tab_body_context();
    
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabHeadBackground:    return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_top_background_context();
    case ContainerType::TabPanelCenterLeft:   return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_left_background_context();
    case ContainerType::TabPanelCenterRight:  return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_right_background_context();
    case ContainerType::TabPanelBottomCenter: return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_bottom_background_context();
    
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomRight:  return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_other_background_context();
    
    case ContainerType::TabHead:                        return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_top_abutting_none_context();
    case ContainerType::TabHeadAbuttingRight:           return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_top_abutting_right_context();
    case ContainerType::TabHeadAbuttingLeftRight:       return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_top_abutting_both_context();
    case ContainerType::TabHeadAbuttingLeft:            return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_top_abutting_left_context();
    
    case ContainerType::TabHeadBottom:                  return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_bottom_abutting_none_context();
    case ContainerType::TabHeadBottomAbuttingRight:     return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_bottom_abutting_right_context();
    case ContainerType::TabHeadBottomAbuttingLeftRight: return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_bottom_abutting_both_context();
    case ContainerType::TabHeadBottomAbuttingLeft:      return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_bottom_abutting_left_context();
    
    case ContainerType::TabHeadLeft:                    return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_left_abutting_none_context();
    case ContainerType::TabHeadLeftAbuttingBottom:      return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_left_abutting_below_context();
    case ContainerType::TabHeadLeftAbuttingTopBottom:   return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_left_abutting_both_context();
    case ContainerType::TabHeadLeftAbuttingTop:         return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_left_abutting_above_context();
    
    case ContainerType::TabHeadRight:                   return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_right_abutting_none_context();
    case ContainerType::TabHeadRightAbuttingBottom:     return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_right_abutting_below_context();
    case ContainerType::TabHeadRightAbuttingTopBottom:  return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_right_abutting_both_context();
    case ContainerType::TabHeadRightAbuttingTop:        return foreground ? painter_cache_for(control).tab_head_label_context() : painter_cache_for(control).tab_head_right_abutting_above_context();
    
    case ContainerType::ToggleSwitchChannelChecked:
    case ContainerType::ToggleSwitchChannelUnchecked: return painter_cache_for(control).toggle_switch_channel_context();
    case ContainerType::ToggleSwitchThumbChecked:
    case ContainerType::ToggleSwitchThumbUnchecked: return painter_cache_for(control).toggle_switch_thumb_context();
    
    default:
      break;
  }
  
  return nullptr;
}
 
GtkStateFlags MathGtkControlPainter::get_state_flags(ControlContext &control, ContainerType type, ControlState state) {
  int result = 0;
  if(!control.is_foreground_window())
    result = GTK_STATE_FLAG_BACKDROP;
  
  switch(type) {
    case ContainerType::Panel:
    case ContainerType::PopupPanel:
      return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_NORMAL );
    
    case ContainerType::CheckboxUnchecked:
    case ContainerType::OpenerTriangleClosed:
    case ContainerType::RadioButtonUnchecked:
    case ContainerType::ToggleSwitchChannelUnchecked:
    case ContainerType::ToggleSwitchThumbUnchecked:
      break;
    
    case ContainerType::ListViewItem: {
        switch(state) {
          case ControlState::Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_INSENSITIVE );
          case ControlState::PressedHovered: return (GtkStateFlags)( (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Hovered:        
          case ControlState::Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Pressed:        
          case ControlState::Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case ContainerType::CheckboxChecked:
    case ContainerType::OpenerTriangleOpened:
    case ContainerType::RadioButtonChecked:
    case ContainerType::ToggleSwitchChannelChecked:
    case ContainerType::ToggleSwitchThumbChecked: {
        switch(state) {
          case ControlState::Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_INSENSITIVE );
          case ControlState::PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Hovered:        
          case ControlState::Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Pressed:        
          case ControlState::Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case ContainerType::HorizontalSliderThumb:
    case ContainerType::HorizontalSliderUpArrowButton:
    case ContainerType::HorizontalSliderDownArrowButton:
    case ContainerType::VerticalSliderChannel:
    case ContainerType::VerticalSliderThumb:
    case ContainerType::VerticalSliderLeftArrowButton:
    case ContainerType::VerticalSliderRightArrowButton: {
        if(state == ControlState::Hot) // the mouse is somewhere else over the slider
          state = ControlState::Normal;
      } break;
       
    case ContainerType::ListViewItemSelected: {
        switch(state) {
          case ControlState::Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_INSENSITIVE );
          case ControlState::PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Hovered:        
          case ControlState::Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Pressed:        
          case ControlState::Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    case ContainerType::TabHead:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft: 
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft: 
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop:
    case ContainerType::TabHeadRight:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop: {
        switch(state) {
          case ControlState::Disabled:       return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_INSENSITIVE );
          case ControlState::PressedHovered: return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Hovered:        
          case ControlState::Hot:            return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_PRELIGHT );
          case ControlState::Pressed:        return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_CHECKED );
          case ControlState::Normal:         return (GtkStateFlags)( result | (int)GTK_STATE_FLAG_NORMAL );
        }
      } break;
    
    default:
      break;
  }
  
  switch(state) {
    case ControlState::Hovered:
    case ControlState::Hot:
      result|= (int)GTK_STATE_FLAG_PRELIGHT;
      break;
      
    case ControlState::Pressed:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED;
      if(control.is_focused_widget())
        result|= (int)GTK_STATE_FLAG_FOCUSED;
      break;
      
    case ControlState::PressedHovered:
      result|= (int)GTK_STATE_FLAG_ACTIVE | (int)GTK_STATE_FLAG_SELECTED | (int)GTK_STATE_FLAG_PRELIGHT;
      if(control.is_focused_widget())
        result|= (int)GTK_STATE_FLAG_FOCUSED;
      break;
      
    case ControlState::Disabled:
      result|= (int)GTK_STATE_FLAG_INSENSITIVE;
      break;
      
    case ControlState::Normal:
    default:
      result|= (int)GTK_STATE_FLAG_NORMAL;
      break;
  }
  
  switch(type) {
    case ContainerType::AddressBandBackground:
    case ContainerType::AddressBandGoButton:
    case ContainerType::AddressBandInputField:
    case ContainerType::DefaultPushButton:
    case ContainerType::InputField:
    case ContainerType::NavigationBack:
    case ContainerType::NavigationForward:
    case ContainerType::PaletteButton:
    case ContainerType::PushButton:
    case ContainerType::TabBodyBackground:
    case ContainerType::TabHeadBackground:
    case ContainerType::TabPanelTopLeft:
    case ContainerType::TabPanelTopCenter:
    case ContainerType::TabPanelTopRight:
    case ContainerType::TabPanelCenterLeft:
    case ContainerType::TabPanelCenter:
    case ContainerType::TabPanelCenterRight:
    case ContainerType::TabPanelBottomLeft:
    case ContainerType::TabPanelBottomCenter:
    case ContainerType::TabPanelBottomRight: 
      result &= ~(int)GTK_STATE_FLAG_SELECTED;
      break;
      
    case ContainerType::CheckboxIndeterminate:
      result |= (int)GTK_STATE_FLAG_INCONSISTENT;
      break;
      
    default: break;
  }
  
  return (GtkStateFlags)result;
}

GtkStyleProvider *MathGtkControlPainter::current_theme_light() {
  need_change_notifications();
  return mgtk_painter_cache_light.current_theme();
}

GtkStyleProvider *MathGtkControlPainter::current_theme_dark() {
  need_change_notifications();
  return mgtk_painter_cache_dark.current_theme();
}

void MathGtkControlPainter::clear_cache() {
  mgtk_painter_cache_light.clear();
  mgtk_painter_cache_dark.clear();
}
 
#endif
 
//} ... class MathGtkControlPainter

#if GTK_MAJOR_VERSION >= 3

//{ class MathGtkStyleContextCache ...

MathGtkStyleContextCache::MathGtkStyleContextCache(const char *theme_variant) {
  _theme_variant = theme_variant;
  _current_theme = nullptr;
  
  _checkbox_context                       = nullptr;
  _default_push_button_context            = nullptr;
  _expander_arrow_context                 = nullptr;
  _input_field_button_context             = nullptr;
  _input_field_context                    = nullptr;
  _list_item_context                      = nullptr;
  _list_item_selected_context             = nullptr;
  _panel_context                          = nullptr;
  _popup_panel_context                    = nullptr;
  _progress_bar_context                   = nullptr;
  _progress_bar_trough_context            = nullptr;
  _push_button_context                    = nullptr;
  _radio_button_context                   = nullptr;
  _horz_slider_channel_context            = nullptr;
  _horz_slider_thumb_context              = nullptr;
  _horz_slider_thumb_down_arrow_context   = nullptr;
  _horz_slider_thumb_up_arrow_context     = nullptr;
  _vert_slider_channel_context            = nullptr;
  _vert_slider_thumb_context              = nullptr;
  _vert_slider_thumb_left_arrow_context   = nullptr;
  _vert_slider_thumb_right_arrow_context  = nullptr;
  _tab_body_context                       = nullptr;
  _tab_head_top_background_context        = nullptr;
  _tab_head_left_background_context       = nullptr;
  _tab_head_right_background_context      = nullptr;
  _tab_head_bottom_background_context     = nullptr;
  _tab_head_other_background_context      = nullptr;
  _tab_head_top_abutting_none_context     = nullptr;
  _tab_head_top_abutting_right_context    = nullptr;
  _tab_head_top_abutting_both_context     = nullptr;
  _tab_head_top_abutting_left_context     = nullptr;
  _tab_head_bottom_abutting_none_context  = nullptr;
  _tab_head_bottom_abutting_right_context = nullptr;
  _tab_head_bottom_abutting_both_context  = nullptr;
  _tab_head_bottom_abutting_left_context  = nullptr;
  _tab_head_left_abutting_none_context    = nullptr;
  _tab_head_left_abutting_below_context   = nullptr;
  _tab_head_left_abutting_both_context    = nullptr;
  _tab_head_left_abutting_above_context   = nullptr;
  _tab_head_right_abutting_none_context   = nullptr;
  _tab_head_right_abutting_below_context  = nullptr;
  _tab_head_right_abutting_both_context   = nullptr;
  _tab_head_right_abutting_above_context  = nullptr;
  _tab_head_label_context                 = nullptr;
  _toggle_switch_channel_context          = nullptr;
  _toggle_switch_thumb_context            = nullptr;
  _tool_button_context                    = nullptr;
  _tooltip_context                        = nullptr;
}

MathGtkStyleContextCache::~MathGtkStyleContextCache() {
  clear();
}

void MathGtkStyleContextCache::clear() {
  unref_and_null(_current_theme);
  
  unref_and_null(_checkbox_context);
  unref_and_null(_default_push_button_context);
  unref_and_null(_expander_arrow_context);
  unref_and_null(_input_field_button_context);
  unref_and_null(_input_field_context);
  unref_and_null(_list_item_context);
  unref_and_null(_list_item_selected_context);
  unref_and_null(_panel_context);
  unref_and_null(_popup_panel_context);
  unref_and_null(_progress_bar_context);
  unref_and_null(_progress_bar_trough_context);
  unref_and_null(_push_button_context);
  unref_and_null(_radio_button_context);
  unref_and_null(_horz_slider_channel_context);
  unref_and_null(_horz_slider_thumb_context);
  unref_and_null(_horz_slider_thumb_down_arrow_context);
  unref_and_null(_horz_slider_thumb_up_arrow_context);
  unref_and_null(_vert_slider_channel_context);
  unref_and_null(_vert_slider_thumb_context);
  unref_and_null(_vert_slider_thumb_left_arrow_context);
  unref_and_null(_vert_slider_thumb_right_arrow_context);
  unref_and_null(_tab_body_context);
  unref_and_null(_tab_head_top_background_context);
  unref_and_null(_tab_head_left_background_context);
  unref_and_null(_tab_head_right_background_context);
  unref_and_null(_tab_head_bottom_background_context);
  unref_and_null(_tab_head_other_background_context);
  unref_and_null(_tab_head_top_abutting_none_context);
  unref_and_null(_tab_head_top_abutting_right_context);
  unref_and_null(_tab_head_top_abutting_both_context);
  unref_and_null(_tab_head_top_abutting_left_context);
  unref_and_null(_tab_head_bottom_abutting_none_context);
  unref_and_null(_tab_head_bottom_abutting_right_context);
  unref_and_null(_tab_head_bottom_abutting_both_context);
  unref_and_null(_tab_head_bottom_abutting_left_context);
  unref_and_null(_tab_head_left_abutting_none_context);
  unref_and_null(_tab_head_left_abutting_below_context);
  unref_and_null(_tab_head_left_abutting_both_context);
  unref_and_null(_tab_head_left_abutting_above_context);
  unref_and_null(_tab_head_right_abutting_none_context);
  unref_and_null(_tab_head_right_abutting_below_context);
  unref_and_null(_tab_head_right_abutting_both_context);
  unref_and_null(_tab_head_right_abutting_above_context);
  unref_and_null(_tab_head_label_context);
  unref_and_null(_toggle_switch_channel_context);
  unref_and_null(_toggle_switch_thumb_context);
  unref_and_null(_tool_button_context);
  unref_and_null(_tooltip_context);
}

void MathGtkStyleContextCache::render_container(
  GtkStyleContext *ctx, 
  GtkStateFlags    flags,
  Canvas          &canvas,
  ContainerType    type,
  ControlState     state,
  RectangleF       rect
) {
  //gtk_style_context_save(ctx); // changes the widget path and thus does not always work (e.g. with Adwaita notebook tabs)
  gtk_style_context_set_state(ctx, flags);
  
  switch(type) {
    case ContainerType::CheckboxUnchecked:
    case ContainerType::CheckboxChecked:
    case ContainerType::CheckboxIndeterminate:
      render_all_common_inset(ctx, canvas, rect);
      gtk_render_check(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
      break;
      
    case ContainerType::RadioButtonUnchecked:
    case ContainerType::RadioButtonChecked:
      render_all_common_inset(ctx, canvas, rect);
      gtk_render_option(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
      break;
      
    case ContainerType::OpenerTriangleClosed:
    case ContainerType::OpenerTriangleOpened:
      //gtk_render_background(ctx, canvas.cairo(), x, y, width, height);
      gtk_render_expander(  ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
      break;
    
    case ContainerType::NavigationBack:
    case ContainerType::NavigationForward: {
        float cx = rect.x + rect.width/2;
        float cy = rect.y + rect.height/2;
        rect.width = rect.height = std::min(rect.width, rect.height);
        rect.x = cx - rect.width/2;
        rect.y = cy - rect.height/2;
      
        canvas.align_point(&rect.x, &rect.y, false);
        
        render_all_common_inset_const(ctx, canvas, rect);
        
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
        int w, h;
        gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
        int icon_size = std::min(w, h);
        
        GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(icon_theme, type == ContainerType::NavigationBack ? "go-previous-symbolic" : "go-next-symbolic", icon_size, (GtkIconLookupFlags)0);
        GdkPixbuf *pixbuf = gtk_icon_info_load_symbolic_for_context(icon_info, ctx, nullptr, nullptr);
        g_object_unref(icon_info);
        
        canvas.save();
        gtk_render_icon(ctx, canvas.cairo(), pixbuf, cx - w * 0.5, cy - h * 0.5);
        canvas.restore();
        gdk_pixbuf_unref(pixbuf);
        
//          if(GtkIconSet *icon = gtk_icon_factory_lookup_default(type == ContainerType::NavigationBack ? "go-previous" : "go-next")) {
//            GdkPixbuf *pixbuf = gtk_icon_set_render_icon_pixbuf(icon, ctx, GTK_ICON_SIZE_SMALL_TOOLBAR);
//            
//            gtk_render_icon(ctx, canvas.cairo(), pixbuf, cx - w, cy - h);
//            
//            gdk_pixbuf_unref(pixbuf);
//          }
      } break;
    
    case ContainerType::TabBodyBackground: {
        //gtk_style_context_get_border(ctx, GTK_STATE_FLAG_NORMAL, &border);
        GtkBorder border = get_all_border_padding(ctx);
        
        //float top_hide = border.top;// + canvas.get_font_size() / 0.75; // add font size for possible border radius
        rect.grow(Side::Top, border.top);
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    case ContainerType::TabPanelCenter: {
        GtkBorder border = get_all_border_padding(ctx);
        
        rect.grow(Margins<float>{border.left, border.right, border.top, border.bottom});
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    case ContainerType::TabPanelTopLeft: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.width  += 100;
          body_rect.x      = rect.right() - border.left;
          body_rect.y      = rect.bottom() - border.top;
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelTopCenter: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.y      = rect.bottom() - border.top;
          body_rect.grow(Side::Left,  border.left);
          body_rect.grow(Side::Right, border.right);
          
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelTopRight: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.width  += 100;
          body_rect.x      = rect.left() - body_rect.width + border.left;
          body_rect.y      = rect.bottom() - border.top;
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelCenterLeft: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.width += 100;
          body_rect.x      = rect.right() - border.left;
          body_rect.grow(Side::Top,    border.top);
          body_rect.grow(Side::Bottom, border.bottom);
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelCenterRight: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.width += 100;
          body_rect.x      = rect.left() - body_rect.width + border.right;
          body_rect.grow(Side::Top,    border.top);
          body_rect.grow(Side::Bottom, border.bottom);
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelBottomLeft: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.width  += 100;
          body_rect.x      = rect.right() - border.left;
          body_rect.y      = rect.top() - body_rect.height + border.bottom;
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelBottomCenter: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.y      = rect.top() - body_rect.height + border.bottom;
          body_rect.grow(Side::Left,  border.left);
          body_rect.grow(Side::Right, border.right);
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabPanelBottomRight: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.width  += 100;
          body_rect.x      = rect.left() - body_rect.width + border.left;
          body_rect.y      = rect.top() - body_rect.height + border.bottom;
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabHeadBackground: {
        if(auto body_ctx = tab_body_context()) {
          GtkBorder border = get_all_border_padding(body_ctx);
          auto body_rect   = rect;
          body_rect.height += 100;
          body_rect.y      = rect.bottom() - border.top;
          //rect.grow(Side::Bottom, -border.top);
          rect.grow(Side::Left,   -border.left); // TODO: also consider border radius
          rect.grow(Side::Right,  -border.right);
          render_all_common_inset(body_ctx, canvas, body_rect);
        }
        
        if(canvas.glass_background && !canvas.show_only_text) {
          canvas.show_only_text = true;
          render_all_common_inset_const(ctx, canvas, rect);
          canvas.show_only_text = false;
        }
        else {
          render_all_common_inset_const(ctx, canvas, rect);
        }
      } break;
    
    case ContainerType::TabHead:
    case ContainerType::TabHeadAbuttingRight:
    case ContainerType::TabHeadAbuttingLeftRight:
    case ContainerType::TabHeadAbuttingLeft: {
        GtkBorder margin;
        gtk_style_context_get_margin(ctx, flags, &margin);
        
        rect.grow(Side::Top, -margin.top);
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    case ContainerType::TabHeadBottom:
    case ContainerType::TabHeadBottomAbuttingRight:
    case ContainerType::TabHeadBottomAbuttingLeftRight:
    case ContainerType::TabHeadBottomAbuttingLeft: {
        GtkBorder margin;
        gtk_style_context_get_margin(ctx, flags, &margin);
        
        rect.grow(Side::Bottom, -margin.bottom);
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    case ContainerType::TabHeadLeft:
    case ContainerType::TabHeadLeftAbuttingBottom:
    case ContainerType::TabHeadLeftAbuttingTopBottom:
    case ContainerType::TabHeadLeftAbuttingTop: {
        GtkBorder margin;
        gtk_style_context_get_margin(ctx, flags, &margin);
        
        
        rect.grow(Side::Left, -margin.left);
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    case ContainerType::TabHeadRight:
    case ContainerType::TabHeadRightAbuttingBottom:
    case ContainerType::TabHeadRightAbuttingTopBottom:
    case ContainerType::TabHeadRightAbuttingTop: {
        GtkBorder margin;
        gtk_style_context_get_margin(ctx, flags, &margin);
        
        
        rect.grow(Side::Right, -margin.right);
        
        render_all_common_inset_const(ctx, canvas, rect);
      } break;
    
    default:
      render_all_common_inset_const(ctx, canvas, rect);
      break;
  }
  
  //gtk_style_context_restore(ctx); 
}

void MathGtkStyleContextCache::render_all_common(GtkStyleContext *ctx, Canvas &canvas, const RectangleF &rect) {
  if(!ctx)
    return;
  
  render_all_common(gtk_style_context_get_parent(ctx), canvas, rect);
  
  if(!canvas.suppress_output) {
    if(!canvas.show_only_text) {
      gtk_render_background(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
    }
    gtk_render_frame(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
  }
}

void MathGtkStyleContextCache::render_all_common_inset(GtkStyleContext *ctx, Canvas &canvas, RectangleF &rect) {
  if(!ctx)
    return;
  
  if(gtk_style_context_has_class(ctx, "pmath-dummy"))
    return;
  
  GtkStyleContext *parent = gtk_style_context_get_parent(ctx);
  render_all_common_inset(parent, canvas, rect);
  if(parent) {
    GtkBorder margin;
    gtk_style_context_get_margin(ctx, gtk_style_context_get_state(ctx), &margin);
    rect.x +=      margin.left;
    rect.y +=      margin.top;
    rect.width -=  margin.left + margin.right;
    rect.height -= margin.top + margin.bottom;
  }
  if(!canvas.suppress_output) {
    if(!canvas.show_only_text) {
      gtk_render_background(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
    }
    gtk_render_frame(ctx, canvas.cairo(), rect.x, rect.y, rect.width, rect.height);
  }
  
  GtkBorder border;
  gtk_style_context_get_border(ctx, gtk_style_context_get_state(ctx), &border);
  rect.x +=      border.left;
  rect.y +=      border.top;
  rect.width -=  border.left + border.right;
  rect.height -= border.top + border.bottom;
  
  gtk_style_context_get_padding(ctx, gtk_style_context_get_state(ctx), &border);
  rect.x +=      border.left;
  rect.y +=      border.top;
  rect.width -=  border.left + border.right;
  rect.height -= border.top + border.bottom;
}

GtkBorder MathGtkStyleContextCache::get_all_border_padding(GtkStyleContext *ctx) {
  if(!ctx) 
    return {0,0,0,0};
  
  if(gtk_style_context_has_class(ctx, "pmath-dummy"))
    return {0,0,0,0};
  
  GtkStyleContext *parent = gtk_style_context_get_parent(ctx);
  GtkBorder padding = get_all_border_padding(parent);
  if(parent) {
    GtkBorder margin;
    gtk_style_context_get_margin(ctx, gtk_style_context_get_state(ctx), &margin);
    padding.left   += margin.left;
    padding.right  += margin.right;
    padding.top    += margin.top;
    padding.bottom += margin.bottom;
  }
  
  GtkBorder border;
  gtk_style_context_get_padding(ctx, gtk_style_context_get_state(ctx), &border);
  padding.left   += border.left;
  padding.right  += border.right;
  padding.top    += border.top;
  padding.bottom += border.bottom;
  
  gtk_style_context_get_border(ctx, gtk_style_context_get_state(ctx), &border);
  padding.left   += border.left;
  padding.right  += border.right;
  padding.top    += border.top;
  padding.bottom += border.bottom;
  
  return padding;
}

GtkStyleContext *MathGtkStyleContextCache::make_context_from_path_and_free(GtkWidgetPath *path, GtkStyleContext *parent) {
  GtkStyleContext *control = gtk_style_context_new();
  gtk_style_context_set_path(control, path);
  gtk_widget_path_unref(path);
  if(parent) {
    gtk_style_context_set_parent(control, parent);
    g_object_unref(parent);
  }
  return control;
}

GtkStyleProvider *MathGtkStyleContextCache::make_current_theme_provider(const char *variant) {
  char *theme_name;
  g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme_name, nullptr);
  pmath_debug_print("[gtk-theme-name = %s]\n", theme_name);
  
  // TODO: if there is no theme with that name, Gtk falls back to "Adwaita"
  
  GtkCssProvider *provider = gtk_css_provider_get_named(theme_name, variant);
  
  pmath_debug_print("[theme(%s, %s) = %p]\n", theme_name, variant ? variant : "(null)", provider);
  
  g_free(theme_name);
  
  return GTK_STYLE_PROVIDER(g_object_ref(provider));
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
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
  GtkStyleContext *window = make_context_from_path_and_free(path);
  gtk_style_context_add_class(window, "pmath-dummy"); // only here for its forground color definition, not for its frame/border/...
  path = gtk_widget_path_copy(gtk_style_context_get_path(window));

  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
  gtk_widget_path_iter_set_object_name(path, -1, "list");
  
  GtkStyleContext *list = make_context_from_path_and_free(path, window);
  gtk_style_context_add_class(list, "pmath-dummy"); // only here for its forground color definition, not for its frame/border/...
  path = gtk_widget_path_copy(gtk_style_context_get_path(list));
  
  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_path_iter_set_object_name(path, -1, "row");
  gtk_widget_path_iter_add_class(path, -1, "activatable");
  
//  gtk_widget_path_append_type(path, GTK_TYPE_LABEL);
//  gtk_widget_path_iter_set_object_name(path, -1, "label");
  
  return make_context_from_path_and_free(path, list);
  
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
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
  GtkStyleContext *window = make_context_from_path_and_free(path);
  gtk_style_context_add_class(window, "pmath-dummy"); // only here for its forground color definition, not for its frame/border/...
  path = gtk_widget_path_copy(gtk_style_context_get_path(window));
  
  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX);
  gtk_widget_path_iter_set_object_name(path, -1, "list");
  
  GtkStyleContext *list = make_context_from_path_and_free(path, window);
  gtk_style_context_add_class(list, "pmath-dummy"); // only here for its forground color definition, not for its frame/border/...
  path = gtk_widget_path_copy(gtk_style_context_get_path(list));
  
  gtk_widget_path_append_type(path, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_path_iter_set_object_name(path, -1, "row");
  gtk_widget_path_iter_add_class(path, -1, "activatable");
  
  return make_context_from_path_and_free(path, list);
  
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
  return make_context_from_path_and_free(path);
//  GtkStyleContext *frame_context = make_context_from_path_and_free(path);
//  
//  path = gtk_widget_path_copy(gtk_style_context_get_path(frame_context));
//  gtk_widget_path_append_type(path, G_TYPE_NONE);
//  gtk_widget_path_iter_set_object_name(path, -1, "border");
//  
//  return make_context_from_path_and_free(path, frame_context);

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

GtkStyleContext *MathGtkStyleContextCache::make_popup_panel_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  //gtk_widget_path_iter_add_class(path, -1, "popup");
  
  GtkStyleContext *window = make_context_from_path_and_free(path);
  path = gtk_widget_path_copy(gtk_style_context_get_path(window));
  
  gtk_widget_path_append_type(path, GTK_TYPE_FRAME);
  gtk_widget_path_iter_set_object_name(path, -1, "frame");
  gtk_widget_path_iter_add_class(path, -1, "background");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  return make_context_from_path_and_free(path, window);
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

GtkStyleContext *MathGtkStyleContextCache::make_slider_channel_context(bool vertical) {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "scale");
  gtk_widget_path_iter_add_class(path, -1, vertical ? "vertical" : "horizontal");
  
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
//  //int pos = gtk_widget_path_append_type(path, vertical ? GTK_TYPE_VSCALE : GTK_TYPE_HSCALE);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
//  
//  gtk_style_context_set_path(    slider_channel_context, path);
//  gtk_style_context_set_screen(  slider_channel_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(slider_channel_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_SCALE);
//  gtk_style_context_add_class(   slider_channel_context, vertical ? GTK_STYLE_CLASS_VERTICAL : GTK_STYLE_CLASS_HORIZONTAL);
//  gtk_style_context_add_class(   slider_channel_context, GTK_STYLE_CLASS_TROUGH);
//
//  gtk_widget_path_unref(path);
//  return slider_channel_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_slider_thumb_context(bool vertical, bool marks_before, bool marks_after) {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
  gtk_widget_path_iter_set_object_name(path, -1, "scale");
  gtk_widget_path_iter_add_class(path, -1, vertical ? "vertical" : "horizontal");
  if(marks_before)
    gtk_widget_path_iter_add_class(path, -1, "marks-before");
  if(marks_after)
    gtk_widget_path_iter_add_class(path, -1, "marks-after");
  
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
//  //int pos = gtk_widget_path_append_type(path, vertical ? GTK_TYPE_VSCALE : GTK_TYPE_HSCALE);
//  gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SCALE);
//
//  gtk_style_context_set_path(    slider_thumb_context, path);
//  gtk_style_context_set_screen(  slider_thumb_context, gdk_screen_get_default());
//  gtk_style_context_add_provider(slider_thumb_context, GTK_STYLE_PROVIDER(gtk_settings_get_default()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
//  gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SCALE);
//  gtk_style_context_add_class(   slider_thumb_context, vertical ? GTK_STYLE_CLASS_VERTICAL : GTK_STYLE_CLASS_HORIZONTAL);
//  gtk_style_context_add_class(   slider_thumb_context, GTK_STYLE_CLASS_SLIDER);
//
//  gtk_widget_path_unref(path);
//  return slider_thumb_context;
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_body_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_set_object_name(path, -1, "window");
  gtk_widget_path_iter_add_class(path, -1, "background");
  
  GtkStyleContext *window = make_context_from_path_and_free(path);
  path = gtk_widget_path_copy(gtk_style_context_get_path(window));
  
  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  //gtk_widget_path_iter_add_class(path, -1, "background");
  //return make_context_from_path_and_free(path);
  GtkStyleContext *notebook = make_context_from_path_and_free(path, window);
  //return notebook;
  
  path = gtk_widget_path_copy(gtk_style_context_get_path(notebook));
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "stack");
  return make_context_from_path_and_free(path, notebook);
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_head_background_context(Side side) {
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
  switch(side) {
    case Side::Left:   gtk_widget_path_iter_add_class(path, -1, "left"); break;
    case Side::Top:    gtk_widget_path_iter_add_class(path, -1, "top"); break;
    case Side::Right:  gtk_widget_path_iter_add_class(path, -1, "right"); break;
    case Side::Bottom: gtk_widget_path_iter_add_class(path, -1, "bottom"); break;
  }
  return make_context_from_path_and_free(path);
}

GtkStyleContext *MathGtkStyleContextCache::make_tab_head_context(Side side, bool has_sibling_before, bool has_sibling_after) {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
  gtk_widget_path_iter_set_object_name(path, -1, "notebook");
  gtk_widget_path_iter_add_class(path, -1, "frame");
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "header");
  switch(side) {
    case Side::Left:   gtk_widget_path_iter_add_class(path, -1, "left"); break;
    case Side::Top:    gtk_widget_path_iter_add_class(path, -1, "top"); break;
    case Side::Right:  gtk_widget_path_iter_add_class(path, -1, "right"); break;
    case Side::Bottom: gtk_widget_path_iter_add_class(path, -1, "bottom"); break;
  }
  
  gtk_widget_path_append_type(path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name(path, -1, "tabs");
  
//  GtkStyleContext *tabs = make_context_from_path_and_free(path);
//  path = gtk_widget_path_copy(gtk_style_context_get_path(tabs));
  
  if(has_sibling_before || has_sibling_after) {
    GtkWidgetPath *sibling_path = gtk_widget_path_new();
    
    if(has_sibling_before) {
      gtk_widget_path_append_type(sibling_path, G_TYPE_NONE);
      gtk_widget_path_iter_set_object_name(sibling_path, -1, "tab");
    }
    
    gtk_widget_path_append_type(sibling_path, G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name(sibling_path, -1, "tab");
    
    if(has_sibling_after) {
      gtk_widget_path_append_type(sibling_path, G_TYPE_NONE);
      gtk_widget_path_iter_set_object_name(sibling_path, -1, "tab");
    }
    
    gtk_widget_path_append_with_siblings(path, sibling_path, has_sibling_before ? 1 : 0);
    gtk_widget_path_unref(sibling_path);
  }
  else {
    gtk_widget_path_append_type(path, G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name(path, -1, "tab");
  }
  
//  return make_context_from_path_and_free(path, tabs);
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

GtkStyleContext *MathGtkStyleContextCache::make_toggle_switch_channel_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SWITCH);
  gtk_widget_path_iter_set_object_name(path, -1, "switch");
  
  return make_context_from_path_and_free(path);
}

GtkStyleContext *MathGtkStyleContextCache::make_toggle_switch_thumb_context() {
  GtkWidgetPath *path = gtk_widget_path_new();
  gtk_widget_path_append_type(path, GTK_TYPE_SWITCH);
  gtk_widget_path_iter_set_object_name(path, -1, "switch");
  
  gtk_widget_path_append_type(path, GTK_TYPE_SWITCH);
  gtk_widget_path_iter_set_object_name(path, -1, "slider");
  
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

//} ... class MathGtkStyleContextCache

#endif
