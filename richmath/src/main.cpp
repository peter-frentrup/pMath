#if defined( RICHMATH_USE_WIN32_FONT ) + defined( RICHMATH_USE_FT_FONT ) != 1
#error either RICHMATH_USE_WIN32_FONT or RICHMATH_USE_FT_FONT must be defined
#endif

#if defined( RICHMATH_USE_WIN32_GUI ) + defined( RICHMATH_USE_GTK_GUI ) != 1
#error RICHMATH_USE_WIN32_GUI or RICHMATH_USE_GTK_GUI must be defined
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
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/win32-document-window.h>
#include <gui/win32/win32-menu.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#include <gui/gtk/mgtk-clipboard.h>
#include <gui/gtk/mgtk-document-window.h>
#include <gui/gtk/mgtk-menu-builder.h>
#include <gui/gtk/mgtk-tooltip-window.h>
#endif

#include <gui/document.h>

#include <resources.h>

#undef STRICT

#include <pango/pangocairo.h>

#ifdef RICHMATH_USE_WIN32_GUI
#include <pango/pangowin32.h>
#include <windows.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#include <gtk/gtk.h>
#endif



#ifdef _MSC_VER
#define snprintf sprintf_s
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
      if(rule[0] == PMATH_SYMBOL_RULE
          && rule.expr_length() == 2) {
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
            
            if(def[0] == PMATH_SYMBOL_RULE
                && def.expr_length() == 2) {
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
    SetErrorMode(SEM_NOOPENFILEERRORBOX);
    
    // remove current directory from dll search path:
    kernel32 = GetModuleHandleW(L"Kernel32");
    if(kernel32) {
      BOOL (WINAPI * SetDllDirectoryW_ptr)(const WCHAR*);
      SetDllDirectoryW_ptr = (BOOL (WINAPI*)(const WCHAR*))
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
    GtkWidget *dialog = NULL;
    
    dialog = gtk_message_dialog_new(
               NULL,
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
  Expr fontlist = FontInfo::all_fonts();
  PMATH_RUN_ARGS("FE`Private`$AllStartupFonts:= Union(`1`)", "(o)", pmath_ref(fontlist.get()));
  
  Hashtable<String, Void> fonttable;
  for(size_t i = fontlist.expr_length(); i > 0; --i) {
    fonttable.set(String(fontlist[i]), Void());
  }
  
  if(!fonttable.search("Asana Math")) {
    FontInfo::add_private_font(Application::application_directory + "/Asana-Math.otf");
  }
  
  if(!fonttable.search("pMathFallback")) {
    FontInfo::add_private_font(Application::application_directory + "/pMathFallback.otf");
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
        
        shaper = OTMathShaperDB::find(s, NoStyle);
        if(shaper) {
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
      Entry<String, SharedPtr<MathShaper> > *e = MathShaper::available_shapers.entry(i);
      
      if(e) {
        MathShaper::available_shapers.default_value = e->value;
        break;
      }
    }
  }
}

static void init_stylesheet() {
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
  Stylesheet::Default->base->set(SectionGenerated,                    false);
  Stylesheet::Default->base->set(ShowAutoStyles,                      false);
  Stylesheet::Default->base->set(SectionLabelAutoDelete,              true);
  Stylesheet::Default->base->set(ShowSectionBracket,                  true);
  Stylesheet::Default->base->set(ShowStringCharacters,                true);
  
  Stylesheet::Default->base->set(FontSize,                 10.0);
  Stylesheet::Default->base->set(AspectRatio,               1.0);
  Stylesheet::Default->base->set(GridBoxColumnSpacing,      0.4);
  Stylesheet::Default->base->set(GridBoxRowSpacing,         0.5);
  
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
  
  //Stylesheet::Default->base->set(FontFamily,   "Arial");
  //Stylesheet::Default->base->set(SectionLabel, "");
  
  Stylesheet::Default->base->set_pmath_string(Method,
      Expr(pmath_option_value(
             PMATH_SYMBOL_BUTTONBOX,
             PMATH_SYMBOL_METHOD,
             PMATH_UNDEFINED)));
             
  Stylesheet::Default->base->set(ButtonFunction,
                                 Expr(pmath_option_value(
                                        PMATH_SYMBOL_BUTTONBOX,
                                        PMATH_SYMBOL_BUTTONFUNCTION,
                                        PMATH_UNDEFINED)));
                                        
  Style *s;
  
  s = new Style;
  s->set(AutoSpacing,    true);
  s->set(ShowAutoStyles, true);
  Stylesheet::Default->styles.set("Edit", s);
  
  s = new Style;
  s->set(AutoSpacing,    true);
  s->set(ShowAutoStyles, true);
  s->set(FontSlant,      FontSlantItalic);
  Stylesheet::Default->styles.set("Arg", s);
  
  s = new Style;
  s->set(FontSlant, FontSlantItalic);
  Stylesheet::Default->styles.set("TI", s);
  
  s = new Style;
  s->set(AspectRatio,         0.61803);
  s->set(AutoNumberFormating, true);
  s->set(AutoSpacing,         true);
  s->set(ShowAutoStyles,      false);
  s->set(FontSize,            8.0);
  Stylesheet::Default->styles.set("Graphics", s);
  
  s = new Style;
  s->set(AutoSpacing,         true);
  s->set(AutoNumberFormating, false);
  s->set(Evaluatable,         true);
  s->set(ShowAutoStyles,      true);
  s->set(FontSize,            11.0);
  s->set(SectionMarginLeft,   56.0);
  s->set(SectionMarginTop,    10.0);
  s->set(SectionMarginBottom,  5.0);
  Stylesheet::Default->styles.set("Input", s);
  
  s = new Style;
  s->set(BaseStyleName, "Input");
  s->set(AutoNumberFormating,     true);
  s->set(Evaluatable,             false);
  s->set(ShowAutoStyles,          false);
  s->set(ShowStringCharacters,    false);
  s->set(SectionGroupPrecedence,  10);
  s->set(SectionMarginTop,         5.0);
  Stylesheet::Default->styles.set("Output", s);
  
  s = new Style;
  s->set(BaseStyleName, "Output");
  s->set(Editable,               false);
  s->set(SectionGroupPrecedence, 20);
  s->set(SectionMarginLeft,      50.0);
  Stylesheet::Default->styles.set("Print", s);
  
  s = new Style;
  ControlPainter::std->system_font_style(s);
  s->set(ShowAutoStyles,       false);
  s->set(ShowStringCharacters, false);
  Stylesheet::Default->styles.set("ControlStyle", s);
  
  s = new Style;
  s->set(BaseStyleName, "ControlStyle");
  //s->set(ContinuousAction,     false);
  s->set(ShowStringCharacters, true);
  Stylesheet::Default->styles.set("InputField", s);
  
  s = new Style;
  s->set(BaseStyleName, "ControlStyle");
  s->set(ShowStringCharacters, false);
  s->set(SectionMarginLeft,    0.0);
  s->set(SectionMarginRight,   0.0);
  s->set(SectionMarginTop,     0.0);
  s->set(SectionMarginBottom,  0.0);
  s->set(SectionFrameMarginLeft,   3);
  s->set(SectionFrameMarginRight,  3);
  s->set(SectionFrameMarginTop,    3);
  s->set(SectionFrameMarginBottom, 3);
  s->set(SectionFrameBottom,  0.0001f);
  Stylesheet::Default->styles.set("Docked", s);
  
  s = new Style;
  s->set(BaseStyleName, "ControlStyle"); //"Print"
  s->set(AutoSpacing,          false);
  s->set(Editable,             false);
  s->set(ShowAutoStyles,       false);
  s->set(ShowStringCharacters, false);
  s->set(FontColor, 0x800000); // 0xAF501A
  s->set(FontSize, 8.0);
//    s->set(FontFamily, "Arial"); // Segoe UI
  s->set(SectionGroupPrecedence,  20);
  s->set(SectionMarginLeft, 50.0);
  Stylesheet::Default->styles.set("Message", s);
  
  s = new Style;
  s->set(BaseStyleName, "ControlStyle");
  s->set(Background, /*0xEEFFDD*/ 0xEEFFCC);
  s->set(Editable,             false);
  s->set(ShowAutoStyles,       false);
  s->set(ShowStringCharacters, false);
  s->set(SectionFrameColor, /*0x008000*/0xAACC99);
  s->set(SectionFrameLeft,   0.75);
  s->set(SectionFrameRight,  0.75);
  s->set(SectionFrameTop,    0.75);
  s->set(SectionFrameBottom, 0.75);
  s->set(SectionFrameMarginLeft,   6.0);
  s->set(SectionFrameMarginRight,  6.0);
  s->set(SectionFrameMarginTop,    6.0);
  s->set(SectionFrameMarginBottom, 6.0);
  s->set(SectionGroupPrecedence,  20);
  s->set(SectionMarginLeft, 50.0);
//    s->set(FontFamily, "Arial");
  Stylesheet::Default->styles.set("PrintUsage", s);
  
#define CAPTION_FONT "Arial" //"Calibri"
#define TEXT_FONT    "Times New Roman" //"Constantia"
  
  s = new Style;
  s->set(ShowStringCharacters, false);
  s->set(SectionMarginLeft,   50.0);
  s->set(SectionMarginTop,     7.0);
  s->set(SectionMarginBottom,  7.0);
  s->set(FontFamily,           TEXT_FONT);
  Stylesheet::Default->styles.set("Text", s);
  
  s = new Style;
  s->set(BaseStyleName, "Text");
  s->set(SectionLabel, "todo:");
  s->set(SectionLabelAutoDelete, false);
  Stylesheet::Default->styles.set("Todo", s);
  
  s = new Style;
  s->set(BaseStyleName, "Text");
  s->set(FontWeight, FontWeightBold);
  s->set(SectionGroupPrecedence, -100);
  s->set(FontSize,            22.0);
  s->set(SectionMarginLeft,   17.0);
  s->set(SectionMarginTop,    15.0);
  s->set(SectionMarginBottom, 5.0);
  s->set(FontFamily,           CAPTION_FONT);
  Stylesheet::Default->styles.set("Title", s);
  
  s = new Style;
  s->set(BaseStyleName, "Text");
  s->set(SectionGroupPrecedence, -90);
  s->set(FontSize,            18.0);
  s->set(SectionMarginLeft,   17.0);
  s->set(SectionMarginTop,     2.0);
  s->set(SectionMarginBottom, 10.0);
  s->set(FontFamily,           CAPTION_FONT);
  Stylesheet::Default->styles.set("Subtitle", s);
  
  s = new Style;
  s->set(BaseStyleName, "Text");
  s->set(SectionGroupPrecedence, -80);
  s->set(FontSize,            14.0);
  s->set(SectionMarginLeft,   17.0);
  s->set(SectionMarginTop,     2.0);
  s->set(SectionMarginBottom,  8.0);
  s->set(FontFamily,           CAPTION_FONT);
  Stylesheet::Default->styles.set("Subsubtitle", s);
  
  s = new Style;
  s->set(BaseStyleName, "Text");
  s->set(SectionGroupPrecedence, -50);
  s->set(FontWeight, FontWeightBold);
  s->set(FontSize,             16.0);
  s->set(SectionMarginLeft,    17.0);
  s->set(SectionMarginTop,     14.0);
  s->set(SectionMarginBottom,   8.0);
  s->set(SectionFrameTop,       0.75);
  s->set(SectionFrameMarginTop, 4.0);
  s->set(FontFamily,           CAPTION_FONT);
  Stylesheet::Default->styles.set("Section", s);
  
  s = new Style;
  s->set(Background,           0xFFF8CC);
  s->set(FontColor,            0x808080);//0xE9E381
  s->set(FontSize,             9.0);
  s->set(Placeholder,          true);
  s->set(Selectable,           false);
  s->set(ShowAutoStyles,       false);
  s->set(ShowStringCharacters, false);
  Stylesheet::Default->styles.set("Placeholder", s);
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
             pango_version_string());
             
    message_dialog("pMath Fatal Error", str);
    return 1;
  }
  
  if(pango_version() < PANGO_VERSION_ENCODE(1, 28, 0)) {
    char str[200];
    snprintf(str, sizeof(str),
             "Pango Version 1.28.0 or newer needed, but only %s found.",
             pango_version_string());
             
    message_dialog("pMath Fatal Error", str);
    return 1;
  }
  
#ifdef PMATH_DEBUG_LOG
  printf("cairo version: %s\n", cairo_version_string());
  printf("pango version: %s\n", pango_version_string());
  
#ifdef RICHMATH_USE_GTK_GUI
  printf("gtk version: %d.%d.%d\n", gtk_major_version, gtk_minor_version, gtk_micro_version);
#endif
#endif
  
  if(!pmath_init() || !init_bindings()) {
    message_dialog("pMath Fatal Error", "Cannot not initialize the pMath library.");
    
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
  
#define SHORTCUTS_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"shortcuts.pmath\"))"
#define MAIN_MENU_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"mainmenu.pmath\"))"
  
#ifdef RICHMATH_USE_WIN32_GUI
  Win32Themes::init();
  Win32Clipboard::init();
  Win32AcceleratorTable::main_table = new Win32AcceleratorTable(Evaluate(Parse(SHORTCUTS_CMD)));
  Win32Menu::main_menu              = new Win32Menu(Evaluate(Parse(MAIN_MENU_CMD)));
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  Clipboard::std = &MathGtkClipboard::obj;
  MathGtkAccelerators::load(Evaluate(Parse(SHORTCUTS_CMD)));
  MathGtkMenuBuilder::main_menu = MathGtkMenuBuilder(Evaluate(Parse(MAIN_MENU_CMD)));
#endif
  
  load_fonts();
  load_math_shapers();
  
  load_aliases(
    Evaluate(Parse(
               "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"aliases.pmath\"))")),
    &global_immediate_macros,
    &global_macros);
    
  init_stylesheet();
  
  PMATH_RUN("EndPackage()"); /* FE` */
  
  Document *palette_doc = 0;
  Document *main_doc = 0;
  int result = 0;
  
  if(!MathShaper::available_shapers.default_value) {
    message_dialog("pMath Fatal Error",
                   "Cannot start pMath because there is no math font on this System.");
                   
    result = 1;
    goto QUIT;
  }
  
#ifdef RICHMATH_USE_WIN32_GUI
  {
    Win32DocumentWindow *wndMain;
    Win32DocumentWindow *wndPalette;
    
    wndMain = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      580,
      600);
    wndMain->init();
    
    main_doc = wndMain->document();
    
//    wndMain->top_glass()->insert(0,
//      Section::create_from_object(Evaluate(Parse(
//        "Section(BoxData("
//          "{\"\\\" Help Topic:  \\\"\","
//            "FillBox(InputFieldBox(\"not yet implemented\")),"
//            "ButtonBox("
//              "\"\\\" Go \\\"\","
//              "ButtonFrame->\"Palette\")}),"
//          "\"Docked\","
//          "LineBreakWithin->False,"
//          "SectionMargins->{0.75, 0.75, 3, 4.5},"
//          "SectionFrameMargins->0)"
//        ))));

//    wndMain->top_glass()->insert(1,
//      Section::create_from_object(Evaluate(Parse(
//        "Section({\" Help Topic: \n not yet implemented\"},"
//          "\"Docked\")"))));

//    wndMain->top()->insert(0,
//      Section::create_from_object(Evaluate(Parse(
//        "Section(BoxData({"
//          "GridBox({{"
//            "ButtonBox(\"\\\" a \\\"\", ButtonFrame->\"Palette\"),"
//            "ButtonBox(\"\\\" b \\\"\", ButtonFrame->\"Palette\"),"
//            "ButtonBox(\"\\\" c \\\"\", ButtonFrame->\"Palette\"),"
//            "ButtonBox(\"\\\" d \\\"\", ButtonFrame->\"Palette\"),"
//            "\"\\\" \\u2190 Toolbar\\\"\""
//            "}},"
//            "GridBoxColumnSpacing->0)"
//        "}),\"Docked\","
//        "SectionFrame->{0,0,0,0.5},"
//        "SectionFrameColor->GrayLevel(0.5),"
//        "SectionFrameMargins->{1, 1, 0, 0},"
//        "SectionMargins->0)"))));
//
//    wndMain->top()->insert(wndMain->top()->length(),
//    Section::create_from_object(Evaluate(Parse(
//      "Section(BoxData({"
//          "\"\\\"Warning\\\"\","
//          "FillBox(\"\"),"
//          "ButtonBox(\"\\\" Apply \\\"\"),"
//          "ButtonBox("
//            "StyleBox("
//                "\"\\[Times]\","
//              "FontWeight->Bold,"
//              "FontColor->RGBColor(0.2),"
//              "TextShadow->{{0,0.75,GrayLevel(0.8)}}),"
//            "ButtonFrame->\"Palette\")"
//        "}),\"Docked\","
//        "Background->RGBColor(1, 0.8, 0.8),"
//        "SectionFrame->{0,0,0,0.5},"
//        "SectionFrameColor->GrayLevel(0.5),"
//        "SectionFrameMargins->{2, 2, 0, 0},"
//        "SectionMargins->0)"))));
//
//    wndMain->bottom()->insert(0,
//    Section::create_from_object(Evaluate(Parse(
//      "Section(BoxData({"
//          "\"\\\"InfoInfoInfoInfo\\\"\","
//          "FillBox(\"\"),"
//          "ButtonBox(\"\\\" Apply \\\"\"),"
//          "ButtonBox("
//            "StyleBox("
//                "\"\\[Times]\","
//              "FontWeight->Bold,"
//              "FontColor->GrayLevel(0.2),"
//              "TextShadow->{{0,0.75,GrayLevel(0.8)}}),"
//            "ButtonFrame->\"Palette\")"
//        "}),\"Docked\","
//        //"Background->RGBColor(1, 0.847, 0),"
//        "SectionFrame->{0,0,0.5,0},"
//        "SectionFrameColor->GrayLevel(0.5),"
//        "SectionFrameMargins->{2, 2, 0, 0},"
//        "SectionMargins->0)"))));

//    PMATH_RUN(
////      "FE`$StatusSlider:= 0.5;"
//      "FE`$StatusText:=\"Press ALT to show the menu.\"");

//    wndMain->bottom_glass()->insert(0,
//    Section::create_from_object(Evaluate(Parse(
//      "Section(BoxData({"
//          "DynamicBox(ToBoxes(FE`$StatusText)),"
//          "FillBox(\"\")"
////          ",\"\\[CircleMinus]\","
////          "SliderBox(Dynamic(FE`$StatusSlider, "
////            "With({val:= If(Abs(# - 0.5) < 0.05, 0.5, #)}, If(val != FE`$StatusSlider, FE`$StatusSlider:= val))&),0..1),"
////          "\"\\[CirclePlus]\""
//        "}),\"Docked\","
//        "LineBreakWithin->False,"
//        "SectionMargins->{0, 12, 1.5, 0},"
//        "SectionFrameMargins->0)"))));

    wndPalette = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      0,//CW_USEDEFAULT,
      0,//CW_USEDEFAULT,
      0,
      0);
    wndPalette->init();
    
    palette_doc = wndPalette->document();
    wndPalette->is_palette(true);
    
    RECT rect;
    RECT pal_rect;
    GetWindowRect(wndMain->hwnd(), &rect);
    GetWindowRect(wndPalette->hwnd(), &pal_rect);
    rect.left += 100;
    
    SetWindowPos(
      wndMain->hwnd(),
      NULL,
      rect.left,
      rect.top,
      0,
      0,
      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      
    SetWindowPos(
      wndPalette->hwnd(),
      NULL,
      rect.left - (pal_rect.right - pal_rect.left),//rect.right,
      rect.top,
      0,
      0,
      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      
    // override CreateProcess STARTF_USESHOWWINDOW flag:
    //ShowWindow(wndMain->hwnd(), SW_SHOWDEFAULT);
    ShowWindow(wndPalette->hwnd(), SW_SHOWNORMAL);
    ShowWindow(wndMain->hwnd(),    SW_SHOWNORMAL);
    
    if(0) {
      Win32DocumentWindow *wndInterrupt = new Win32DocumentWindow(
        new Document,
        0, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0);
      wndInterrupt->init();
      
      wndInterrupt->is_palette(true);
      Document *doc = wndInterrupt->document();
      
      doc->style->set(Editable,    false);
      doc->style->set(Selectable,  false);
      doc->style->set(WindowTitle, "Kernel Interrupt");
      
      write_section(doc, Evaluate(Parse(
                                    "Section(BoxData("
                                    "GridBox({"
                                    "{ButtonBox(\"\\\"Abort Command Being Evaluated\\\"\")},"
                                    "{ButtonBox(\"\\\"Enter Subsession\\\"\")},"
                                    "{ButtonBox(\"\\\"Continue Evaluation\\\"\")}})),"
                                    "\"ControlStyle\","
                                    "FontSize->9,"
                                    "ShowSectionBracket->False,"
                                    "ShowStringCharacters->False)")));
                                    
      doc->select(0, 0, 0);
      doc->invalidate_options();
      ShowWindow(wndInterrupt->hwnd(), SW_SHOWNORMAL);
    }
    
    Application::doevents();
    SetActiveWindow(wndMain->hwnd());
    
  }
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  {
    MathGtkDocumentWindow *wndMain = new MathGtkDocumentWindow();
    wndMain->init();
    
    main_doc = wndMain->document();
    
    wndMain->set_initial_rect(200, 50, 580, 600);
    gtk_window_present(GTK_WINDOW(wndMain->widget()));
    
    
    MathGtkDocumentWindow *wndPalette = new MathGtkDocumentWindow();
    wndPalette->init();
    
    wndPalette->set_initial_rect(704, 50, 100, 100);
    wndPalette->is_palette(true);
    
    palette_doc = wndPalette->document();
    
    gtk_window_present(GTK_WINDOW(wndPalette->widget()));
  }
#endif
  
  if(main_doc) {
    write_text_section(main_doc, "Title", "Welcome");
    write_text_section(main_doc, "Section", "Todo-List");
    todo(main_doc, "Support macros/DocumentApply in TextSequence");
    todo(main_doc, "CTRL-9 to insert inline text/math section into math/text sequence.");
    todo(main_doc, "Implement Interrupt().");
    todo(main_doc, "Leave caret at end of line at automatic line breaks.");
    todo(main_doc, "Navigation: ALT-left/right: previous/next span/sentence.");
    todo(main_doc, "Resize every section, not only the visible ones.");
    todo(main_doc, "Add option to allways show menu bar.");
    todo(main_doc, "CTRL-R to refactor local variable names.");
    todo(main_doc, "Add CounterBox, CounterAssignments, CounterIncrements.");
    todo(main_doc, "Implement Options(FrontEndObject(id), option).");
    main_doc->select(main_doc, 0, 0);
    main_doc->move_horizontal(Forward, true);
    main_doc->move_horizontal(Backward, false);
  }
  
  if(palette_doc) {
    palette_doc->style->set(Editable, false);
    palette_doc->style->set(Selectable, false);
    palette_doc->style->set(WindowTitle, "Math Input");
    palette_doc->select(0, 0, 0);
    
    write_section(
      palette_doc,
      Evaluate(
        Parse(
          "Section(BoxData("
          " GridBox({"
          "  {ButtonBox({\"\\[SelectionPlaceholder]\",SuperscriptBox(\"\\[Placeholder]\")}),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   ButtonBox(FractionBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
          "   \"\\[SpanFromLeft]\"},"
          "  {ButtonBox(SqrtBox(\"\\[SelectionPlaceholder]\")),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   ButtonBox(RadicalBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
          "   \"\\[SpanFromLeft]\"},"
          "  {ButtonBox({\"\\[Integral]\", \"\\[SelectionPlaceholder]\", \"\\[DifferentialD]\", \"\\[Placeholder]\"}),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   ButtonBox({\"\\[PartialD]\", SubscriptBox(\"\\[Placeholder]\"), \"\\[SelectionPlaceholder]\"}),"
          "   \"\\[SpanFromLeft]\"},"
          "  {ButtonBox({\"\\[Integral]\", SubsuperscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\"), \"\\[Placeholder]\", \"\\[DifferentialD]\", \"\\[Placeholder]\"}),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   ButtonBox({\"\\[PartialD]\", SubscriptBox({\"\\[Placeholder]\", \",\", \"\\[Placeholder]\"}), \"\\[SelectionPlaceholder]\"}),"
          "   \"\\[SpanFromLeft]\"},"
          "  {ButtonBox({UnderoverscriptBox(\"\\[Sum]\", {\"\\[SelectionPlaceholder]\", \"=\", \"\\[Placeholder]\"}, \"\\[Placeholder]\"), \"\\[Placeholder]\"}),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   ButtonBox({UnderoverscriptBox(\"\\[Product]\", {\"\\[SelectionPlaceholder]\", \"=\", \"\\[Placeholder]\"}, \"\\[Placeholder]\"), \"\\[Placeholder]\"}),"
          "   \"\\[SpanFromLeft]\"},"
          "  {StyleBox("
          "    ButtonBox({\"(\",GridBox({{\"\\[Placeholder]\", \"\\[Placeholder]\"}, {\"\\[Placeholder]\", \"\\[Placeholder]\"}}),\")\"}),"
          "    GridBoxColumnSpacing->0.2,"
          "    GridBoxRowSpacing->0.25),"
          "   \"\\[SpanFromLeft]\","
          "   \"\\[SpanFromLeft]\","
          "   StyleBox("
          "    ButtonBox({\"\\[Piecewise]\",GridBox({{\"\\[Placeholder]\", \"\\[Placeholder]\"}, {\"\\[Placeholder]\", \"\\[Placeholder]\"}})}),"
          "    GridBoxColumnSpacing->0.2,"
          "    GridBoxRowSpacing->0.25),\"\\[SpanFromLeft]\"},"
          //"ButtonBox({\"\\[SelectionPlaceholder]\", SubscriptBox({\"[\", \"\\[Placeholder]\", \"]\"})}),\"\\[SpanFromLeft]\"},"
          "  {TooltipBox(ButtonBox(\"\\[Pi]\"),           \"\\\"\\[AliasDelimiter]p\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[ExponentialE]\"), \"\\\"\\[AliasDelimiter]ee\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[ImaginaryI]\"),   \"\\\"\\[AliasDelimiter]ii\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Infinity]\"),     \"\\\"\\[AliasDelimiter]inf\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Degree]\"),       \"\\\"\\[AliasDelimiter]deg\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Times]\"),        \"\\\"\\[AliasDelimiter]*\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Dot]\"),          \"\\\"\\[AliasDelimiter].\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Divide]\"),       \"\\\"\\[AliasDelimiter]/\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Cross]\"),        \"\\\"\\[AliasDelimiter]cross\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[PlusMinus]\"),    \"\\\"\\[AliasDelimiter]+-\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Rule]\"),         \"\\\"\\[AliasDelimiter]->\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[RuleDelayed]\"),  \"\\\"\\[AliasDelimiter]:>\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Function]\"),     \"\\\"\\[AliasDelimiter]mt\\[AliasDelimiter]\\\"\"),"
          //"ButtonBox(\"\\[Assign]\"),"
          //"ButtonBox(\"\\[AssignDelayed]\")},"
          "   TooltipBox(ButtonBox(\"\\u21D2\"),          \"\\\"\\[AliasDelimiter]=>\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\u21D4\"),          \"\\\"\\[AliasDelimiter]<=>\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[NotEqual]\"),     \"\\\"\\[AliasDelimiter]!=\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[LessEqual]\"),    \"\\\"\\[AliasDelimiter]<=\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[GreaterEqual]\"), \"\\\"\\[AliasDelimiter]>=\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Element]\"),      \"\\\"\\[AliasDelimiter]elem\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[NotElement]\"),   \"\\\"\\[AliasDelimiter]!elem\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Not]\"),          \"\\\"\\[AliasDelimiter]!\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[And]\"),          \"\\\"\\[AliasDelimiter]and\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Or]\"),           \"\\\"\\[AliasDelimiter]or\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Intersection]\"), \"\\\"\\[AliasDelimiter]inter\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Union]\"),        \"\\\"\\[AliasDelimiter]un\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Alpha]\"),        \"\\\"\\[AliasDelimiter]a\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Beta]\"),         \"\\\"\\[AliasDelimiter]b\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Gamma]\"),        \"\\\"\\[AliasDelimiter]g\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Delta]\"),        \"\\\"\\[AliasDelimiter]d\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CurlyEpsilon]\"), \"\\\"\\[AliasDelimiter]ce\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Zeta]\"),         \"\\\"\\[AliasDelimiter]z\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Eta]\"),          \"\\\"\\[AliasDelimiter]h\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Theta]\"),        \"\\\"\\[AliasDelimiter]q\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CurlyTheta]\"),   \"\\\"\\[AliasDelimiter]cq\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Iota]\"),         \"\\\"\\[AliasDelimiter]i\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Kappa]\"),        \"\\\"\\[AliasDelimiter]k\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Lambda]\"),       \"\\\"\\[AliasDelimiter]l\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Mu]\"),           \"\\\"\\[AliasDelimiter]m\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Nu]\"),           \"\\\"\\[AliasDelimiter]n\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Xi]\"),           \"\\\"\\[AliasDelimiter]x\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[Pi]\"),           \"\\\"\\[AliasDelimiter]p\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Rho]\"),          \"\\\"\\[AliasDelimiter]r\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Sigma]\"),        \"\\\"\\[AliasDelimiter]s\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[FinalSigma]\"),   \"\\\"\\[AliasDelimiter]varsigma\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Upsilon]\"),      \"\\\"\\[AliasDelimiter]u\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[CurlyPhi]\"),     \"\\\"\\[AliasDelimiter]j\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Phi]\"),          \"\\\"\\[AliasDelimiter]f\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Chi]\"),          \"\\\"\\[AliasDelimiter]c\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Psi]\"),          \"\\\"\\[AliasDelimiter]y\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[Omega]\"),        \"\\\"\\[AliasDelimiter]w\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[CapitalGamma]\"), \"\\\"\\[AliasDelimiter]G\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalDelta]\"), \"\\\"\\[AliasDelimiter]D\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalTheta]\"), \"\\\"\\[AliasDelimiter]Q\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalLambda]\"),\"\\\"\\[AliasDelimiter]L\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalXi]\"),    \"\\\"\\[AliasDelimiter]X\\[AliasDelimiter]\\\"\")},"
          "  {TooltipBox(ButtonBox(\"\\[CapitalPi]\"),    \"\\\"\\[AliasDelimiter]P\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalSigma]\"), \"\\\"\\[AliasDelimiter]S\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalPhi]\"),   \"\\\"\\[AliasDelimiter]F\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalPsi]\"),   \"\\\"\\[AliasDelimiter]Y\\[AliasDelimiter]\\\"\"),"
          "   TooltipBox(ButtonBox(\"\\[CapitalOmega]\"), \"\\\"\\[AliasDelimiter]W\\[AliasDelimiter]\\\"\")},"
          "  {ButtonBox({\"\\[SelectionPlaceholder]\",SubscriptBox(\"\\[Placeholder]\")}),"
          "   ButtonBox({\"\\[SelectionPlaceholder]\",SubsuperscriptBox(\"\\[Placeholder]\",\"\\[Placeholder]\")}),"
          "   ButtonBox(UnderscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
          "   ButtonBox(OverscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
          "   ButtonBox(UnderoverscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\", \"\\[Placeholder]\"))}})),"
          "  \"Output\","
          "  FontSize->9,"
          "  ButtonFrame->\"Palette\","
          "  ButtonFunction->(DocumentApply(SelectedDocument(), #)&),"
          "  GridBoxColumnSpacing->0,"
          "  GridBoxRowSpacing->0,"
          "  SectionMargins->0,"
          "  ShowSectionBracket->False)"
          //".Replace(TooltipBox(~FE`Private`x,~) :> FE`Private`x)"
        )));
        
    palette_doc->invalidate_options();
    palette_doc->invalidate();
  }
  
  if(all_document_ids.size() == 0) {
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
  MathShaper::available_shapers.default_value = 0;
  
  ConfigShaperDB::clear_all();
  OTMathShaperDB::clear_all();
  
  TextShaper::clear_cache();
  
  global_immediate_macros.clear();
  global_macros.clear();
  
  Stylesheet::Default = 0;
  
  GeneralSyntaxInfo::std = 0;
  
  Application::done();
  
#ifdef RICHMATH_USE_WIN32_GUI
  Win32Clipboard::done();
  Win32Menu::main_menu              = 0;
  Win32AcceleratorTable::main_table = 0;
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  MathGtkMenuBuilder::main_menu = MathGtkMenuBuilder();
  MathGtkAccelerators::done();
#endif
  
  // needed to clear the message_queue member:
  Server::local_server = 0;
  
  done_bindings();
  
  pmath_done();
  
  return result;
}
