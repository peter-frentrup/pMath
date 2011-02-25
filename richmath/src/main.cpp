#define __STDC_FORMAT_MACROS
#define WINVER 0x0500 // RegisterFontResoureceEx

#include <cmath>
#include <cstdio>
#include <ctime>

#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/binding.h>
#include <eval/client.h>
#include <eval/server.h>
#include <graphics/win32-shaper.h>
#include <graphics/config-shaper.h>
#include <graphics/ot-math-shaper.h>
#include <gui/control-painter.h>
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/win32-document-window.h>
#include <gui/win32/win32-themes.h>

#include <resources.h>

#undef STRICT

#include <pango/pangocairo.h>
#include <pango/pangowin32.h>

#include <Windows.h>

using namespace richmath;

static void write_section(Document *doc, Expr expr){
  Box *b = doc->selection_box();
  int i  = doc->selection_end();
  while(b && b != doc){
    i = b->index();
    b = b->parent();
  }
  
  if(!b){
    b = doc;
    i = doc->length();
  }
  
  doc->insert(i, Section::create_from_object(expr));
  
  doc->move_to(b, i + 1);
}

static void write_text_section(Document *doc, String style, String text){
  write_section(doc, 
    Call(
      Symbol(PMATH_SYMBOL_SECTION),
      text,
      style));
}

static void todo(Document *doc, String msg){
  write_text_section(doc, "Todo", msg);
}

class PrivateFont: public Shareable{
  public:
    static bool load(String name){
      name+= String::FromChar(0);
      
      String file = Client::application_directory + "/" + name;
      
      guard = new PrivateFont(guard);
      guard->filename.length(file.length());
      memcpy(
        guard->filename.items(), 
        file.buffer(), 
        guard->filename.length() * sizeof(uint16_t));
      
      if(AddFontResourceExW(guard->filename.items(), FR_PRIVATE, 0) > 0){
        return true;
      }
      
      guard = guard->next;
      return false;
    }
    
  private:
    PrivateFont(SharedPtr<PrivateFont> _next)
    : Shareable(),
      filename(0),
      next(_next)
    {
    }
    
    ~PrivateFont(){
      if(filename.length() > 0){
        RemoveFontResourceExW(filename.items(), FR_PRIVATE, 0);
      }
    }
    
  private:
    static SharedPtr<PrivateFont> guard;
    
    Array<WCHAR> filename;
    SharedPtr<PrivateFont> next;
};

SharedPtr<PrivateFont> PrivateFont::guard = 0;

static void load_aliases(
  Expr                                  aliases,
  Hashtable<String, Expr, object_hash> *implicit_macros,
  Hashtable<String, Expr, object_hash> *explicit_macros
){
  Hashtable<String, Expr, object_hash> *table = explicit_macros;
  
  clock_t start = clock();
  
  if(aliases[0] == PMATH_SYMBOL_LIST){
    for(size_t i = 1;i <= aliases.expr_length();++i){
      Expr rule = aliases[i];
      if(rule[0] == PMATH_SYMBOL_RULE
      && rule.expr_length() == 2){
        String lhs = rule[1];
        Expr   rhs = rule[2];
        
        if(lhs.equals("Explicit"))
          table = explicit_macros;
        else if(lhs.equals("Implicit"))
          table = implicit_macros;
        else
          continue;
          
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = 1;j <= rhs.expr_length();++j){
            Expr def = rhs[j];
            
            if(def[0] == PMATH_SYMBOL_RULE
            && def.expr_length() == 2){
              String name = def[1];
              
              if(name.length() > 0)
                table->set(name, def[2]);
            }
          }
        }
      }
    }
  }

  printf("Loaded aliases in %f seconds.\n", (clock() - start) / (double)CLOCKS_PER_SEC);
}

static void os_init(){
  HMODULE kernel32;
  
  // do not show message boxes on LoadLibrary errors:
  SetErrorMode(SEM_NOOPENFILEERRORBOX);
  
  // remove current directory from dll search path:
  kernel32 = GetModuleHandleW(L"Kernel32");
  if(kernel32){
    BOOL (WINAPI *SetDllDirectoryW_ptr)(const WCHAR*);
    SetDllDirectoryW_ptr = (BOOL (WINAPI*)(const WCHAR*))
      GetProcAddress(kernel32, "SetDllDirectoryW");
    
    if(SetDllDirectoryW_ptr)
      SetDllDirectoryW_ptr(L"");
  }
}

int main(){
  os_init();
  
  if(cairo_version() < CAIRO_VERSION_ENCODE(1,10,0)){
    fprintf(stderr, 
      "Cairo Version 1.10.0 or newer needed, but only %s found.\n",
      cairo_version_string());
    return 1;
  }
  
  if(pango_version() < PANGO_VERSION_ENCODE(1,28,0)){
    fprintf(stderr, 
      "Pango Version 1.28.0 or newer needed, but only %s found.\n",
      pango_version_string());
    return 1;
  }
  
  #ifdef PMATH_DEBUG_LOG
    printf("cairo version: %s\n", cairo_version_string());
    printf("pango version: %s\n", pango_version_string());
  #endif
  
  if(!pmath_init()
  || !init_bindings()){
    fprintf(stderr, "cannot initialize pmath\n");
    return 1;
  }
  
  Win32Themes::init();
  Client::init();
  Server::init_local_server();
  
  GeneralSyntaxInfo::std = new GeneralSyntaxInfo;
  Win32Clipboard::init();
  
  // load the syntax information table in parallel
  PMATH_RUN("NewTask(SyntaxInformation(Sin))");
      
  PMATH_RUN("BeginPackage(\"FE`\")");
  {
    Expr fontlist = FontInfo::all_fonts();
    
    Hashtable<String, Void> fonttable;
    for(size_t i = fontlist.expr_length();i > 0;--i){
      fonttable.set(String(fontlist[i]), Void());
    }
    
    if(!fonttable.search("Asana Math")){
      PrivateFont::load("Asana-Math.otf");
    }
    
    if(!fonttable.search("pMathFallback")){
      PrivateFont::load("pMathFallback.otf");
    }
    
    PMATH_RUN(
      "ParallelScan("
        "FileNames("
          "\"*.pmath\","
          "ToFileName({FE`$FrontEndDirectory,\"resources\"},\"shapers\")),"
        "FE`AddConfigShaper)"
      );
  }
  
  {
    Expr prefered_fonts = Evaluate(Parse("FE`$MathShapers"));
    
    SharedPtr<MathShaper> shaper;
    SharedPtr<MathShaper> def;
    
    if(prefered_fonts[0] == PMATH_SYMBOL_LIST){
      for(size_t i = 1;i <= prefered_fonts.expr_length();++i){
        String s(prefered_fonts[i]);
        
        shaper = MathShaper::available_shapers[s];
        
        if(!shaper){
          clock_t start_font = clock();
          
          shaper = OTMathShaperDB::find(s, NoStyle);
          if(shaper){
            MathShaper::available_shapers.set(s, shaper);
            
            pmath_debug_print_object("loaded ", s.get(), "");
            pmath_debug_print(" in %f seconds\n", (clock() - start_font) / (float)CLOCKS_PER_SEC);
          }
        }
        
        if(shaper && !def)
          def = shaper;
      }
    }
    
    if(def){
      MathShaper::available_shapers.default_value = def;
    }
    else if(MathShaper::available_shapers.size() > 0){
      for(int i = 0;;++i){
        Entry<String, SharedPtr<MathShaper> > *e = MathShaper::available_shapers.entry(i);
        
        if(e){
          MathShaper::available_shapers.default_value = e->value;
          break;
        }
      }
    }
  }
  
  if(!MathShaper::available_shapers.default_value){
    MessageBoxW(
      0, 
      L"Cannot start pMath because there is no math font on this System.",
      L"Fatal Error",
      MB_OK | MB_ICONERROR);
    return 1;
  }
  
  load_aliases(
    Evaluate(Parse(
      "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"aliases.pmath\"))")),
    &global_immediate_macros,
    &global_macros);
  
  PMATH_RUN("EndPackage()"); /* FE` */
  
  {
    Stylesheet::Default = new Stylesheet;
    
    Stylesheet::Default->base = new Style;
    Stylesheet::Default->base->set(Background,             -1);
    Stylesheet::Default->base->set(FontColor,              0x000000);
    Stylesheet::Default->base->set(SectionFrameColor,      0x000000);
    
    Stylesheet::Default->base->set(FontSlant,              FontSlantPlain);
    Stylesheet::Default->base->set(FontWeight,             FontWeightPlain);
    
    Stylesheet::Default->base->set(AutoDelete,             false);
    Stylesheet::Default->base->set(AutoNumberFormating,    true);
    Stylesheet::Default->base->set(AutoSpacing,            false);
    Stylesheet::Default->base->set(Editable,               true);
    Stylesheet::Default->base->set(Evaluatable,            false);
    Stylesheet::Default->base->set(LineBreakWithin,        true);
    Stylesheet::Default->base->set(SectionGenerated,       false);
    Stylesheet::Default->base->set(ShowAutoStyles,         false);
    Stylesheet::Default->base->set(SectionLabelAutoDelete, true);
    Stylesheet::Default->base->set(ShowSectionBracket,     true);
    Stylesheet::Default->base->set(ShowStringCharacters,   true);
    
    Stylesheet::Default->base->set(FontSize,                 10.0);
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
    s->set(AutoSpacing, true);
    s->set(ShowAutoStyles, true);
    Stylesheet::Default->styles.set("Edit", s);
    
    s = new Style;
    s->set(AutoSpacing, true);
    s->set(ShowAutoStyles, true);
    s->set(FontSlant, FontSlantItalic);
    Stylesheet::Default->styles.set("Arg", s);
    
    s = new Style;
    s->set(AutoSpacing,         true);
    s->set(AutoNumberFormating, false);
    s->set(Evaluatable,         true);
    s->set(ShowAutoStyles,      true);
    s->set(FontSize,                11.0);
    s->set(SectionMarginLeft,       56.0);
    s->set(SectionMarginTop,        10.0);
    s->set(SectionMarginBottom,      5.0);
    Stylesheet::Default->styles.set("Input", s);
    
    s = new Style;
    s->set(BaseStyleName, "Input");
    s->set(AutoNumberFormating,  true);
    s->set(Evaluatable,          false);
    s->set(ShowAutoStyles,       false);
    s->set(ShowStringCharacters, false);
    s->set(SectionGroupPrecedence,  10);
    s->set(SectionMarginTop,         5.0);
    Stylesheet::Default->styles.set("Output", s);
    
    s = new Style;
    s->set(BaseStyleName, "Output");
    s->set(Editable, false);
    s->set(SectionMarginLeft, 50.0);
    Stylesheet::Default->styles.set("Print", s);
    
    s = new Style;
    ControlPainter::std->system_font_style(s);
//    s->set(AutoSpacing, false);
//    s->set(ShowAutoStyles, false);
    Stylesheet::Default->styles.set("ControlStyle", s);
    
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
    s->set(SectionGroupPrecedence,  10);
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
    s->set(SectionGroupPrecedence,  10);
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
  
  {
    Document *doc;
    Win32DocumentWindow *wndMain;
    Win32DocumentWindow *wndPalette;
    
    wndMain = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      500,
      550);
    wndMain->init();
    
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
    
    PMATH_RUN(
//      "FE`$StatusSlider:= 0.5;"
      "FE`$StatusText:=\"Press ALT to show the menu.\"");
    
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
        
    doc = wndMain->document();
    
    write_text_section(doc, "Title", "Welcome");
    write_text_section(doc, "Section", "Todo-List");
    todo(doc, "Support macros/DocumentApply in TextSequence");
    todo(doc, "CTRL-9 to insert inline text/math section into math/text sequence.");
    todo(doc, "Implement Interrupt().");
    todo(doc, "Leave caret at end of line at automatic line breaks.");
    todo(doc, "Navigation: ALT-left/right: previous/next span/sentence.");
    todo(doc, "Resize every section, not only the visible ones.");
    todo(doc, "Add option to allways show menu bar.");
    todo(doc, "Build menu and keybord accelerators at runtime from a script.");
    todo(doc, "CTRL-R to refactor local variable names.");
    todo(doc, "Add CounterBox, CounterAssignments, CounterIncrements.");
    todo(doc, "Implement Options(FrontEndObject(id), option).");
    todo(doc, "Use/test the Menu class.");
    doc->select(doc,0,0);
    doc->move_horizontal(Forward, true);
    doc->move_horizontal(Backward, false);
    
    wndPalette = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      0,//CW_USEDEFAULT,
      0,//CW_USEDEFAULT,
      0,
      0);
    wndPalette->init();
      
    wndPalette->is_palette(true);
    doc = wndPalette->document();
    
    doc->style->set(Editable, false);
    doc->style->set(Selectable, false);
    
    wndPalette->title("Math Input");
    write_section(doc, Evaluate(Parse(
      "Section(BoxData("
        "GridBox({"
          "{ButtonBox({\"\\[SelectionPlaceholder]\",SuperscriptBox(\"\\[Placeholder]\")}),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "ButtonBox(FractionBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),\"\\[SpanFromLeft]\"},"
          "{ButtonBox(SqrtBox(\"\\[SelectionPlaceholder]\")),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "ButtonBox(RadicalBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),\"\\[SpanFromLeft]\"},"
          "{ButtonBox({\"\\[Integral]\", \"\\[SelectionPlaceholder]\", \"\\[DifferentialD]\", \"\\[Placeholder]\"}),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "ButtonBox({\"\\[PartialD]\", SubscriptBox(\"\\[Placeholder]\"), \"\\[SelectionPlaceholder]\"}),\"\\[SpanFromLeft]\"},"
          "{ButtonBox({\"\\[Integral]\", SubsuperscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\"), \"\\[Placeholder]\", \"\\[DifferentialD]\", \"\\[Placeholder]\"}),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "ButtonBox({\"\\[PartialD]\", SubscriptBox({\"\\[Placeholder]\", \",\", \"\\[Placeholder]\"}), \"\\[SelectionPlaceholder]\"}),\"\\[SpanFromLeft]\"},"
          "{ButtonBox({UnderoverscriptBox(\"\\[Sum]\", {\"\\[SelectionPlaceholder]\", \"=\", \"\\[Placeholder]\"}, \"\\[Placeholder]\"), \"\\[Placeholder]\"}),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "ButtonBox({UnderoverscriptBox(\"\\[Product]\", {\"\\[SelectionPlaceholder]\", \"=\", \"\\[Placeholder]\"}, \"\\[Placeholder]\"), \"\\[Placeholder]\"}),\"\\[SpanFromLeft]\"},"
          "{StyleBox("
              "ButtonBox({\"(\",GridBox({{\"\\[Placeholder]\", \"\\[Placeholder]\"}, {\"\\[Placeholder]\", \"\\[Placeholder]\"}}),\")\"}),"
              "GridBoxColumnSpacing->0.2,"
              "GridBoxRowSpacing->0.25),\"\\[SpanFromLeft]\",\"\\[SpanFromLeft]\","
            "StyleBox("
              "ButtonBox({\"\\[Piecewise]\",GridBox({{\"\\[Placeholder]\", \"\\[Placeholder]\"}, {\"\\[Placeholder]\", \"\\[Placeholder]\"}})}),"
              "GridBoxColumnSpacing->0.2,"
              "GridBoxRowSpacing->0.25),\"\\[SpanFromLeft]\"},"
            //"ButtonBox({\"\\[SelectionPlaceholder]\", SubscriptBox({\"[\", \"\\[Placeholder]\", \"]\"})}),\"\\[SpanFromLeft]\"},"
          "{ButtonBox(\"\\[Pi]\"),"
            "ButtonBox(\"\\[ExponentialE]\"),"
            "ButtonBox(\"\\[ImaginaryI]\"),"
            "ButtonBox(\"\\[Infinity]\"),"
            "ButtonBox(\"\\[Degree]\")},"
          "{ButtonBox(\"\\[Times]\"),"
            "ButtonBox(\"\\[Dot]\"),"
            "ButtonBox(\"\\[Divide]\"),"
            "ButtonBox(\"\\[Cross]\"),"
            "ButtonBox(\"\\[PlusMinus]\")},"
          "{ButtonBox(\"\\[Rule]\"),"
            "ButtonBox(\"\\[RuleDelayed]\"),"
            "ButtonBox(\"\\[Function]\"),"
//            "ButtonBox(\"\\[Assign]\"),"
//            "ButtonBox(\"\\[AssignDelayed]\")},"
            "ButtonBox(\"\\u21D2\"),"
            "ButtonBox(\"\\u21D4\")},"
          "{ButtonBox(\"\\[NotEqual]\"),"
            "ButtonBox(\"\\[LessEqual]\"),"
            "ButtonBox(\"\\[GreaterEqual]\"),"
            "ButtonBox(\"\\[Element]\"),"
            "ButtonBox(\"\\[NotElement]\")},"
          "{ButtonBox(\"\\[Not]\"),"
            "ButtonBox(\"\\[And]\"),"
            "ButtonBox(\"\\[Or]\"),"
            "ButtonBox(\"\\[Intersection]\"),"
            "ButtonBox(\"\\[Union]\")},"
          "{ButtonBox(\"\\[Alpha]\"),"
            "ButtonBox(\"\\[Beta]\"),"
            "ButtonBox(\"\\[Gamma]\"),"
            "ButtonBox(\"\\[Delta]\"),"
            "ButtonBox(\"\\[CurlyEpsilon]\")},"
          "{ButtonBox(\"\\[Zeta]\"),"
            "ButtonBox(\"\\[Eta]\"),"
            "ButtonBox(\"\\[Theta]\"),"
            "ButtonBox(\"\\[Iota]\"),"
            "ButtonBox(\"\\[Kappa]\")},"
          "{ButtonBox(\"\\[Lambda]\"),"
            "ButtonBox(\"\\[Mu]\"),"
            "ButtonBox(\"\\[Nu]\"),"
            "ButtonBox(\"\\[Xi]\"),"
            "ButtonBox(\"\\[Pi]\")},"
          "{ButtonBox(\"\\[Rho]\"),"
            "ButtonBox(\"\\[Sigma]\"),"
            "ButtonBox(\"\\[FinalSigma]\"),"
            "ButtonBox(\"\\[Upsilon]\"),"
            "ButtonBox(\"\\[Phi]\")},"
          "{ButtonBox(\"\\[Chi]\"),"
            "ButtonBox(\"\\[Psi]\"),"
            "ButtonBox(\"\\[Omega]\"),"
            "\"\","
            "\"\"},"
          "{ButtonBox(\"\\[CapitalGamma]\"),"
            "ButtonBox(\"\\[CapitalDelta]\"),"
            "ButtonBox(\"\\[CapitalTheta]\"),"
            "ButtonBox(\"\\[CapitalLambda]\"),"
            "ButtonBox(\"\\[CapitalXi]\")},"
          "{""ButtonBox(\"\\[CapitalPi]\"),"
            "ButtonBox(\"\\[CapitalSigma]\"),"
            "ButtonBox(\"\\[CapitalPhi]\"),"
            "ButtonBox(\"\\[CapitalPsi]\"),"
            "ButtonBox(\"\\[CapitalOmega]\")},"
          "{""ButtonBox({\"\\[SelectionPlaceholder]\",SubscriptBox(\"\\[Placeholder]\")}),"
            "ButtonBox({\"\\[SelectionPlaceholder]\",SubsuperscriptBox(\"\\[Placeholder]\",\"\\[Placeholder]\")}),"
            "ButtonBox(UnderscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
            "ButtonBox(OverscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\")),"
            "ButtonBox(UnderoverscriptBox(\"\\[SelectionPlaceholder]\", \"\\[Placeholder]\", \"\\[Placeholder]\"))}})),"
        "\"Output\"," 
        "FontSize->9,"
        "ButtonFrame->\"Palette\","
        "ButtonFunction->(DocumentApply(SelectedDocument(), #)&),"
        "GridBoxColumnSpacing->0,"
        "GridBoxRowSpacing->0,"
        "SectionMargins->0,"
        "ShowSectionBracket->False)")));
    
    doc->select(0,0,0);
    
    RECT rect;
    RECT pal_rect;
    GetWindowRect(wndMain->hwnd(), &rect);
    GetWindowRect(wndPalette->hwnd(), &pal_rect);
    rect.left+= 100;
    
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
    ShowWindow(wndMain->hwnd(), SW_SHOWNORMAL);
    
    for(int i = 0;i < 0;++i){
      wndMain = new Win32DocumentWindow(
        new Document,
        0, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        500,
        550);
      wndMain->init();
      ShowWindow(wndMain->hwnd(), SW_SHOWNORMAL);
    }
    
    if(0){
      Win32DocumentWindow *wndInterrupt = new Win32DocumentWindow(
        new Document,
        0, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0);
      wndInterrupt->init();
        
      wndInterrupt->is_palette(true);
      doc = wndInterrupt->document();
      
      doc->style->set(Editable, false);
      doc->style->set(Selectable, false);
      
      wndInterrupt->title("Kernel Interrupt");
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
      
      doc->select(0,0,0);
      ShowWindow(wndInterrupt->hwnd(), SW_SHOWNORMAL);
    }
      
    Client::doevents();
    SetActiveWindow(wndMain->hwnd());
    
  }
  
  int result = Client::run();
  
  printf("quitted\n");
  
  MathShaper::available_shapers.clear();
  MathShaper::available_shapers.default_value = 0;
  
  ConfigShaperDB::clear_all();
  OTMathShaperDB::clear_all();
  
  TextShaper::clear_cache();
  
  global_immediate_macros.clear();
  global_macros.clear();
  
  Stylesheet::Default = 0;
  
  GeneralSyntaxInfo::std = 0;
  
  Client::done();
  Win32Clipboard::done();
  
  // needed to clear the message_queue member:
  Server::local_server = 0;
  
  done_bindings();
  
  pmath_done();
  
  size_t current, max;
  pmath_mem_usage(&current, &max);
  printf("memory: %"PRIuPTR" (should be 0)\n", current);
  printf("max. used: %"PRIuPTR"\n", max);
  
  if(current != 0){
    printf("\a");
    system("pause");
  }
  
  return result;
}
