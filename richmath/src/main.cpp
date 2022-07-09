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

#include <util/rle-array.h>
#include <boxes/graphics/axisticks.h>
#include <boxes/graphics/graphicsbox.h>
#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/linebox.h>
#include <boxes/graphics/pointbox.h>
#include <boxes/box-factory.h>
#include <boxes/buttonbox.h>
#include <boxes/checkboxbox.h>
#include <boxes/dynamicbox.h>
#include <boxes/dynamiclocalbox.h>
#include <boxes/fillbox.h>
#include <boxes/fractionbox.h>
#include <boxes/framebox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/interpretationbox.h>
#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/openerbox.h>
#include <boxes/panebox.h>
#include <boxes/panelbox.h>
#include <boxes/paneselectorbox.h>
#include <boxes/progressindicatorbox.h>
#include <boxes/radicalbox.h>
#include <boxes/radiobuttonbox.h>
#include <boxes/section.h>
#include <boxes/setterbox.h>
#include <boxes/sliderbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/templatebox.h>
#include <boxes/textsequence.h>
#include <boxes/tooltipbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>
#include <eval/binding.h>
#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <eval/server.h>
#include <graphics/config-shaper.h>
#include <graphics/ot-math-shaper.h>
#include <gui/control-painter.h>
#include <gui/documents.h>
#include <gui/recent-documents.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/win32-clipboard.h>
#  include <gui/win32/win32-control-painter.h>
#  include <gui/win32/win32-document-window.h>
#  include <gui/win32/api/win32-highdpi.h>
#  include <gui/win32/api/win32-touch.h>
#  include <gui/win32/api/win32-version.h>
#  include <gui/win32/menus/win32-menu.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-clipboard.h>
#  include <gui/gtk/mgtk-control-painter.h>
#  include <gui/gtk/mgtk-css.h>
#  include <gui/gtk/mgtk-document-window.h>
#  include <gui/gtk/mgtk-menu-builder.h>
#  include <gui/gtk/mgtk-tooltip-window.h>
#endif

#ifdef RICHMATH_USE_FT_FONT
#  include <cairo-ft.h>
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


namespace richmath { namespace strings {
  extern String Global_namespace;
}}

using namespace richmath;

extern pmath_symbol_t richmath_System_DollarFrontEndSession;
extern pmath_symbol_t richmath_System_DollarPageWidth;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonFunction;
extern pmath_symbol_t richmath_System_Identity;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Method;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Section;

extern pmath_symbol_t richmath_FE_DollarMathShapers;
extern pmath_symbol_t richmath_FE_DollarPrivateStartupFontFiles;

#ifdef RICHMATH_DEBUG_MEMORY
#  ifdef PMATH_64BIT
     static_assert(sizeof(Base) == 32, "");
#  endif
#else
  static_assert(sizeof(Base) == 1, ""); // unused
#endif

#ifdef NDEBUG
#  if defined(PMATH_64BIT)
    static_assert(sizeof(Array<void*>)              ==  8,  "");
    static_assert(sizeof(Array<bool>)               ==  8,  "");
    static_assert(sizeof(RleArray<void*>)           ==  8,  "");
    static_assert(sizeof(Matrix<void*>)             ==  16, ""); // 8 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(Dynamic)                   ==  24, ""); // 8 + 8 + 4 (enum where 1 byte would suffice) + 4 PADDING BYTES AT END
    static_assert(sizeof(PartialDynamic)            ==  24, ""); // 8 + 8 + 8
    static_assert(sizeof(FrontEndReference)         ==   4, "");
    static_assert(sizeof(FrontEndObject)            ==  16, ""); // 8 + 4 + 4
    static_assert(sizeof(StyledObject)              ==  16, "");
    static_assert(sizeof(ActiveStyledObject)        ==  24, ""); // (16) + 8
    static_assert(sizeof(BoxSize)                   ==  12, ""); // 3*4
    static_assert(sizeof(Box)                       ==  48, ""); // (24) + 8 + 4 + 12
    static_assert(sizeof(AbstractSequence)          ==  72, ""); // (48) + 8 + 8 + 4 + 4 UNUSED PADDING BYTES AT END
    static_assert(sizeof(MathSequence)              == 120, ""); // (72) + 2*8 + 3*8 + 8
    static_assert(sizeof(TextSequence)              == 112, ""); // (72) + 8 + 8 + 8 + 8 + 4 + 4 UNUSED PADDING BYTES AT END
    static_assert(sizeof(EmptyWidgetBox)            ==  72, ""); // (48 + 8) + 8 + 3*1 + 5 UNUSED PADDING BYTES
    static_assert(sizeof(CheckboxBox)               == 112, ""); // (72) + 24 + 8 + 8
    static_assert(sizeof(OpenerBox)                 == 104, ""); // (72) + 24 + 8
    static_assert(sizeof(RadioButtonBox)            == 104, ""); // (72) + 24 + 8
    static_assert(sizeof(SliderBox)                 == 160, ""); // (72) + 6*8 + 8 + 24 + 4 + 4
    static_assert(sizeof(OwnerBox)                  ==  64, ""); // (48) + 8 + 4 + 4
    static_assert(sizeof(ExpandableOwnerBox)        ==  64, "");
    static_assert(sizeof(InlineSequenceBox)         ==  64, "");
    static_assert(sizeof(AbstractStyleBox)          ==  64, "");
    static_assert(sizeof(StyleBox)                  ==  64, "");
    static_assert(sizeof(InterpretationBox)         ==  72, ""); // (64) + 8
    static_assert(sizeof(TagBox)                    ==  72, ""); // (64) + 8
    static_assert(sizeof(TooltipBox)                ==  72, ""); // (64) + 8
    static_assert(sizeof(NumberBox)                 == 104, ""); // (64) + 8 + 4*8
    static_assert(sizeof(ContainerWidgetBox)        ==  88, ""); // (64 + 8) + 8 + 2*1 + 6 UNUSED PADDING BYTES
    static_assert(sizeof(AbstractButtonBox)         ==  88, "");
    static_assert(sizeof(ButtonBox)                 ==  88, "");
    static_assert(sizeof(SetterBox)                 == 120, ""); // (88) + 24 + 8
    static_assert(sizeof(InputFieldBox)             == 136, ""); // (88) + 4 + 4 UNUSED PADDING BYTES + 8 + 24 + 8
    static_assert(sizeof(AbstractDynamicBox)        ==  64, "");
    static_assert(sizeof(DynamicBox)                ==  88, ""); // (64) + 24
    static_assert(sizeof(DynamicLocalBox)           ==  80, ""); // (64) + 2*8
    static_assert(sizeof(FillBox)                   ==  72, ""); // (64) + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(FractionBox)               ==  72, ""); // (48) + 8 + 8 + 4 + 4
    static_assert(sizeof(FrameBox)                  ==  72, ""); // (64) + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(GridItem)                  ==  72, ""); // (64) + 4 + 4
    static_assert(sizeof(GridBox)                   ==  88, ""); // (48) + 4 + 4 + 16 + 8 + 8
    static_assert(sizeof(AbstractTransformationBox) == 112, ""); // (64) + 6*8
    static_assert(sizeof(RotationBox)               == 120, ""); // (112) + 8
    static_assert(sizeof(TransformationBox)         == 120, ""); // (112) + 8
    static_assert(sizeof(PaneBox)                   == 112, "");
    static_assert(sizeof(PanelBox)                  ==  88, "");
    static_assert(sizeof(PaneSelectorBox)           ==  96, ""); // (48) + 8 + 8 + 24 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(ProgressIndicatorBox)      == 120, ""); // (48 + 8) + 3*8 + 8 + 24 + 8
    static_assert(sizeof(RadicalBox)                ==  96, ""); // (48) + 8 + 8 + 12 + 4 + 4 + 2*4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(SubsuperscriptBox)         ==  88, ""); // (48) + 8 + 8 + 2*4 + 2*4 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(UnderoverscriptBox)        ==  96, ""); // (48) + 3*8 + 4 + 2*4 + 2*4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(TemplateBox)               ==  88, ""); // (0 + 64) + 8 + 8 + 8
    static_assert(sizeof(TemplateBoxSlot)           ==  72, ""); // (64) + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(SectionGroupInfo)          ==  20, ""); // 4 + 3*4 + 4
    static_assert(sizeof(Section)                   == 104, ""); // (48) + 4*4 + 4 + 20 + 8 + 8
    static_assert(sizeof(SectionOrnament)           ==  16, ""); // 8 + 8
    static_assert(sizeof(AbstractSequenceSection)   == 136, ""); // (104) + 8 + 16 + 4 + 4
    static_assert(sizeof(MathSection)               == 136, "");
    static_assert(sizeof(TextSection)               == 136, "");
    static_assert(sizeof(EditSection)               == 144, ""); // (136) + 8
    static_assert(sizeof(StyleDataSection)          == 144, ""); // (136) + 8
    static_assert(sizeof(AxisTicks)                 == 152, ""); // (48) + 4*8 + 4 + 9*4 + 3*8 + 8
    static_assert(sizeof(GraphicsBounds)            ==  80, ""); // 6*8 + 4*8
    static_assert(sizeof(GraphicsElement)           ==  24, ""); // (16) + 8
    static_assert(sizeof(GraphicsElementCollection) ==  32, ""); // (24) + 8
    static_assert(sizeof(GraphicsBox)               == 176, ""); // (48) + 4 + 7*4 + 6*8 + 8 + 32 + 8
    static_assert(sizeof(GraphicsDirective)         ==  64, ""); // (24) + 8 + 24 + 8
    static_assert(sizeof(LineBox)                   ==  40, ""); // (24) + 8 + 8
    static_assert(sizeof(DoubleMatrix)              ==  40, ""); // 8 + 3*8 + 8
    static_assert(sizeof(PointBox)                  ==  72, ""); // (24) + 8 + 40
#  elif defined(PMATH_32BIT)
    static_assert(sizeof(Array<void*>)              ==  4,  "");
    static_assert(sizeof(Array<bool>)               ==  4,  "");
    static_assert(sizeof(RleArray<void*>)           ==  4,  "");
    static_assert(sizeof(Matrix<void*>)             ==  8,  ""); // 4 + 4
    static_assert(sizeof(Dynamic)                   ==  16, ""); // 8 + 4 + 4 (enum where 1 byte would suffice)
    static_assert(sizeof(PartialDynamic)            ==  24, ""); // 8 + 8 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(FrontEndReference)         ==   4, "");
    static_assert(sizeof(FrontEndObject)            ==  12, ""); // 4 + 4 + 4
    static_assert(sizeof(StyledObject)              ==  12, "");
    static_assert(sizeof(ActiveStyledObject)        ==  16, ""); // (12) + 4
    static_assert(sizeof(BoxSize)                   ==  12, ""); // 3*4
    static_assert(sizeof(Box)                       ==  36, ""); // (16) + 12 + 4 + 4
    static_assert(sizeof(AbstractSequence)          ==  56, ""); // (36) + 8 + 4 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(MathSequence)              ==  80, ""); // (56) + 2*4 + 3*4 + 4
    static_assert(sizeof(TextSequence)              ==  80, ""); // (56) + 4 + 4 + 4 + 4 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(EmptyWidgetBox)            ==  48, ""); // (32 + 4) + 4 + 3*1 + 1 UNUSED PADDING BTYE + 4
    static_assert(sizeof(CheckboxBox)               ==  80, ""); // (48) + 16 + 8 + 8
    static_assert(sizeof(OpenerBox)                 ==  72, ""); // (48) + 16 + 8
    static_assert(sizeof(RadioButtonBox)            ==  72, ""); // (48) + 16 + 8
    static_assert(sizeof(SliderBox)                 == 128, ""); // (48) + 6*8 + 8 + 16 + 4 + 4
    static_assert(sizeof(OwnerBox)                  ==  48, ""); // (36) + 4 + 4 + 4
    static_assert(sizeof(ExpandableOwnerBox)        ==  48, "");
    static_assert(sizeof(InlineSequenceBox)         ==  48, "");
    static_assert(sizeof(AbstractStyleBox)          ==  48, "");
    static_assert(sizeof(StyleBox)                  ==  48, "");
    static_assert(sizeof(InterpretationBox)         ==  56, ""); // (48) + 8
    static_assert(sizeof(TagBox)                    ==  56, ""); // (48) + 8
    static_assert(sizeof(TooltipBox)                ==  56, ""); // (48) + 8
    static_assert(sizeof(NumberBox)                 ==  72, ""); // (48) + 8 + 4*4
    static_assert(sizeof(ContainerWidgetBox)        ==  60, ""); // (48 + 4) + 4 + 2*1 + 2 UNUSED PADDING BYTES
    static_assert(sizeof(AbstractButtonBox)         ==  60, "");
    static_assert(sizeof(ButtonBox)                 ==  60, "");
    static_assert(sizeof(SetterBox)                 ==  88, ""); // (60) + 4 UNUSED PADDING BYTES + 16 + 8
    static_assert(sizeof(InputFieldBox)             ==  96, ""); // (60) + 4 + 8 + 16 + 8
    static_assert(sizeof(AbstractDynamicBox)        ==  48, "");
    static_assert(sizeof(DynamicBox)                ==  64, ""); // (48) + 16
    static_assert(sizeof(DynamicLocalBox)           ==  64, ""); // (48) + 2*8
    static_assert(sizeof(FillBox)                   ==  52, ""); // (48) + 4
    static_assert(sizeof(FractionBox)               ==  52, ""); // (36) + 4 + 4 + 4 + 4
    static_assert(sizeof(FrameBox)                  ==  52, ""); // (48) + 4
    static_assert(sizeof(GridItem)                  ==  56, ""); // (48) + 4 + 4
    static_assert(sizeof(GridBox)                   ==  60, ""); // (36) + 4 + 4 + 8 + 4 + 4
    static_assert(sizeof(AbstractTransformationBox) ==  96, ""); // (48) + 6*8
    static_assert(sizeof(RotationBox)               == 104, ""); // (96) + 8
    static_assert(sizeof(TransformationBox)         == 104, ""); // (96) + 8
    static_assert(sizeof(PaneBox)                   ==  96, "");
    static_assert(sizeof(PanelBox)                  ==  60, "");
    static_assert(sizeof(PaneSelectorBox)           ==  72, ""); // (36) + 4 + 4 + 4 UNUSED PADDING BYTES + 16 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(ProgressIndicatorBox)      ==  96, ""); // (36 + 4) + 3*8 + 8 + 16 + 4 + 2 (bools) + 2 UNUSED PADDING BYTES
    static_assert(sizeof(RadicalBox)                ==  72, ""); // (36) + 4 + 4 + 12 + 4 + 4 + 2*4
    static_assert(sizeof(SubsuperscriptBox)         ==  64, ""); // (36) + 4 + 4 + 2*4 + 2*4 + 4
    static_assert(sizeof(UnderoverscriptBox)        ==  68, ""); // (36) + 3*4 + 4 + 2*4 + 2*4
    static_assert(sizeof(TemplateBox)               ==  72, ""); // (0 + 48) + 8 + 8 + 8
    static_assert(sizeof(TemplateBoxSlot)           ==  52, ""); // (48) + 4
    static_assert(sizeof(SectionGroupInfo)          ==  20, ""); // 4 + 3*4 + 4
    static_assert(sizeof(Section)                   ==  88, ""); // (36) + 4*4 + 4 + 20 + 4 + 8
    static_assert(sizeof(SectionOrnament)           ==  16, ""); // 4 + 4 UNUSED PADDING BYTES + 8
    static_assert(sizeof(AbstractSequenceSection)   == 120, ""); // (88) + 4 + 4 UNUSED PADDING BYTES + 16 + 4 + 4
    static_assert(sizeof(MathSection)               == 120, "");
    static_assert(sizeof(TextSection)               == 120, "");
    static_assert(sizeof(EditSection)               == 128, ""); // (120) + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(StyleDataSection)          == 128, ""); // (120) + 8
    static_assert(sizeof(AxisTicks)                 == 128, ""); // (36) + 4*4 + 4 + 9*4 + 4 UNUSED PADDING BYTES + 3*8 + 8
    static_assert(sizeof(GraphicsBounds)            ==  80, ""); // 6*8 + 4*8
    static_assert(sizeof(GraphicsElement)           ==  16, ""); // (12) + 4
    static_assert(sizeof(GraphicsElementCollection) ==  20, ""); // (16) + 4
    static_assert(sizeof(GraphicsBox)               == 128, ""); // (36) + 4 + 7*4 + 6*4 + 4 + 20 + 4 UNUSED PADDING BYTES + 8
    static_assert(sizeof(GraphicsDirective)         ==  56, ""); // (16) + 4 + 4 UNUSED PADDING BYTES + 24 + 8
    static_assert(sizeof(LineBox)                   ==  32, ""); // (16) + 8 + 4 + 4 UNUSED PADDING BYTES
    static_assert(sizeof(DoubleMatrix)              ==  24, ""); // 8 + 3*4 + 4
    static_assert(sizeof(PointBox)                  ==  48, ""); // (16) + 8 + 24
#  endif
#endif

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
  
  doc->insert(i, BoxFactory::create_section(expr));
  
  doc->move_to(b, i + 1);
}

static void write_text_section(Document *doc, String style, String text) {
  write_section(doc,
                Call(
                  Symbol(richmath_System_Section),
                  text,
                  style));
}

static void todo(Document *doc, String msg) {
  write_text_section(doc, "Todo", msg);
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
  Expr font_files = Expr{pmath_symbol_get_value(richmath_FE_DollarPrivateStartupFontFiles)};
  
  if(font_files[0] == richmath_System_List) {
    for(auto item : font_files.items()) {
      if(FontInfo::add_private_font(String { item })) {
        pmath_debug_print_object("add private font ", item.get(), "\n");
      }
      else {
        pmath_debug_print_object("failed to add private font ", item.get(), "\n");
      }
    }
  }
}

static void load_math_shapers() {
  PMATH_RUN(R"PMATH(
    FileNames(
      ToFileName({FE`$FrontEndDirectory, "resources"}, "shapers"),
      "*.pmath"
    ).ParallelMap(
      Get
    ).Scan(
      Function(FrontEnd`AddConfigShaper(#))
    )
  )PMATH");
  
  Expr prefered_fonts = Expr{pmath_symbol_get_value(richmath_FE_DollarMathShapers)};
  
  SharedPtr<MathShaper> def;
  String def_name;
  
  if(prefered_fonts[0] == richmath_System_List) {
    for(String s : prefered_fonts.items()) {
      SharedPtr<MathShaper> shaper = MathShaper::available_shapers[s];
      
      if(!shaper) {
#ifdef PMATH_DEBUG_LOG
        double start_font = pmath_tickcount();
#endif
        
        shaper = OTMathShaper::try_register(s);
        if(shaper) {
          MathShaper::available_shapers.set(s, shaper);
          
#ifdef PMATH_DEBUG_LOG
          pmath_debug_print_object("loaded ", s.get(), "");
          pmath_debug_print(" in %f seconds\n", pmath_tickcount() - start_font);
#endif
        }
      }
      
      if(shaper && !def) {
        def = shaper;
        def_name = s;
      }
    }
  }
  
  if(!def) {
    for(const auto &e : MathShaper::available_shapers.entries()) {
      def = e.value;
      def_name = e.key;
      break;
    }
  }
  
  Application::front_end_session->style->set(MathFontFamily, def_name);
  MathShaper::available_shapers.default_value = def;
}

static void init_stylesheet() {
  Stylesheet::Default = new Stylesheet;
  
  Stylesheet::Default->base = new Style;
  Stylesheet::Default->base->set(Background,             Color::None);
  Stylesheet::Default->base->set(ColorForGraphics,       Color::Black);
  Stylesheet::Default->base->set(FontColor,              Color::None);
  Stylesheet::Default->base->set(SectionFrameColor,      Color::None);
  
  Stylesheet::Default->base->set(FontSlant,              FontSlantPlain);
  Stylesheet::Default->base->set(FontWeight,             FontWeightPlain);
  
  Stylesheet::Default->base->set(ButtonSource,           ButtonSourceAutomatic);
  
  Stylesheet::Default->base->set(AutoDelete,                          false);
  Stylesheet::Default->base->set(AutoNumberFormating,                 true);
  Stylesheet::Default->base->set(AutoSpacing,                         false);
  Stylesheet::Default->base->set(ClosingAction,                       ClosingActionDelete);
  //Stylesheet::Default->base->set(ContinuousAction,                    false);
  Stylesheet::Default->base->set(Editable,                            true);
  Stylesheet::Default->base->set(Enabled,                             AutoBoolAutomatic);
  Stylesheet::Default->base->set(Evaluatable,                         false);
  Stylesheet::Default->base->set(InternalUsesCurrentValueOfMouseOver, ObserverKindNone);
  Stylesheet::Default->base->set(LineBreakWithin,                     true);
  Stylesheet::Default->base->set(RemovalConditions,                   0);
  Stylesheet::Default->base->set(ReturnCreatesNewSection,             false);
  Stylesheet::Default->base->set(Saveable,                            true);
  Stylesheet::Default->base->set(SectionEditDuplicate,                false);
  Stylesheet::Default->base->set(SectionEditDuplicateMakesCopy,       false);
  Stylesheet::Default->base->set(SectionGenerated,                    false);
  Stylesheet::Default->base->set(Selectable,                          AutoBoolAutomatic);
  Stylesheet::Default->base->set(ShowAutoStyles,                      false);
  Stylesheet::Default->base->set(SectionLabelAutoDelete,              true);
  Stylesheet::Default->base->set(ShowSectionBracket,                  true);
  Stylesheet::Default->base->set(ShowStringCharacters,                true);
  Stylesheet::Default->base->set(Visible,                             true);
  
  Stylesheet::Default->base->set(AspectRatio,                   1.0);
  Stylesheet::Default->base->set(GridBoxColumnSpacing,          0.4);
  Stylesheet::Default->base->set(GridBoxRowSpacing,             0.5);
  Stylesheet::Default->base->set(Magnification,                 1.0);
  
  Stylesheet::Default->base->set(FontSize,                      SymbolicSize::Automatic);
  
//  Stylesheet::Default->base->set(FrameMarginLeft,               SymbolicSize::Automatic);
//  Stylesheet::Default->base->set(FrameMarginRight,              SymbolicSize::Automatic);
//  Stylesheet::Default->base->set(FrameMarginTop,                SymbolicSize::Automatic);
//  Stylesheet::Default->base->set(FrameMarginBottom,             SymbolicSize::Automatic);

  Stylesheet::Default->base->set(SectionMarginLeft,             Length(7.0));
  Stylesheet::Default->base->set(SectionMarginRight,            Length(7.0));
  Stylesheet::Default->base->set(SectionMarginTop,              Length(4.0));
  Stylesheet::Default->base->set(SectionMarginBottom,           Length(4.0));

  Stylesheet::Default->base->set(SectionFrameLeft,              Length(0.0));
  Stylesheet::Default->base->set(SectionFrameRight,             Length(0.0));
  Stylesheet::Default->base->set(SectionFrameTop,               Length(0.0));
  Stylesheet::Default->base->set(SectionFrameBottom,            Length(0.0));
  
  Stylesheet::Default->base->set(SectionFrameLabelMarginLeft,   Length(3.0));
  Stylesheet::Default->base->set(SectionFrameLabelMarginRight,  Length(3.0));
  Stylesheet::Default->base->set(SectionFrameLabelMarginTop,    Length(3.0));
  Stylesheet::Default->base->set(SectionFrameLabelMarginBottom, Length(3.0));
  
  Stylesheet::Default->base->set(SectionFrameMarginLeft,        Length(0.0));
  Stylesheet::Default->base->set(SectionFrameMarginRight,       Length(0.0));
  Stylesheet::Default->base->set(SectionFrameMarginTop,         Length(0.0));
  Stylesheet::Default->base->set(SectionFrameMarginBottom,      Length(0.0));
  
  Stylesheet::Default->base->set(SectionGroupPrecedence,        0);
  
  Stylesheet::Default->base->set(ContextMenu,               List());
  Stylesheet::Default->base->set(DockedSectionsBottom,      List());
  Stylesheet::Default->base->set(DockedSectionsBottomGlass, List());
  Stylesheet::Default->base->set(DockedSectionsTop,         List());
  Stylesheet::Default->base->set(DockedSectionsTopGlass,    List());
  Stylesheet::Default->base->set(DragDropContextMenu,       List());
  Stylesheet::Default->base->set(FontFamilies,              List());
  Stylesheet::Default->base->set(InputAliases,              List());
  Stylesheet::Default->base->set(InputAutoReplacements,     List());
  
  Stylesheet::Default->base->set_pmath(Initialization,   Symbol(richmath_System_None));
  Stylesheet::Default->base->set_pmath(Deinitialization, Symbol(richmath_System_None));
  
  Stylesheet::Default->base->set(EvaluationContext,         strings::Global_namespace);
  Stylesheet::Default->base->set(SectionLabel,              "");
  Stylesheet::Default->base->set(SectionEvaluationFunction, Symbol(richmath_System_Identity));
  
  Stylesheet::Default->base->set_pmath(Method,
                                       Expr(pmath_option_value(
                                           richmath_System_ButtonBox,
                                           richmath_System_Method,
                                           PMATH_UNDEFINED)));
  
  Stylesheet::Default->base->set(ButtonFunction,
                                 Expr(pmath_option_value(
                                        richmath_System_ButtonBox,
                                        richmath_System_ButtonFunction,
                                        PMATH_UNDEFINED)));
                                        
//  Stylesheet::Default->base->set(FontFeatures,
//                                 List(Rule(String("ssty"), Symbol(richmath_System_Automatic))));

//  Stylesheet::Default->base->set(GeneratedSectionStyles,
//                                 Parse("{~FE`Private`style :> FE`Private`style}"));

//  Stylesheet::Default->styles.set("SystemResetStyle", Stylesheet::Default->base);

  Application::front_end_session->style->set(DebugFollowMouse, false);
  Application::front_end_session->style->set(DebugSelectionBounds, false);
#ifdef NDEBUG
  Application::front_end_session->style->set(DebugColorizeChanges, false);
#else
  Application::front_end_session->style->set(DebugColorizeChanges, true);
#endif

  Application::front_end_session->style->set(ScriptLevel,           0);
  Application::front_end_session->style->set(ScriptSizeMultipliers, Symbol(richmath_System_Automatic));
}

static bool have_visible_documents() {
  for(auto win : CommonDocumentWindow::All) {
    if(win->content()->get_style(Visible, true)) 
      return true;
  }
  
  return false;
}

int main(int argc, char **argv) {
  auto leak_detection_start = Base::debug_alloc_clock();
  
  debug_test_rle_array();
  
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
#  ifdef PMATH_USE_WINDOWS_THREADS
  printf("main thread id %u\n", GetCurrentThreadId());
#  endif
  printf("cairo version: %s\n", cairo_version_string());
  printf("pango version: %s\n", pango_version_string());
  
#  ifdef RICHMATH_USE_FT_FONT
  printf("fontconfig version: %d\n", FcGetVersion());
#  endif
  
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
  EvaluationContexts::init();
  
  GeneralSyntaxInfo::std = new GeneralSyntaxInfo;
  
  // do not depend on console window size:
  pmath_symbol_set_value(richmath_System_DollarPageWidth, PMATH_FROM_INT32(72));
  
  {
    pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(richmath_System_DollarFrontEndSession);
    pmath_symbol_set_attributes(richmath_System_DollarFrontEndSession, 0);
    pmath_symbol_set_value(     richmath_System_DollarFrontEndSession, Application::front_end_session->to_pmath_id().release());
    pmath_symbol_set_attributes(richmath_System_DollarFrontEndSession, attr | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
  }
  
  PMATH_RUN("BeginPackage(\"FE`\")");
  
  {
    double start = pmath_tickcount();
    
#define SHORTCUTS_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"shortcuts.pmath\"))"
#define MAIN_MENU_CMD  "Get(ToFileName({FE`$FrontEndDirectory,\"resources\"},\"mainmenu.pmath\"))"
    
#ifdef RICHMATH_USE_WIN32_GUI
    Win32Version::init();
    Win32Themes::init();
    Win32HighDpi::init();
    Win32Touch::init();
    Win32Clipboard::init();
    Win32AcceleratorTable::main_table = new Win32AcceleratorTable(Evaluate(Parse(SHORTCUTS_CMD)));
    Win32Menu::main_menu              = new Win32Menu(Evaluate(Parse(MAIN_MENU_CMD)),  false);
#endif
    
#ifdef RICHMATH_USE_GTK_GUI
    MathGtkCss::init();
    Clipboard::std = &MathGtkClipboard::obj;
    MathGtkAccelerators::load(Evaluate(Parse(SHORTCUTS_CMD)));
    MathGtkMenuBuilder::main_menu  = MathGtkMenuBuilder(Evaluate(Parse(MAIN_MENU_CMD)));
#endif

    double end = pmath_tickcount();
    
    pmath_debug_print("[%f sec reading menus]\n", end - start);
  }
  
  load_fonts();
  load_math_shapers();
  
  init_stylesheet();
  
  PMATH_RUN(
    "EndPackage();" /* FE` */
    "$NamespacePath:= {\"System`\"}");
  
  RecentDocuments::init();
  
  int result = 0;
  
  if(!MathShaper::available_shapers.default_value) {
    message_dialog("pMath Fatal Error",
                   "Cannot start pMath because there is no math font on this system.");
                   
    result = 1;
    goto QUIT;
  }
  
  PMATH_RUN("Get(ToFileName({FE`$FrontEndDirectory},\"frontinit-stage2.pmath\"))");
  
  if(!Documents::selected_document()) {
    Document *main_doc = Application::try_create_document();
    if(main_doc) {
      write_text_section(main_doc, "Title", "Welcome");
      write_text_section(main_doc, "Section", "Todo-List");
      todo(main_doc, "Leave caret at end of line at automatic line breaks.");
      todo(main_doc, "Navigation: ALT-left/right: previous/next span/sentence.");
      todo(main_doc, "Resize every section, not only the visible ones.");
      todo(main_doc, "Add option to always show menu bar.");
      todo(main_doc, "CTRL-R or F2 to refactor local variable names.");
      todo(main_doc, "Add CounterBox, CounterAssignments, CounterIncrements.");
      todo(main_doc, "Undo/Redo support.");
      main_doc->select(main_doc, 0, 0);
      main_doc->move_horizontal(LogicalDirection::Forward,  true);
      main_doc->move_horizontal(LogicalDirection::Backward, false);
      
      main_doc->on_style_changed(true);
    }
  
    if(main_doc) {
      main_doc->native()->bring_to_front();
    }
  }
  
  if(!have_visible_documents()) {
    message_dialog("pMath Error",
                   "No document window could be opened. pMath will quit now.");
  }
  else
    Application::run();
    
QUIT:
  pmath_debug_print("quitted\n");
  
  Observatory::shutdown();
  RecentDocuments::done();
  
#ifdef RICHMATH_USE_GTK_GUI
  MathGtkTooltipWindow::delete_global_tooltip();
#endif
  
  MathShaper::available_shapers.clear();
  MathShaper::available_shapers.default_value = nullptr;
  
  ConfigShaper::dispose_all();
  OTMathShaper::dispose_all();
  
  TextShaper::clear_cache();
  FontInfo::remove_all_private_fonts();
  
  Stylesheet::Default = nullptr;
  
  GeneralSyntaxInfo::std = nullptr;
  
  EvaluationContexts::done();
  Application::done();
  
#ifdef RICHMATH_USE_WIN32_GUI
  Win32Clipboard::done();
  Win32ControlPainter::done();
  Win32Menu::main_menu              = nullptr;
  Win32AcceleratorTable::main_table = nullptr;
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  MathGtkMenuBuilder::done();
  MathGtkAccelerators::done();
  MathGtkControlPainter::done();
  MathGtkCss::done();
#endif
  
  // needed to clear the message_queue member:
  Server::local_server = nullptr;
  
  done_bindings();
  Base::debug_check_leaks_after(leak_detection_start);
  
  pmath_done();
  
  return result;
}
