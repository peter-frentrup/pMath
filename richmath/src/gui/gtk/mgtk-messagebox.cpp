#include <gui/gtk/mgtk-messagebox.h>


#include <boxes/section.h>

#include <gui/documents.h>

#include <gui/gtk/mgtk-widget.h>
#include <gui/gtk/mgtk-control-painter.h>
#include <gui/gtk/mgtk-icons.h>

#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#  ifdef None
#    undef None
#  endif
#endif


using namespace richmath;

namespace richmath { namespace strings {
  extern String Head;
  extern String Location;
}}

extern pmath_symbol_t richmath_System_Abort;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Dialog;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_Developer_SourceLocationOpenerFunction;
extern pmath_symbol_t richmath_FE_CallFrontEnd;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_SystemOpenDirectory;

namespace {
  class MathGtkHyperlinks {
    public:
      MathGtkHyperlinks();
      
      String register_hyperlink_action(Expr action);
      
      void connect(GtkWidget *label);
      void disconnect(GtkWidget *label);
    
    private:
      bool on_activate_link(GtkLabel *label, const char *uri);
    
    private:
      Expr hyperlink_actions;
    
    private:
      static gboolean activate_link_callback(GtkLabel *label, gchar *uri, gpointer user_data);
  };
}

static GtkWidget *make_left_aligned(GtkWidget *widget, Margins<unsigned> padding) {
  GtkWidget *alignment = gtk_alignment_new(0.0f, 0.0f, 0.0f, 0.0f);
  gtk_container_add(GTK_CONTAINER(alignment), widget);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), padding.top, padding.bottom, padding.left, padding.right);
  return alignment;
}

static GtkWidget *make_left_aligned(GtkWidget *widget) {
  return make_left_aligned(widget, Margins<unsigned>(10));
}

static void mark_action_with_css_class(GtkWidget *dialog, GtkResponseType response_id, const char *css_class) {
#if GTK_MAJOR_VERSION >= 3
  if(GtkWidget *button = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), response_id)) {
    GtkStyleContext *style_context = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(style_context, css_class);
  }
#endif
}

static void mark_suggested_action(GtkWidget *dialog, GtkResponseType response_id) {
  mark_action_with_css_class(dialog, response_id, "suggested-action");
}

static void mark_destructive_action(GtkWidget *dialog, GtkResponseType response_id) {
  mark_action_with_css_class(dialog, response_id, "destructive-action");
}

YesNoCancel richmath::mgtk_ask_save(Document *doc, String question) {
  GtkWindow *owner_window = nullptr;
  if(doc) {
    if(MathGtkWidget *mwid = dynamic_cast<MathGtkWidget*>(doc->native())) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(mwid->widget());
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    GTK_STOCK_SAVE, // "_Save", 
    GTK_RESPONSE_YES,
    "Do_n't save", // GTK_STOCK_DISCARD,
    GTK_RESPONSE_NO,
    GTK_STOCK_CANCEL, // "_Cancel",
    GTK_RESPONSE_CANCEL,
    nullptr);
  
  mark_suggested_action(  dialog, GTK_RESPONSE_YES);
  mark_destructive_action(dialog, GTK_RESPONSE_NO);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
  
  char *utf8_question = pmath_string_to_utf8(question.get_as_string(), nullptr);
  GtkWidget *label = gtk_label_new(utf8_question ? utf8_question : "Save changes?");
  
  MathGtkIcons need_app_icons;
  GdkPixbuf *ico = need_app_icons.get_icon(MathGtkIcons::AppIcon48Index);
  GtkWidget *image = gtk_image_new_from_pixbuf(ico);
  g_object_unref(ico);

  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *icon_and_label = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_and_label), make_left_aligned(image, Margins<unsigned>(10, 10, 10, 0)), FALSE, FALSE, 0);
  gtk_box_pack_end(  GTK_BOX(icon_and_label), make_left_aligned(label, Margins<unsigned>(10, 20)), TRUE,  TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), icon_and_label);
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = mgtk_themed_dialog_run(doc ? *doc->native() : ControlContext::dummy, GTK_DIALOG(dialog));
  pmath_mem_free(utf8_question);
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case GTK_RESPONSE_YES: return YesNoCancel::Yes;
    case GTK_RESPONSE_NO:  return YesNoCancel::No;
    default:               return YesNoCancel::Cancel;
  }
}

YesNoCancel richmath::mgtk_ask_remove_private_style_definitions(Document *doc) {
  GtkWindow *owner_window = nullptr;
  if(doc) {
    if(MathGtkWidget *mwid = dynamic_cast<MathGtkWidget*>(doc->native())) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(mwid->widget());
      if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
        owner_window = GTK_WINDOW(toplevel);
    }
  }
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    "_Delete private style definitions",
    GTK_RESPONSE_YES,
    "_Keep private style definitions", // GTK_STOCK_DISCARD,
    GTK_RESPONSE_NO,
    nullptr);
  
  mark_suggested_action(  dialog, GTK_RESPONSE_NO);
  mark_destructive_action(dialog, GTK_RESPONSE_YES);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
  
  GtkWidget *label = gtk_label_new(
    "<big><b>Replace private style definitions?</b></big>\n"
    "\n"
    "This document has private style definitions.\n"
    "Are you sure you want to replace them with a shared stylesheet?");
  gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
  
  MathGtkIcons need_app_icons;
  GdkPixbuf *ico = need_app_icons.get_icon(MathGtkIcons::AppIcon48Index);
  GtkWidget *image = gtk_image_new_from_pixbuf(ico);
  g_object_unref(ico);

  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *icon_and_label = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_and_label), make_left_aligned(image, Margins<unsigned>(10, 10, 10, 0)), FALSE, FALSE, 0);
  gtk_box_pack_end(  GTK_BOX(icon_and_label), make_left_aligned(label, Margins<unsigned>(10, 20)), TRUE,  TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), icon_and_label);
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case GTK_RESPONSE_YES: return YesNoCancel::Yes;
    case GTK_RESPONSE_NO:  return YesNoCancel::No;
    default:               return YesNoCancel::Cancel;
  }
}

bool richmath::mgtk_ask_open_suspicious_system_file(String path) {
  MathGtkHyperlinks hyperlinks;
  GtkWindow *owner_window = nullptr;
  
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(!doc)
    doc = Documents::selected_document();
  
  if(MathGtkWidget *mwid = doc ? dynamic_cast<MathGtkWidget*>(doc->native()) : nullptr) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(mwid->widget());
    if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
      owner_window = GTK_WINDOW(toplevel);
  }
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    "_Open anyway",
    GTK_RESPONSE_YES,
    "Do _not open suspicious file",
    GTK_RESPONSE_NO,
    nullptr);
  
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
  
  GtkWidget *caption_label = gtk_label_new("<big><b>Open suspicous file?</b></big>");
  gtk_label_set_use_markup(GTK_LABEL(caption_label), TRUE);
  GtkWidget *desctiption_label = nullptr;
  
  MathGtkIcons need_app_icons;
  GdkPixbuf *ico = need_app_icons.get_icon(MathGtkIcons::AppIcon48Index);
  GtkWidget *image = gtk_image_new_from_pixbuf(ico);
  g_object_unref(ico);

  GtkWidget *text_content = gtk_vbox_new(FALSE, 0);
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *icon_and_text = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_and_text), make_left_aligned(image, Margins<unsigned>(10, 10, 10, 0)), FALSE, FALSE, 0);
  gtk_box_pack_end(  GTK_BOX(icon_and_text), make_left_aligned(text_content, Margins<unsigned>(0)), TRUE,  TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), icon_and_text);
  
  gtk_container_add(GTK_CONTAINER(text_content), make_left_aligned(caption_label));
  
  String rich_question;
  rich_question = String("The file ") + String::FromChar(0x201C);
  rich_question+= "<a href=\"";
  rich_question+= hyperlinks.register_hyperlink_action(
                    Call(
                      Symbol(richmath_FE_CallFrontEnd), 
                      Call(
                        Symbol(richmath_FrontEnd_SystemOpenDirectory), 
                        path)));
  rich_question+= "\">";
  rich_question+= path;
  rich_question+= "</a>";
  rich_question+= String::FromChar(0x201D) + " has an unrecognized file type.\n";
  rich_question+= "Do you really want to open it?";
  rich_question+= String::FromChar(0);
  
  if(rich_question) {
    if(char *utf8 = pmath_string_to_utf8(rich_question.get(), nullptr)) {
      desctiption_label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(desctiption_label), utf8);
      pmath_mem_free(utf8);
      
      gtk_container_add(GTK_CONTAINER(text_content), make_left_aligned(desctiption_label));
      
      hyperlinks.connect(desctiption_label);
    }
  }
  
  gtk_widget_show_all(GTK_WIDGET(content_box));
  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy(dialog);
  return result == GTK_RESPONSE_YES;
}

Expr richmath::mgtk_ask_interrupt(Expr stack) {
  MathGtkHyperlinks hyperlinks;
  GtkWindow *owner_window = nullptr;
  
  Box *box = Box::find_nearest_box(Application::get_evaluation_object());
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    doc = Documents::selected_document();
  
  if(MathGtkWidget *mwid = doc ? dynamic_cast<MathGtkWidget*>(doc->native()) : nullptr) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(mwid->widget());
    if(gtk_widget_is_toplevel(toplevel) && GTK_IS_WINDOW(toplevel))
      owner_window = GTK_WINDOW(toplevel);
  }
  
  const int ResponseAbort = 1;
  const int ResponseEnterSubsession = 2;
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Richmath",
    owner_window,
    GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    "_Abort", 
    ResponseAbort,
    "_Enter subsession", // GTK_STOCK_DISCARD,
    ResponseEnterSubsession,
    "_Continue", 
    GTK_RESPONSE_CANCEL,
    nullptr);
  
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
  
  GtkWidget *caption_label = gtk_label_new("<big><b>An interrupt occurred</b></big>");
  gtk_label_set_use_markup(GTK_LABEL(caption_label), TRUE);
  GtkWidget *desctiption_label = nullptr;
  GtkWidget *details_section = nullptr;
  GtkWidget *details_label = nullptr;
  
  MathGtkIcons need_app_icons;
  GdkPixbuf *ico = need_app_icons.get_icon(MathGtkIcons::AppIcon48Index);
  GtkWidget *image = gtk_image_new_from_pixbuf(ico);
  g_object_unref(ico);

  GtkWidget *text_content = gtk_vbox_new(FALSE, 0);
  
  GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *icon_and_text = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_and_text), make_left_aligned(image, Margins<unsigned>(10, 10, 10, 0)), FALSE, FALSE, 0);
  gtk_box_pack_end(  GTK_BOX(icon_and_text), make_left_aligned(text_content, Margins<unsigned>(0)), TRUE,  TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content_box), icon_and_text);
  
  
  //GtkWidget *content_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  //gtk_container_set_border_width(GTK_CONTAINER(text_content), 5);
  //gtk_box_set_spacing(GTK_BOX(text_content), 5);
  
  //gtk_box_pack_end(GTK_BOX(text_content), caption_label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(text_content), make_left_aligned(caption_label, Margins<unsigned>(10, 20)));
  
  String content;
  if(box) {
    if(Section *sect = box->find_parent<Section>(true)) {
      content = "During evaluation of <a href=\"";
      content+= hyperlinks.register_hyperlink_action(
                  Call(Symbol(richmath_FE_CallFrontEnd),
                    Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
                    Symbol(richmath_System_Automatic),
                    sect->id().to_pmath())));
      content+= "\">";
      content+= sect->get_own_style(SectionLabel, "?");
      content+= "</a>";
    }
  }
  
  String details;
  if(stack.item_equals(0, richmath_System_List) && stack.expr_length() > 1) {
    Expr default_name = String("?");
    
    for(size_t i = stack.expr_length() - 1; i > 0; --i) {
      Expr frame = stack[i];
      
      if(details)
        details+= "\n";
      
      String name = frame.lookup(strings::Head, default_name).to_string();
      bool have_link = false;
      Expr location {};
      if(frame.try_lookup(strings::Location, location)) {
        location = Application::interrupt_wait(Call(Symbol(richmath_Developer_SourceLocationOpenerFunction), PMATH_CPP_MOVE(location)));
        
        if(location.item_equals(0, richmath_System_Function)) {
          if(location.expr_length() == 1)
            location = location[1];
          else
            location = Call(location);
          
          details+= "<a href=\"";
          details+= hyperlinks.register_hyperlink_action(location);
          details+= "\">";
          have_link = true;
        }
      }
      details+= name;
      if(have_link)
        details+= "</a>";
    }
  }
  
  if(content.is_string()) {
    if(char *utf8 = pmath_string_to_utf8(content.get(), nullptr)) {
      desctiption_label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(desctiption_label), utf8);
      pmath_mem_free(utf8);
      
      gtk_container_add(GTK_CONTAINER(text_content), make_left_aligned(desctiption_label));
      
      hyperlinks.connect(desctiption_label);
    }
  }
  
  if(details) {
    if(char *utf8 = pmath_string_to_utf8(details.get(), nullptr)) {
      details_section = gtk_expander_new_with_mnemonic("Stack _trace:");
      GtkWidget *details_label = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(details_label), utf8);
      pmath_mem_free(utf8);
      
      gtk_container_add(GTK_CONTAINER(details_section), make_left_aligned(details_label));
      gtk_container_add(GTK_CONTAINER(text_content), make_left_aligned(details_section));
      
      hyperlinks.connect(details_label);
    }
  }
  
  gtk_widget_show_all(GTK_WIDGET(content_box));
  
  int result = mgtk_themed_dialog_run(GTK_DIALOG(dialog));
  
  hyperlinks.disconnect(details_label);
  hyperlinks.disconnect(desctiption_label);
  
  gtk_widget_destroy(dialog);
  switch(result) {
    case ResponseAbort:           return Call(Symbol(richmath_System_Abort));
    case ResponseEnterSubsession: return Call(Symbol(richmath_System_Dialog));
  }
  
  return Expr();
}

int richmath::mgtk_themed_dialog_run(ControlContext &ctx, GtkDialog *dialog) {
#if GTK_MAJOR_VERSION >= 3
  bool dark = ctx.is_using_dark_mode();
  GtkStyleProvider *style_provider = dark ? MathGtkControlPainter::gtk_painter.current_theme_dark() : MathGtkControlPainter::gtk_painter.current_theme_light();
  
  BasicGtkWidget::internal_forall_recursive(
    GTK_WIDGET(dialog),
    [=](GtkWidget *w) { 
      gtk_style_context_add_provider(
        gtk_widget_get_style_context(w), style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
     });
#ifdef GDK_WINDOWING_X11
  {
    GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(dialog));
    if(GDK_IS_X11_WINDOW(gdk_window)) {
      gdk_x11_window_set_theme_variant(gdk_window, (char*)(dark ? "dark" : "light"));
    }
  }
#endif // GDK_WINDOWING_X11
#endif // GTK_MAJOR_VERSION

  int result = gtk_dialog_run(dialog);
  
#if GTK_MAJOR_VERSION >= 3
  BasicGtkWidget::internal_forall_recursive(
    GTK_WIDGET(dialog),
    [=](GtkWidget *w) { 
      gtk_style_context_remove_provider(
        gtk_widget_get_style_context(w), style_provider);
     });
#endif // GTK_MAJOR_VERSION
  
  return result;
}

int richmath::mgtk_themed_dialog_run(GtkDialog *dialog) {
#if GTK_MAJOR_VERSION >= 3
  ControlContext *cc = Box::find_nearest_parent<ControlContext>(Application::get_evaluation_object());
  if(!cc) {
    if(auto doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object()))
      cc = doc->native();
  }
  
  if(!cc)
    cc = &ControlContext::dummy;
  
  return mgtk_themed_dialog_run(*cc, dialog);
#else
  return gtk_dialog_run(dialog);
#endif
}

//{ class MathGtkHyperlinks ...

MathGtkHyperlinks::MathGtkHyperlinks() {
}

String MathGtkHyperlinks::register_hyperlink_action(Expr action) {
  if(hyperlink_actions.is_null())
    hyperlink_actions = List(PMATH_CPP_MOVE(action));
  else
    hyperlink_actions.append(PMATH_CPP_MOVE(action));
  
  return Expr(hyperlink_actions.expr_length()).to_string();
}

void MathGtkHyperlinks::connect(GtkWidget *label) {
  if(!label)
    return;
    
  g_signal_connect(label, "activate-link", G_CALLBACK(activate_link_callback), this);
}

void MathGtkHyperlinks::disconnect(GtkWidget *label) {
  if(!label)
    return;
  
  g_signal_handlers_disconnect_by_data(label, this);
}

bool MathGtkHyperlinks::on_activate_link(GtkLabel *label, const char *uri) {
  size_t i = 0;
  const char *s = uri;
  while(*s >= '0' && *s <= '9' && i < hyperlink_actions.expr_length()) {
    i = 10*i + (*s - '0');
    ++s;
  }
  
  if(*s == '\0' && i >= 1 && i <= hyperlink_actions.expr_length()) {
    Expr action = hyperlink_actions[i];
    Application::interrupt_wait(action);
    return true;
  }
  return false;
}

gboolean MathGtkHyperlinks::activate_link_callback(GtkLabel *label, gchar *uri, gpointer user_data) {
  MathGtkHyperlinks *self = (MathGtkHyperlinks*)user_data;
  return self->on_activate_link(label, uri);
}

//} ... class MathGtkHyperlinks
