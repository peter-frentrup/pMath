#include <gui/gtk/mgtk-colordialog.h>

#include <eval/application.h>
#include <gui/gtk/mgtk-widget.h>


using namespace richmath;
using namespace pmath;


//{ class MathGtkColorDialog ...

#if GTK_MAJOR_VERSION >= 3

static Expr color_chooser_dialog_show(int initialcolor) {
  GtkColorChooserDialog *dialog;
  GtkColorChooser       *chooser;
  
  GtkWindow *parent_window = 0;
  Box *box = Application::get_evaluation_box();
  if(box) {
    Document *doc = box->find_parent<Document>(true);
    
    if(doc) {
      MathGtkWidget *widget = dynamic_cast<MathGtkWidget *>(doc->native());
      
      if(widget) {
        GtkWidget *wid = widget->widget();
        
        if(wid)
          parent_window = GTK_WINDOW(gtk_widget_get_ancestor(wid, GTK_TYPE_WINDOW));
      }
    }
  }
  
  dialog = GTK_COLOR_CHOOSER_DIALOG(gtk_color_chooser_dialog_new(NULL, parent_window));
  chooser = GTK_COLOR_CHOOSER(dialog);
  
  gtk_color_chooser_set_use_alpha(chooser, FALSE);
  
  if(initialcolor >= 0) {
    GdkRGBA color;
    
    color.alpha = 1.0;
    color.red   = (uint16_t)(((initialcolor & 0xff0000) >> 16) / 255.0);
    color.green = (uint16_t)(((initialcolor & 0x00ff00) >>  8) / 255.0);
    color.blue  = (uint16_t)( (initialcolor & 0x0000ff)        / 255.0);
    
    gtk_color_chooser_set_rgba(chooser, &color);
  }
  
  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  
  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        GdkRGBA color;
        
        gtk_color_chooser_get_rgba(chooser, &color);
        
        int col = ((int)(color.red   * 0xff) << 16) |
                  ((int)(color.green * 0xff) <<  8) |
                  ((int)(color.blue  * 0xff));
                  
        gtk_widget_destroy(GTK_WIDGET(dialog));
        
        return color_to_pmath(col);
      }
  }
  
  //if(err)
  //  return Symbol(PMATH_SYMBOL_ABORTED);
  
  gtk_widget_destroy(GTK_WIDGET(dialog));
  
  return Symbol(PMATH_SYMBOL_CANCELED);
}

#else

static Expr color_selection_dialog_show(int initialcolor) {
  GtkColorSelectionDialog *dialog;
  GtkColorSelection       *widget;

  dialog = GTK_COLOR_SELECTION_DIALOG(gtk_color_selection_dialog_new(NULL));
  widget = GTK_COLOR_SELECTION(       gtk_color_selection_dialog_get_color_selection(dialog));

  gtk_color_selection_set_has_opacity_control(widget, FALSE);

  if(initialcolor >= 0) {
#if GTK_MAJOR_VERSION >= 3
    {
      GdkRGBA color;

      color.alpha = 1.0;
      color.red   = (uint16_t)(((initialcolor & 0xff0000) >> 16) / 255.0);
      color.green = (uint16_t)(((initialcolor & 0x00ff00) >>  8) / 255.0);
      color.blue  = (uint16_t)( (initialcolor & 0x0000ff)        / 255.0);

      gtk_color_selection_set_current_rgba(widget, &color);
    }
#else
    {
      GdkColor color;

      color.pixel = gdk_rgb_xpixel_from_rgb(initialcolor);
      color.red   = (uint16_t)(((initialcolor & 0xff0000) >> 16) * 0xffff / 0xff);
      color.green = (uint16_t)(((initialcolor & 0x00ff00) >>  8) * 0xffff / 0xff);
      color.blue  = (uint16_t)( (initialcolor & 0x0000ff)        * 0xffff / 0xff);

      gtk_color_selection_set_current_color(widget, &color);
    }
#endif
  }

  int result = gtk_dialog_run(GTK_DIALOG(dialog));

  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        int col;

#if GTK_MAJOR_VERSION >= 3
        {
          GdkRGBA color;

          gtk_color_selection_get_current_rgba(widget, &color);

          col = ((int)(color.red   * 0xff) << 16) |
                ((int)(color.green * 0xff) <<  8) |
                ((int)(color.blue  * 0xff));
        }
#else
        {
          GdkColor color;

          gtk_color_selection_get_current_color(widget, &color);

          col = (((int)color.red   * 0xff / 0xffff) << 16) |
                (((int)color.green * 0xff / 0xffff) <<  8) |
                ( (int)color.blue  * 0xff / 0xffff);
        }
#endif

        gtk_widget_destroy(GTK_WIDGET(dialog));

        return color_to_pmath(col);
      }


  }

  //if(err)
  //  return Symbol(PMATH_SYMBOL_ABORTED);

  gtk_widget_destroy(GTK_WIDGET(dialog));

  return Symbol(PMATH_SYMBOL_CANCELED);
}

#endif

Expr MathGtkColorDialog::show(int initialcolor) {
#if GTK_MAJOR_VERSION >= 3
  return color_chooser_dialog_show(initialcolor);
#else
  return color_selection_dialog_show(initialcolor);
#endif
}

//} ... class MathGtkColorDialog
