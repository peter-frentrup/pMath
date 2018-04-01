#if defined( RICHMATH_USE_WIN32_FONT ) + defined( RICHMATH_USE_FT_FONT ) != 1
#  error either RICHMATH_USE_WIN32_FONT or RICHMATH_USE_FT_FONT must be defined
#endif

#if defined( RICHMATH_USE_WIN32_GUI ) + defined( RICHMATH_USE_GTK_GUI ) != 1
#  error either RICHMATH_USE_WIN32_GUI or RICHMATH_USE_GTK_GUI must be defined
#endif


#define __STDC_FORMAT_MACROS
#define WINVER 0x0500 // RegisterFontResoureceEx

#include <cmath>
#include <cstdio>

#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/binding.h>
#include <eval/application.h>
#include <eval/server.h>
#include <graphics/config-shaper.h>
#include <graphics/ot-math-shaper.h>
#include <gui/control-painter.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-clipboard.h>
#  include <gui/win32/win32-document-window.h>
#  include <gui/win32/win32-menu.h>
#  include <gui/win32/win32-touch.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-clipboard.h>
#  include <gui/gtk/mgtk-document-window.h>
#  include <gui/gtk/mgtk-menu-builder.h>
#  include <gui/gtk/mgtk-tooltip-window.h>
#endif

#include <gui/document.h>

#include <resources.h>

#undef STRICT

#include <pango/pangocairo.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <pango/pangowin32.h>
#  include <windows.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gtk/gtk.h>
#endif

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


using namespace richmath;

static void write_section(Document *doc, Expr expr) {
  Box *b = doc->selection_box();
  int i  = doc->selection_end();
  while(b && b != doc) {
    i = b->index();
    b = b->parent();
  }
  
  if(!b) {
    b = doc;
    i = doc->length();
  }
  
  doc->insert(i, Section::create_from_object(expr));
  
  doc->move_to(b, i + 1);
}

static void write_text_section(Document *doc, String style, String text) {
  write_section(doc,
                Call(
                  Symbol(PMATH_SYMBOL_SECTION),
                  text,
                  style));
}

static void todo(Document *doc, String msg) {
  write_text_section(doc, "Todo", msg);
}

static void load_aliases(
  Expr                                  aliases,
  Hashtable<String, Expr, object_hash> *implicit_macros,
  Hashtable<String, Expr, object_hash> *explicit_macros
) {
  Hashtable<String, Expr, object_hash> *table = explicit_macros;
  
#ifdef PMATH_DEBUG_LOG
  double start = pmath_tickcount();
#endif
  
  if(aliases[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= aliases.expr_length(); ++i) {
      Expr rule = aliases[i];
      if(rule.is_rule()) {
        String lhs = rule[1];
        Expr   rhs = rule[2];
        
        if(lhs.equals("Explicit"))
          table = explicit_macros;
        else if(lhs.equals("Implicit"))
          table = implicit_macros;
        else
          continue;
          
        if(rhs[0] == PMATH_SYMBOL_LIST) {
          for(size_t j = 1; j <= rhs.expr_length(); ++j) {
            Expr def = rhs[j];
            
            if(def.is_rule()) {
              String name = def[1];
              
              if(name.length() > 0)
                table->set(name, def[2]);
            }
          }
        }
      }
    }
  }
  
#ifdef PMATH_DEBUG_LOG
  pmath_debug_print("Loaded aliases in %f seconds.\n", pmath_tickcount() - start);
#endif
}

static void os_init() {
#ifdef PMATH_OS_WIN32
  {
    HMODULE kernel32;
    
    // do not show message boxes on LoadLibrary errors:
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    
    // remove current directory from dll search path:
    kernel32 = GetModuleHandleW(L"Kernel32");
    if(kernel32) {
      BOOL (WINAPI * SetDllDirectoryW_ptr)(const WCHAR *);
      SetDllDirectoryW_ptr = (BOOL (WINAPI *)(const WCHAR *))
      GetProcAddress(kernel32, "SetDllDirectoryW");
      
      if(SetDllDirectoryW_ptr)
        SetDllDirectoryW_ptr(L"");
    }
  }
#endif
}

static void message_dialog(const char *title, const char *content) {
#ifdef RICHMATH_USE_WIN32_GUI
  MessageBoxA(
    0,
    content,
    title,
    MB_OK | MB_ICONERROR);
#endif
    
#ifdef RICHMATH_USE_GTK_GUI
  {
    GtkWidget *dialog = nullptr;
    
    dialog = gtk_message_dialog_new(
               nullptr,
               GTK_DIALOG_MODAL,
               GTK_MESSAGE_ERROR,
               GTK_BUTTONS_OK,
               "%s",
               title);
               
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", content);
    
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
#endif
}

static void load_fonts() {
  Expr font_files = Evaluate(Parse("FE`$PrivateStartupFontFiles"));
  
  if(font_files[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1;i <= font_files.expr_length();++i) {
      String filename(font_files[i]);
      
      if(FontInfo::add_private_font(filename)) {
        pmath_debug_print_object("add private font ", filename.get(), "\n");
      }
      else {
        Expr arg = font_files[i];
        pmath_debug_print_object("failed to add private font ", arg.get(), "\n");
      }
    }
  }
}

static void load_math_shapers() {
  PMATH_RUN(
    "ParallelScan("
    " FileNames("
    "  ToFileName({FE`$FrontEndDirectory,\"resources\"},\"shapers\"),"
    "  \"*.pmath\"),"
    "FE`AddConfigShaper)"
  );
  
  Expr prefered_fonts = Evaluate(Parse("FE`$MathShapers"));
  
  SharedPtr<MathShaper> shaper;
  SharedPtr<MathShaper> def;
  
  if(prefered_fonts[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= prefered_fonts.expr_length(); ++i) {
      String s(prefered_fonts[i]);
      
      shaper = MathShaper::available_shapers[s];
      
      if(!shaper) {
#ifdef PMATH_DEBUG_LOG
        double start_font = pmath_tickcount();
#endif
        
        shaper = OTMathShaper::try_register(s);
        if(shaper.is_valid()) {
          MathShaper::available_shapers.set(s, shaper);
          
#ifdef PMATH_DEBUG_LOG
          pmath_debug_print_object("loaded ", s.get(), "");
          pmath_debug_print(" in %f seconds\n", pmath_tickcount() - start_font);
#endif
        }
      }
      
      if(shaper && !def)
        def = shaper;
    }
  }
  
  if(def) {
    MathShaper::available_shapers.default_value = def;
  }
  else if(MathShaper::available_shapers.size() > 0) {
    for(int i = 0;; ++i) {
      if(auto e = MathShaper::available_shapers.entry(i)) {
        MathShaper::available_shapers.default_value = e->value;
        break;
      }
    }
  }
}

static void init_stylesheet() {
#define CAPTION_FONT List(String("Calibri"), String("Verdana"),    String("Arial"))
#define TEXT_FONT    List(String("Georgia"), String("Constantia"), String("Times New Roman"))

  Stylesheet::Default = new Stylesheet;
  
  Stylesheet::Default->base = new Style;
  Stylesheet::Default->base->set(Background,             -1);
  Stylesheet::Default->base->set(FontColor,              0x000000);
  Stylesheet::Default->base->set(SectionFrameColor,      0x000000);
  
  Stylesheet::Default->base->set(FontSlant,              FontSlantPlain);
  Stylesheet::Default->base->set(FontWeight,             FontWeightPlain);
  
  Stylesheet::Default->base->set(AutoDelete,                          false);
  Stylesheet::Default->base->set(AutoNumberFormating,                 true);
  Stylesheet::Default->base->set(AutoSpacing,                         false);
  //Stylesheet::Default->base->set(ContinuousAction,                    false);
  Stylesheet::Default->base->set(Editable,                            true);
  Stylesheet::Default->base->set(Evaluatable,                         false);
  Stylesheet::Default->base->set(InternalUsesCurrentValueOfMouseOver, false);
  Stylesheet::Default->base->set(LineBreakWithin,                     true);
  Stylesheet::Default->base->set(ReturnCreatesNewSection,             false);
  Stylesheet::Default->base->set(SectionEditDuplicate,                false);
  Stylesheet::Default->base->set(SectionEditDuplicateMakesCopy,       false);
  Stylesheet::Default->base->set(SectionGenerated,                    false);
  Stylesheet::Default->base->set(ShowAutoStyles,                      false);
  Stylesheet::Default->base->set(SectionLabelAutoDelete,              true);
  Stylesheet::Default->base->set(ShowSectionBracket,                  true);
  Stylesheet::Default->base->set(ShowStringCharacters,                true);
  Stylesheet::Default->base->set(Visible,                             true);
  
  Stylesheet::Default->base->set(FontSize,                 10.0);
  Stylesheet::Default->base->set(AspectRatio,               1.0);
  Stylesheet::Default->base->set(GridBoxColumnSpacing,      0.4);
  Stylesheet::Default->base->set(GridBoxRowSpacing,         0.5);
  Stylesheet::Default->base->set(Magnification,             1.0);
  
  Stylesheet::Default->base->set(SectionMarginLeft,         7.0);
  Stylesheet::Default->base->set(SectionMarginRight,        7.0);
  Stylesheet::Default->base->set(SectionMarginTop,          4.0);
  Stylesheet::Default->base->set(SectionMarginBottom,       4.0);
  
  Stylesheet::Default->base->set(SectionFrameLeft,          0.0);
  Stylesheet::Default->base->set(SectionFrameRight,         0.0);
  Stylesheet::Default->base->set(SectionFrameTop,           0.0);
  Stylesheet::Default->base->set(SectionFrameBottom,        0.0);
  
  Stylesheet::Default->base->set(SectionFrameMarginLeft,    0.0);
  Stylesheet::Default->base->set(SectionFrameMarginRight,   0.0);
  Stylesheet::Default->base->set(SectionFrameMarginTop,     0.0);
  Stylesheet::Default->base->set(SectionFrameMarginBottom,  0.0);
  
  Stylesheet::Default->base->set(SectionGroupPrecedence,    0);
  
  Stylesheet::Default->base->set(FontFamilies,   List());
  Stylesheet::Default->base->set(SectionLabel, "");
  
  Stylesheet::Default->base->set_pmath(Method,
                                       Expr(pmath_option_value(
                                           PMATH_SYMBOL_BUTTONBOX,
                                           PMATH_SYMBOL_METHOD,
                                           PMATH_UNDEFINED)));
                                           
  Stylesheet::Default->base->set(LanguageCategory, "NaturalLanguage");
  
  Stylesheet::Default->base->set(ButtonFunction,
                                 Expr(pmath_option_value(
                                        PMATH_SYMBOL_BUTTONBOX,
                                        PMATH_SYMBOL_BUTTONFUNCTION,
                                        PMATH_UNDEFINED)));
                                        
//  Stylesheet::Default->base->set(FontFeatures,
//                                 List(Rule(String("ssty"), Symbol(PMATH_SYMBOL_AUTOMATIC))));

//  Stylesheet::Default->base->set(GeneratedSectionStyles,
//                                 Parse("{~FE`Private`style :> FE`Private`style}"));

  Stylesheet::Default->styles.set("_base", Stylesheet::Default->base);
}

static bool have_visible_documents() {
  for(unsigned int count = 0, i = 0; count < all_document_ids.size(); ++i) {
    if(all_document_ids.entry(i)) {
      ++count;
      
      Document *doc = FrontEndObject::find_cast<Document>(all_document_ids.entry(i)->key);
      
      assert(doc);
      
      if(doc->get_style(Visible, true)) {
        return true;
      }
    }
  }
  
  return false;
}

int main(int argc, char **argv) {
  os_init();
  
#ifdef RICHMATH_USE_GTK_GUI
  gtk_init(&argc, &argv);
#endif
  
  if(cairo_version() < CAIRO_VERSION_ENCODE(1, 10, 0)) {
    char str[200];
    snprintf(str, sizeof(str),
             "Cairo Version 1.10.0 or newer needed, but only %s found.",
             cairo_version_string());
             
    message_dialog("pMath Fatal Error", str);
    return 1;
  }
  
  if(pango_version() < PANGO_VERSION_ENCODE(1, 29, 0)) {
    char str[200];
    snprintf(str, sizeof(str),
             "Pango Version 1.29.0 or newer needed, but only %s found.",
             pango_version_string());
             
    message_dialog("pMath Fatal Error", str);
    return 1;
  }
  
#ifdef PMATH_DEBUG_LOG
  printf("cairo version: %s\n", cairo_version_string());
  printf("pango version: %s\n", pango_version_string());
  
#  ifdef RICHMATH_USE_GTK_GUI
  printf("gtk version: %d.%d.%d\n", gtk_major_version, gtk_minor_version, gtk_micro_version);
#  endif
#endif
  
  if(!pmath_init() || !init_bindings()) {
    message_dialog("pMath Fatal Error", "Cannot initialize the pMath library.");
    
    return 1;
  }
  
  Application::init();
  Server::init_local_server();
  
  GeneralSyntaxInfo::std = new GeneralSyntaxInfo;
  
  // load the syntax information table in parallel
  PMATH_RUN("NewTask(SyntaxInformation(Sin))");
  
  // do not depend on console window size:
  PMATH_RUN("$PageWidth:= 72");
  
  PMATH_RUN("BeginPackage(\"FE`\")");
  
  {
    double start = pmath_tickcount();
    
#define SHORTCUTS_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"shortcuts.pmath\"))"
#define MAIN_MENU_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"mainmenu.pmath\"))"
#define POPUP_MENU_CMD "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"popupmenu.pmath\"))"
    
#ifdef RICHMATH_USE_WIN32_GUI
    Win32Themes::init();
    Win32Touch::init();
    Win32Clipboard::init();
    Win32AcceleratorTable::main_table = new Win32AcceleratorTable(Evaluate(Parse(SHORTCUTS_CMD)));
    Win32Menu::main_menu              = new Win32Menu(Evaluate(Parse(MAIN_MENU_CMD)),  false);
    Win32Menu::popup_menu             = new Win32Menu(Evaluate(Parse(POPUP_MENU_CMD)), true);
#endif
    
#ifdef RICHMATH_USE_GTK_GUI
    Clipboard::std = &MathGtkClipboard::obj;
    MathGtkAccelerators::load(Evaluate(Parse(SHORTCUTS_CMD)));
    MathGtkMenuBuilder::main_menu  = MathGtkMenuBuilder(Evaluate(Parse(MAIN_MENU_CMD)));
    MathGtkMenuBuilder::popup_menu = MathGtkMenuBuilder(Evaluate(Parse(POPUP_MENU_CMD)));
#endif

    double end = pmath_tickcount();
    
    pmath_debug_print("[%f sec reading menus]\n", end - start);
  }
  
  load_fonts();
  load_math_shapers();
  
  load_aliases(
    Evaluate(Parse(
               "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"aliases.pmath\"))")),
    &global_immediate_macros,
    &global_macros);
    
  init_stylesheet();
  
  PMATH_RUN("EndPackage()"); /* FE` */
  
  Document *main_doc = nullptr;
  int result = 0;
  
  if(!MathShaper::available_shapers.default_value) {
    message_dialog("pMath Fatal Error",
                   "Cannot start pMath because there is no math font on this System.");
                   
    result = 1;
    goto QUIT;
  }
  
  main_doc = Application::create_document();
  if(main_doc) {
    write_text_section(main_doc, "Title", "Welcome");
    write_text_section(main_doc, "Section", "Todo-List");
    todo(main_doc, "CTRL-9 to insert inline text/math section into math/text sequence.");
    todo(main_doc, "Implement Interrupt().");
    todo(main_doc, "Leave caret at end of line at automatic line breaks.");
    todo(main_doc, "Navigation: ALT-left/right: previous/next span/sentence.");
    todo(main_doc, "Resize every section, not only the visible ones.");
    todo(main_doc, "Add option to allways show menu bar.");
    todo(main_doc, "CTRL-R to refactor local variable names.");
    todo(main_doc, "Add CounterBox, CounterAssignments, CounterIncrements.");
    main_doc->select(main_doc, 0, 0);
    main_doc->move_horizontal(LogicalDirection::Forward,  true);
    main_doc->move_horizontal(LogicalDirection::Backward, false);
    
    main_doc->invalidate_options();
  }
  
  if(main_doc) {
    main_doc->native()->bring_to_front();
  }
  
  if(!have_visible_documents()) {
    message_dialog("pMath Error",
                   "No document window could be opened. pMath will quit now.");
  }
  else
    Application::run();
    
QUIT:
  pmath_debug_print("quitted\n");
  
#ifdef RICHMATH_USE_GTK_GUI
  MathGtkTooltipWindow::delete_global_tooltip();
#endif
  
  MathShaper::available_shapers.clear();
  MathShaper::available_shapers.default_value = nullptr;
  
  ConfigShaper::dispose_all();
  OTMathShaper::dispose_all();
  
  TextShaper::clear_cache();
  FontInfo::remove_all_private_fonts();
  
  global_immediate_macros.clear();
  global_macros.clear();
  
  Stylesheet::Default = nullptr;
  
  GeneralSyntaxInfo::std = nullptr;
  
  Application::done();
  
#ifdef RICHMATH_USE_WIN32_GUI
  Win32Clipboard::done();
  Win32Menu::main_menu              = nullptr;
  Win32Menu::popup_menu             = nullptr;
  Win32AcceleratorTable::main_table = nullptr;
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  MathGtkMenuBuilder::main_menu  = MathGtkMenuBuilder();
  MathGtkMenuBuilder::popup_menu = MathGtkMenuBuilder();
  MathGtkMenuBuilder::done();
  MathGtkAccelerators::done();
#endif
  
  // needed to clear the message_queue member:
  Server::local_server = nullptr;
  
  done_bindings();
  
  pmath_done();
  
  return result;
}
