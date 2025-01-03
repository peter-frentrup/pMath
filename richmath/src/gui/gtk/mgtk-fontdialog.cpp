#include <gui/gtk/mgtk-fontdialog.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <gui/documents.h>
#include <gui/gtk/mgtk-widget.h>
#include <gui/gtk/mgtk-messagebox.h>


using namespace richmath;
using namespace pmath;


extern pmath_symbol_t richmath_System_DollarCanceled;
extern pmath_symbol_t richmath_System_List;

//{ class MathGtkFontDialog ...

#if GTK_MAJOR_VERSION >= 3

static Expr font_chooser_dialog_show(Style initial_style) {
  GtkFontChooserDialog *dialog;
  GtkFontChooser       *chooser;

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
  
  dialog  = GTK_FONT_CHOOSER_DIALOG(gtk_font_chooser_dialog_new(nullptr, parent_window));
  chooser = GTK_FONT_CHOOSER(dialog);

  if(initial_style) {
    PangoFontDescription *desc;
    char                 *utf8_name = nullptr;

    desc = pango_font_description_new();
    Expr families;
    if(initial_style.get(FontFamilies, &families)) {
      String family(families);
      
      if(families.item_equals(0, richmath_System_List)) {
        for(size_t i = 1;i <= families.expr_length();++i){
          family = String(families[i]);
          
          if(FontInfo::font_exists_similar(family))
            break;
        }
      }
      
      utf8_name = pmath_string_to_utf8(family.get_as_string(), nullptr);
      if(utf8_name)
        pango_font_description_set_family_static(desc, utf8_name);
    }
    
    Length size = SymbolicSize::Invalid;
    if(initial_style.get(FontSize, &size) && size.is_explicit_abs() && size.explicit_abs_value() >= 1) {
      pango_font_description_set_absolute_size(desc, size.explicit_abs_value() * PANGO_SCALE);
    }

    int weight = FontWeightPlain;
    int slant  = FontSlantPlain;

    bool has_weight = initial_style.get(FontWeight, &weight);
    bool has_slant  = initial_style.get(FontSlant, &slant);
    if(has_weight || has_slant) {
      pango_font_description_set_style( desc, slant  != FontSlantPlain ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
      pango_font_description_set_weight(desc, weight >= FontWeightBold ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
    }

    gtk_font_chooser_set_font_desc(chooser, desc);

    pango_font_description_free(desc);
    pmath_mem_free(utf8_name);
  }

  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));

  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        Style result_style;

        PangoFontDescription *desc;

        desc = gtk_font_chooser_get_font_desc(chooser);
        if(desc) {
          PangoFontMask set_fields = pango_font_description_get_set_fields(desc);

          if(const char *utf8_name = pango_font_description_get_family(desc))
            result_style.set(FontFamilies, String::FromUtf8(utf8_name));

          if(set_fields & PANGO_FONT_MASK_WEIGHT) {
            PangoWeight weight = pango_font_description_get_weight(desc);

            result_style.set(FontWeight, weight >= PANGO_WEIGHT_BOLD ? FontWeightBold  : FontWeightPlain);
          }

          if(set_fields & PANGO_FONT_MASK_STYLE) {
            PangoStyle style = pango_font_description_get_style(desc);

            result_style.set(FontSlant, style == PANGO_STYLE_NORMAL ? FontSlantPlain : FontSlantItalic);
          }

          if(set_fields & PANGO_FONT_MASK_SIZE) {
            int size = pango_font_description_get_size(desc);

            if(size > 0)
              result_style.set(FontSize, Length((float)size / PANGO_SCALE));
          }

          pango_font_description_free(desc);
        }

        gtk_widget_destroy(GTK_WIDGET(dialog));

        Gather g;
        result_style.emit_to_pmath(false);
        return g.end();
      }
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));

  return Symbol(richmath_System_DollarCanceled);
}

#else

#if !GTK_CHECK_VERSION(2,22,0)
static GtkWidget *gtk_font_selection_dialog_get_font_selection(GtkFontSelectionDialog *fsd){
  return fsd->fontsel;
}
#endif

static Expr split_family_names(const char *str_utf8){
  Gather g;
  
  while(*str_utf8){
    const char *next = str_utf8;
    while(*next && *next != ',')
      ++next;
    
    Gather::emit(String::FromUtf8(str_utf8, (int)(next - str_utf8)));
    
    str_utf8 = next;
    if(*str_utf8)
      ++str_utf8;
  }
  
  Expr e = g.end();
  if(e.expr_length() == 1)
    return e[1];
  
  return e;
}

static Expr font_selection_dialog_show(Style initial_style) {
  GtkFontSelectionDialog *dialog;
  GtkFontSelection       *widget;

  dialog = GTK_FONT_SELECTION_DIALOG(gtk_font_selection_dialog_new(nullptr));
  widget = GTK_FONT_SELECTION(       gtk_font_selection_dialog_get_font_selection(dialog));

  if(initial_style) {
    PangoFontDescription *desc;
    char                 *utf8_name = nullptr;

    desc = pango_font_description_new();
    
    Expr families;
    if(initial_style.get(FontFamilies, &families)) {
      String family(families);
      
      if(families.item_equals(0, richmath_System_List)){
        for(size_t i = 1;i <= families.expr_length();++i){
          String fam(families[i]);
          
          if(FontInfo::font_exists_similar(fam)) {
            if(family.length() > 0)
              family+= ",";
            
            family+= fam;
          }
        }
      }
      
      utf8_name = pmath_string_to_utf8(family.get_as_string(), nullptr);
      if(utf8_name)
        pango_font_description_set_family_static(desc, utf8_name);
    }

    Length size = SymbolicSize::Invalid;
    if(initial_style.get(FontSize, &size) && size.is_explicit_abs() && size.explicit_abs_value() >= 1) {
      pango_font_description_set_absolute_size(desc, size.explicit_abs_value() * PANGO_SCALE);
    }

    int weight = FontWeightPlain;
    int slant  = FontSlantPlain;

    bool has_weight = initial_style.get(FontWeight, &weight);
    bool has_slant  = initial_style.get(FontSlant, &slant);
    if(has_weight || has_slant) {
      pango_font_description_set_style( desc, slant  != FontSlantPlain ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
      pango_font_description_set_weight(desc, weight >= FontWeightBold ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
    }

    if(char *desc_str = pango_font_description_to_string(desc)) {
      gtk_font_selection_set_font_name(widget, desc_str);

      g_free(desc_str);
    }

    pango_font_description_free(desc);
    pmath_mem_free(utf8_name);
  }

  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));

  switch(result) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_OK: {
        Style result_style;
        
        if(char *desc_str = gtk_font_selection_dialog_get_font_name(dialog)) {
          PangoFontDescription *desc;
          PangoFontMask         set_fields;

          desc       = pango_font_description_from_string(desc_str);
          set_fields = pango_font_description_get_set_fields(desc);

          if(const char *utf8_name = pango_font_description_get_family(desc))
            result_style.set(FontFamilies, split_family_names(utf8_name));

          if(set_fields & PANGO_FONT_MASK_WEIGHT) {
            PangoWeight weight = pango_font_description_get_weight(desc);
            result_style.set(FontWeight, weight >= PANGO_WEIGHT_BOLD ? FontWeightBold  : FontWeightPlain);
          }

          if(set_fields & PANGO_FONT_MASK_STYLE) {
            PangoStyle style = pango_font_description_get_style(desc);
            result_style.set(FontSlant, style == PANGO_STYLE_NORMAL ? FontSlantPlain : FontSlantItalic);
          }

          if(set_fields & PANGO_FONT_MASK_SIZE) {
            int size = pango_font_description_get_size(desc);
            if(size > 0)
              result_style.set(FontSize, Length((float)size / PANGO_SCALE));
          }

          pango_font_description_free(desc);
          g_free(desc_str);
        }

        gtk_widget_destroy(GTK_WIDGET(dialog));

        Gather g;
        result_style.emit_to_pmath(false);
        return g.end();
      }
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));

  return Symbol(richmath_System_DollarCanceled);
}

#endif

Expr MathGtkFontDialog::show(Style initial_style) {
#if GTK_MAJOR_VERSION >= 3
  return font_chooser_dialog_show(PMATH_CPP_MOVE(initial_style));
#else
  return font_selection_dialog_show(PMATH_CPP_MOVE(initial_style));
#endif
}

//} ... class MathGtkFontDialog
