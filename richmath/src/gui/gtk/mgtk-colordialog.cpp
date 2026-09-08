#include <gui/gtk/mgtk-colordialog.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/documents.h>

#include <gui/gtk/mgtk-widget.h>
#include <gui/gtk/mgtk-messagebox.h>

#include <util/autovaluereset.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

namespace {
  struct MathGtkColorDialogHook {
    static MathGtkColorDialogHook *current_hook;
  public:
    MathGtkColorDialog *dialog;
    Color prev_color;
    
  public:
    Expr show_color_selection_dialog(Color initialcolor);
    static Color color_from_selection_widget(GtkColorSelection *widget);
  
  private:
    static void color_selection_changed_cb(GtkColorSelection *sender, void *user_data);
    
    void on_color_selection_changed(GtkColorSelection *sender);
    
    void update_color(Color current_color);
    
#if GTK_MAJOR_VERSION >= 3
  public:
    Expr show_color_chooser_dialog(Color initialcolor);
    static Color color_from_chooser_widget(GtkColorChooser *widget);
    
  private:
    static void color_activated_cb(GtkColorChooser *sender, GdkRGBA *color, void *user_data);
    
    void on_color_activated(GtkColorChooser *sender, const GdkRGBA &color);
#endif
  };
}

//{ class MathGtkColorDialog ...

Expr MathGtkColorDialog::show_impl(Color initialcolor) {
  MathGtkColorDialogHook hook;
  hook.dialog = this;
  hook.prev_color = Color::None;
  
  AutoValueReset<MathGtkColorDialogHook*> auto_hook(MathGtkColorDialogHook::current_hook);
  MathGtkColorDialogHook::current_hook = &hook;
  
#if GTK_MAJOR_VERSION >= 3
  return hook.show_color_chooser_dialog(initialcolor);
#endif
  return hook.show_color_selection_dialog(initialcolor);
}

//} ... class MathGtkColorDialog

//{ class MathGtkColorDialogHook ...

MathGtkColorDialogHook *MathGtkColorDialogHook::current_hook = nullptr;

Color MathGtkColorDialogHook::color_from_selection_widget(GtkColorSelection *widget) {
#if GTK_MAJOR_VERSION >= 3
  {
    GdkRGBA color;

    gtk_color_selection_get_current_rgba(widget, &color);

    // ignoring alpha
    return Color::from_rgb(color.red, color.green, color.blue);
  }
#else
  {
    GdkColor color;

    gtk_color_selection_get_current_color(widget, &color);
    
    return Color::from_rgb(color.red / (double)0xffff, color.green / (double)0xffff, color.blue / (double)0xffff);
  }
#endif
}

#if GTK_MAJOR_VERSION >= 3

Color MathGtkColorDialogHook::color_from_chooser_widget(GtkColorChooser *widget) {
  GdkRGBA color;
  
  gtk_color_chooser_get_rgba(widget, &color);
  
  // ignoring alpha
  return Color::from_rgb(color.red, color.green, color.blue);
}

Expr MathGtkColorDialogHook::show_color_chooser_dialog(Color initialcolor) {
  GtkColorChooserDialog *dialog;
  GtkColorChooser       *chooser;
  
  GtkWindow *parent_window = nullptr;
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(!doc)
    doc = Documents::selected_document();
  
  if(doc) {
    if(auto widget = dynamic_cast<MathGtkWidget *>(doc->native())) {
      if(GtkWidget *wid = widget->widget())
        parent_window = GTK_WINDOW(gtk_widget_get_ancestor(wid, GTK_TYPE_WINDOW));
    }
  }
  
  dialog = GTK_COLOR_CHOOSER_DIALOG(gtk_color_chooser_dialog_new(nullptr, parent_window));
  chooser = GTK_COLOR_CHOOSER(dialog);
  
  gtk_color_chooser_set_use_alpha(chooser, FALSE);
  
  g_signal_connect(chooser, "color-activated", G_CALLBACK(color_activated_cb), this);
  
  if(initialcolor.is_valid()) {
    GdkRGBA color;
    
    color.alpha = 1.0;
    color.red   = initialcolor.red();
    color.green = initialcolor.green();
    color.blue  = initialcolor.blue();
    
    gtk_color_chooser_set_rgba(chooser, &color);
  }
  
  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));
  
  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        Color col = color_from_chooser_widget(chooser);
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return col.to_pmath();
      }
  }
  
  gtk_widget_destroy(GTK_WIDGET(dialog));
  
  return Symbol(richmath_System_DollarCanceled);
}

void MathGtkColorDialogHook::color_activated_cb(GtkColorChooser *sender, GdkRGBA *color, void *user_data) {
  ((MathGtkColorDialogHook*)user_data)->on_color_activated(sender, *color);
}

void MathGtkColorDialogHook::on_color_activated(GtkColorChooser *sender, const GdkRGBA &color) {
  // ignoring alpha
  update_color(Color::from_rgb(color.red, color.green, color.blue));
}

#endif // GTK_MAJOR_VERSION >= 3

Expr MathGtkColorDialogHook::show_color_selection_dialog(Color initialcolor) {
  GtkColorSelectionDialog *dialog;
  GtkColorSelection       *widget;

  dialog = GTK_COLOR_SELECTION_DIALOG(gtk_color_selection_dialog_new(nullptr));
  widget = GTK_COLOR_SELECTION(       gtk_color_selection_dialog_get_color_selection(dialog));

  gtk_color_selection_set_has_opacity_control(widget, FALSE);

  g_signal_connect(widget, "color-changed", G_CALLBACK(color_selection_changed_cb), this);
  
  if(initialcolor.is_valid()) {
#if GTK_MAJOR_VERSION >= 3
    {
      GdkRGBA color;

      color.alpha = 1.0;
      color.red   = initialcolor.red();
      color.green = initialcolor.green();
      color.blue  = initialcolor.blue();

      gtk_color_selection_set_current_rgba(widget, &color);
    }
#else
    {
      GdkColor color;

      color.pixel = gdk_rgb_xpixel_from_rgb(initialcolor.to_rgb24());
      color.red   = (uint16_t)(initialcolor.red()   * 0xffff + 0.5);
      color.green = (uint16_t)(initialcolor.green() * 0xffff + 0.5);
      color.blue  = (uint16_t)(initialcolor.blue()  * 0xffff + 0.5);

      gtk_color_selection_set_current_color(widget, &color);
    }
#endif
  }

  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));

  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        Color col = color_from_selection_widget(widget);
        
        gtk_widget_destroy(GTK_WIDGET(dialog));

        return col.to_pmath();
      }
  }
  
  gtk_widget_destroy(GTK_WIDGET(dialog));

  return Symbol(richmath_System_DollarCanceled);
}

void MathGtkColorDialogHook::color_selection_changed_cb(GtkColorSelection *sender, void *user_data) {
  ((MathGtkColorDialogHook*)user_data)->on_color_selection_changed(sender);
}

void MathGtkColorDialogHook::on_color_selection_changed(GtkColorSelection *sender) {
  update_color(color_from_selection_widget(sender));
}

void MathGtkColorDialogHook::update_color(Color current_color) {
  if(current_color != prev_color) {
    prev_color = current_color;
    dialog->set_color(current_color);
  }
}

//} ... class MathGtkColorDialogHook
