#include <gui/gtk/mgtk-colordialog.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <gui/gtk/mgtk-widget.h>
#include <gui/documents.h>


using namespace richmath;
using namespace pmath;

extern pmath_symbol_t richmath_System_DollarCanceled;

//{ class MathGtkColorDialog ...

#if GTK_MAJOR_VERSION >= 3

static Expr color_chooser_dialog_show(Color initialcolor) {
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
  
  if(initialcolor.is_valid()) {
    GdkRGBA color;
    
    color.alpha = 1.0;
    color.red   = initialcolor.red();
    color.green = initialcolor.green();
    color.blue  = initialcolor.blue();
    
    gtk_color_chooser_set_rgba(chooser, &color);
  }
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        GdkRGBA color;
        
        gtk_color_chooser_get_rgba(chooser, &color);
        
        gtk_widget_destroy(GTK_WIDGET(dialog));
        
        // ignoring alpha
        return Color::from_rgb(color.red, color.green, color.blue).to_pmath();
      }
  }
  
  gtk_widget_destroy(GTK_WIDGET(dialog));
  
  return Symbol(richmath_System_DollarCanceled);
}

#endif

static Expr color_selection_dialog_show(Color initialcolor) {
  GtkColorSelectionDialog *dialog;
  GtkColorSelection       *widget;

  dialog = GTK_COLOR_SELECTION_DIALOG(gtk_color_selection_dialog_new(nullptr));
  widget = GTK_COLOR_SELECTION(       gtk_color_selection_dialog_get_color_selection(dialog));

  gtk_color_selection_set_has_opacity_control(widget, FALSE);

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

  int result = gtk_dialog_run(GTK_DIALOG(dialog));

  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        Color col;

#if GTK_MAJOR_VERSION >= 3
        {
          GdkRGBA color;

          gtk_color_selection_get_current_rgba(widget, &color);

          // ignoring alpha
          col = Color::from_rgb(color.red, color.green, color.blue);
        }
#else
        {
          GdkColor color;

          gtk_color_selection_get_current_color(widget, &color);
          
          col = Color::from_rgb(color.red / (double)0xffff, color.green / (double)0xffff, color.blue / (double)0xffff);
        }
#endif

        gtk_widget_destroy(GTK_WIDGET(dialog));

        return col.to_pmath();
      }
  }
  
  gtk_widget_destroy(GTK_WIDGET(dialog));

  return Symbol(richmath_System_DollarCanceled);
}

Expr MathGtkColorDialog::show(Color initialcolor) {
#if GTK_MAJOR_VERSION >= 3
  return color_chooser_dialog_show(initialcolor);
#endif
  return color_selection_dialog_show(initialcolor);
}

//} ... class MathGtkColorDialog
