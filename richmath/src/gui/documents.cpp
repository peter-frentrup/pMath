#include <gui/documents.h>

#include <boxes/section.h>

#include <gui/common-document-windows.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/recent-documents.h>

#include <eval/current-value.h>

#include <syntax/spanexpr.h>

#include <util/autovaluereset.h>
#include <util/filesystem.h>

#include <algorithm>
#include <cmath>


namespace richmath {
  namespace strings {
    extern String DocumentDirectory;
    extern String DocumentFileName;
    extern String DocumentFullFileName;
    extern String EditStyleDefinitions;
    extern String Input;
    extern String MenuListPalettesMenu;
    extern String Output;
    extern String PageWidthCharacters;
    extern String ShowHideMenu;
  }
  
  struct DocumentsImpl {
    static bool show_hide_menu_cmd(Expr cmd);
    static MenuCommandStatus can_show_hide_menu(Expr cmd);

    static bool open_selection_help_cmd(Expr cmd);
    
    static bool edit_style_definitions_cmd(Expr cmd);
    static MenuCommandStatus can_edit_style_definitions(Expr cmd);
    static Document *open_private_style_definitions(Document *doc, bool create);
    
    static String get_style_name_at(VolatileSelection sel);
    static Section *find_style_definition(Expr stylesheet_name, String style_name);
    static Section *find_style_definition(Document *style_doc, int index, String style_name);
    static bool find_style_definition_cmd(Expr cmd);

    static void collect_selections(Array<SelectionReference> &sels, Expr expr);
  };
}

using namespace richmath;
using namespace std;

namespace {
  class DocumentCurrentValueProvider {
    public:
      static void init();
      static void done();
      
    private:
      static Expr get_DocumentDirectory(FrontEndObject *obj, Expr item);
      static bool put_DocumentDirectory(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_DocumentFileName(FrontEndObject *obj, Expr item);
      static bool put_DocumentFileName(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_DocumentFullFileName(FrontEndObject *obj, Expr item);
      static bool put_DocumentFullFileName(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_PageWidthCharacters(FrontEndObject *obj, Expr item);
      static Expr get_WindowTitle(FrontEndObject *obj, Expr item);
  };
  
  class StylesMenuImpl final : public FrontEndObject {
    public:
      static void init();
      static void done();
    
    private:
      static MenuCommandStatus can_set_style(Expr cmd);
      static bool set_style(Expr cmd);
      
      static Expr enum_styles_menu(Expr name);
      
      static bool find_style_definition(Expr submenu_cmd, Expr item_cmd);
    
    protected:
      virtual ObjectWithLimbo *next_in_limbo() final override { return nullptr; }
      virtual void next_in_limbo(ObjectWithLimbo *next) final override { RICHMATH_ASSERT(0 && "not supported"); }
    
    private:
      StylesMenuImpl();
      
      void clear_cache();
      virtual void dynamic_updated() override;
      
      String style_name_from_command(Expr cmd);
      String style_name_from_command_key(int command_key);
      
      struct StyleItem {
        int sorting_value;
        int command_key;
        String name;
        
        friend bool operator<(const StyleItem &left, const StyleItem &right) { return left.sorting_value < right.sorting_value; }
      };
      
      const Array<StyleItem> &enum_styles();
      
      static StylesMenuImpl cache;
      Stylesheet *latest_stylesheet;
      Array<StyleItem> latest_items;
  };
  
  class SelectDocumentMenuImpl {
    public:
      static void init();
      static void done();
    
    private:
      static MenuCommandStatus can_set_selected_document(Expr cmd);
      static bool set_selected_document_cmd(Expr cmd);
      
      static Expr enum_windows_menu(Expr name);
      
      static MenuCommandStatus can_locate_window(Expr submenu_cmd, Expr item_cmd);
      static bool locate_window(Expr submenu_cmd, Expr item_cmd);
      static bool remove_window(Expr submenu_cmd, Expr item_cmd);
  };
  
  class OpenDocumentMenuImpl {
    public:
      static void init();
      static void done();
    
    private:
      static bool document_open_cmd(Expr cmd);
      
      static Expr enum_palettes_menu(Expr name);
      static Expr enum_recent_documents_menu(Expr name);

      static bool locate_document(Expr submenu_cmd, Expr item_cmd);
      static bool remove_recent_document(Expr submenu_cmd, Expr item_cmd);
  };
}

Expr richmath_eval_FrontEnd_AttachBoxes(Expr expr);
Expr richmath_eval_FrontEnd_CreateDocument(Expr expr);
Expr richmath_eval_FrontEnd_DocumentClose(Expr expr);
Expr richmath_eval_FrontEnd_DocumentDelete(Expr expr);
Expr richmath_eval_FrontEnd_DocumentGet(Expr expr);
Expr richmath_eval_FrontEnd_DocumentOpen(Expr expr);
Expr richmath_eval_FrontEnd_Documents(Expr expr);
Expr richmath_eval_FrontEnd_FindStyleDefinition(Expr expr);
Expr richmath_eval_FrontEnd_KeyboardInputBox(Expr expr);
Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr);
Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr);
Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr);

extern pmath_symbol_t richmath_Documentation_FindSymbolDocumentationByFullName;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_FindStyleDefinition;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_SystemOpenDirectory;

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_BaseStyle;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_CreateDocument;
extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_FileNames;
extern pmath_symbol_t richmath_System_Infinity;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_MakeBoxes;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_StyleDefinitions;
extern pmath_symbol_t richmath_System_TimeConstrained;
extern pmath_symbol_t richmath_System_True;
extern pmath_symbol_t richmath_System_WindowTitle;

namespace richmath {
  namespace strings {
    extern String MenuListRecentDocuments;
    extern String MenuListStyles;
    extern String MenuListWindows;
  }
}

//{ class Documents ...

bool Documents::init() {
  Menus::register_command(strings::EditStyleDefinitions,                 Impl::edit_style_definitions_cmd, Impl::can_edit_style_definitions);
  Menus::register_command(Symbol(richmath_FrontEnd_FindStyleDefinition), Impl::find_style_definition_cmd);
  Menus::register_command(String("OpenSelectionHelp"),                   Impl::open_selection_help_cmd);
  Menus::register_command(strings::ShowHideMenu,                         Impl::show_hide_menu_cmd, Impl::can_show_hide_menu);
  
  OpenDocumentMenuImpl::init();
  SelectDocumentMenuImpl::init();
  StylesMenuImpl::init();
  DocumentCurrentValueProvider::init();
  
  return true;
}

void Documents::done() {
  DocumentCurrentValueProvider::done();
  SelectDocumentMenuImpl::done();
  StylesMenuImpl::done();
  OpenDocumentMenuImpl::done();
}

ObservableValue<FrontEndReference> Documents::selected_document_id { FrontEndReference::None };
ObservableValue<FrontEndReference> Documents::focused_document_id { FrontEndReference::None };

Document *Documents::selected_document() {
  return FrontEndObject::find_cast<Document>(selected_document_id);
}

void Documents::selected_document(Document *document) {
  FrontEndReference id = document ? document->id() : FrontEndReference::None;
  if(selected_document_id.unobserved_equals(id))
    return;
  
  if(auto old = selected_document()) 
    old->focus_killed(document);
    
  if(document)
    document->focus_set();
    
  selected_document_id = id;
}

Box *Documents::keyboard_input_box() {
  if(auto doc = focused_document()) {
    if(auto box = doc->selection_box())
      return box;
  }
  
  if(auto doc = selected_document()) {
    if(auto box = doc->selection_box())
      return box;
  }
  
  return nullptr;
}

Document *Documents::focused_document() {
  return FrontEndObject::find_cast<Document>(focused_document_id);
}

void Documents::focus_gained(Document *document) {
  FrontEndReference id = document ? document->id() : FrontEndReference::None;
  
  focused_document_id = id;
}

bool Documents::focus_lost(Document *old_focus_doc) {
  if(!old_focus_doc)
    return false;
  
  FrontEndReference id = old_focus_doc->id();
  if(!focused_document_id.unobserved_equals(id))
    return false;
  
  focused_document_id = FrontEndReference::None;
  return true;
}

Expr Documents::make_section_boxes(Expr boxes, Document *doc) {
  if(boxes[0] == richmath_System_Section)
    return boxes;
    
  if(boxes[0] == richmath_System_SectionGroup)
    return boxes;
  
  if(boxes[0] != richmath_System_BoxData) {
    boxes = Application::interrupt_wait(Call(Symbol(richmath_System_MakeBoxes), std::move(boxes))); // ToBoxes instead?
    boxes = Call(Symbol(richmath_System_BoxData), std::move(boxes));
  }
  
  return Call(Symbol(richmath_System_Section), 
              std::move(boxes), 
              doc ? doc->get_own_style(DefaultNewSectionStyle, strings::Input) : strings::Input);
}

bool Documents::locate_document_from_command(Expr item_cmd) {
  if(item_cmd.expr_length() >= 1 && item_cmd[0] == richmath_FrontEnd_DocumentOpen) {
    String path{ item_cmd[1] };
    if(path.length() > 0) {
      Expr expr = Call(Symbol(richmath_FrontEnd_SystemOpenDirectory), path);
      expr = Evaluate(std::move(expr));
      return expr.is_null();
    }
  }
  return false;
}

//} ... class Documents

//{ class DocumentsImpl ...

bool DocumentsImpl::show_hide_menu_cmd(Expr cmd) {
  Document * const doc = Documents::selected_document();
  if(!doc)
    return false;
  
  if(!doc->native()->can_toggle_menubar())
    return false;
  return doc->native()->try_set_menubar(!doc->native()->has_menubar());
}

MenuCommandStatus DocumentsImpl::can_show_hide_menu(Expr cmd) {
  Document * const doc = Documents::selected_document();
  if(!doc)
    return false;
  
  MenuCommandStatus status{ doc->native()->can_toggle_menubar() };
  status.checked = doc->native()->has_menubar();
  return status;
}

bool DocumentsImpl::open_selection_help_cmd(Expr cmd) {
  Document * const doc = Documents::selected_document();
  if(!doc)
    return false;
  
  VolatileSelection sel = doc->selection_now();
  VolatileSelection word_src = sel.start_only();
  do {
    word_src.expand();
  } while(word_src.box && !word_src.directly_contains(sel));
  
  if(!word_src.box) {
    doc->native()->beep();
    return false;
  }
  
  doc->select(word_src);
  
  // TODO: give context-dependent help
  
  if(auto seq = dynamic_cast<AbstractSequence *>(word_src.box)) {
    String word = seq->raw_substring(word_src.start, word_src.length());
    
    Expr helpfile = Call(
                      Symbol(richmath_Documentation_FindSymbolDocumentationByFullName), 
                      std::move(word));
    helpfile = Call(Symbol(richmath_System_TimeConstrained), std::move(helpfile), Application::button_timeout);
    helpfile = Evaluate(std::move(helpfile));
    
    if(helpfile.is_string()) {
      Document *helpdoc = Application::find_open_document(helpfile);
      if(!helpdoc) 
        helpdoc = Application::open_new_document(helpfile);
      
      if(helpdoc) {
        if(helpdoc->selectable())
          helpdoc->native()->bring_to_front();
        return true;
      }
    }
  }
  
//  if(auto seq = dynamic_cast<MathSequence *>(doc->selection_box())) {
//    int pos = doc->selection_start();
//    int end = doc->selection_end();
//    SpanExpr *span = new SpanExpr(pos, seq);
//    
//    while(span) {
//      if(span->start() <= pos && span->end() >= end && span->length() > 1)
//        break;
//        
//      span = span->expand(true);
//    }
//    
//    if(!span) 
//      return false;
//    
//    if(span->count() == 0) {
//      // TODO: get symbol namespace from context
//      
//      String name = span->as_text();
//      
//      doc->select(seq, span->start(), span->end() + 1);
//      delete span;
//      span = nullptr;
//      
//      Expr call = Call(
//                    Symbol(richmath_Documentation_FindSymbolDocumentationByFullName), 
//                    std::move(name));
//      call = Call(Symbol(richmath_System_TimeConstrained), std::move(call), Application::button_timeout);
//      call = Evaluate(std::move(call));
//      
//      return FrontEndReference::from_pmath(std::move(call)).is_valid();
//    }
//    
//    delete span;
//  }
  
  doc->native()->beep();
  return false;
}

bool DocumentsImpl::edit_style_definitions_cmd(Expr cmd) {
  Document *doc = Documents::selected_document();
  
  if(!doc || !doc->editable())
    return false;
  
  if(doc->native()->owner_document())
    return false;
  
  Document *style_doc = open_private_style_definitions(doc, true);
  if(!style_doc)
    return false;
    
  style_doc->native()->bring_to_front();
  return true;
}

MenuCommandStatus DocumentsImpl::can_edit_style_definitions(Expr cmd) {
  Document *doc = Documents::selected_document();
  
  if(!doc || !doc->editable())
    return MenuCommandStatus(false);
  
  if(doc->native()->owner_document())
    return MenuCommandStatus(false);
  
  Expr stylesheet = doc->get_style(StyleDefinitions);
  MenuCommandStatus status{ true };
  status.checked = stylesheet[0] == richmath_System_Document;
  return status;
}

Document *DocumentsImpl::open_private_style_definitions(Document *doc, bool create) {
  if(!doc)
    return nullptr;
  
  Document *style_doc = doc->native()->stylesheet_document();
  if(style_doc) 
    return style_doc;
  
  Expr stylesheet = doc->get_style(StyleDefinitions);
  if(create && stylesheet[0] != richmath_System_Document) {
    stylesheet = Call(
                   Symbol(richmath_System_Document),
                   Call(
                     Symbol(richmath_System_Section),
                     Call(
                       Symbol(richmath_System_StyleData),
                       Rule(Symbol(richmath_System_StyleDefinitions), stylesheet))),
                   Rule(Symbol(richmath_System_StyleDefinitions), String("PrivateStyleDefinitions.pmathdoc")));
    
    doc->style->set(StyleDefinitions, stylesheet);
  }
  
  if(stylesheet[0] != richmath_System_Document)
    return nullptr;
  
  style_doc = Application::try_create_document(stylesheet);
  if(!style_doc)
    return nullptr;
  
  if(!doc->native()->stylesheet_document(style_doc))
    doc->native()->beep();
  
  doc->on_style_changed(true);
  style_doc->on_style_changed(true);
  return style_doc;
}

String DocumentsImpl::get_style_name_at(VolatileSelection sel) {
  VolatileSelection inner_sel = sel;
  while(Box *inner = inner_sel.contained_box()) 
    inner_sel = {inner, 0, inner->length()};
  
  for(Box *tmp = inner_sel.box; tmp; tmp = tmp->parent()) {
    if(auto style_sect = dynamic_cast<StyleDataSection*>(tmp)) {
      if(String style_name = style_sect->style_data[1])
        return style_name;
    }
    else if(auto mseq = dynamic_cast<MathSequence*>(tmp)) {
      if(mseq == sel.box) {
        SpanExpr *span = SpanExpr::find(mseq, sel.start);
        
        while(span && !span->range().directly_contains(sel))
          span = span->expand();
        
        for(; span; span = span->expand()) {
          String style_name;
          switch(span->as_token()) {
            case PMATH_TOK_STRING:
              style_name = span->as_text();
              if(style_name.length() > 2 && style_name[style_name.length()-1] == '"' && style_name[0] == '"') {
                // TODO: proper un-escaping
                style_name = style_name.part(1, style_name.length() - 2);
              }
              else
                style_name = String();
              break;
            
            default: break;
          }
          
          if(style_name) {
            delete span;
            return style_name;
          }
        }
      }
    }
    else if(String style_name = tmp->get_own_style(BaseStyleName)) {
      return style_name;
    }
  }
  
  return {};
}

Section *DocumentsImpl::find_style_definition(Expr stylesheet_name, String style_name) {
  if(SharedPtr<Stylesheet> stylesheet = Stylesheet::find_registered(stylesheet_name)) {
    if(stylesheet->styles.search(style_name)) {
      if(String path = Stylesheet::path_for_name(stylesheet_name)) {
        Document *doc = Application::find_open_document(path);
        bool was_open = doc;
        if(!doc) 
          doc = Application::open_new_document(path);
        
        if(doc) {
          auto result = find_style_definition(doc, doc->length(), style_name);
          
          if(doc->is_parent_of(result)) {
            doc->native()->bring_to_front();
          }
          else if(!was_open) {
            // Closing the window too soon seems to sometimes cause a crash on windows in "OleMainThreadWndClass" ?
            //doc->native()->close();
          }
          
          if(result)
            return result;
        }
      }
      else {
        pmath_debug_print_object("[Stylesheet::path_for_name failed for ", stylesheet_name.get(), "]\n");
      }
    }
    else {
      pmath_debug_print_object("[skip stylesheet ", stylesheet_name.get(), "]\n");
    }
  }
  else {
    pmath_debug_print_object("[stylesheet ", stylesheet_name.get(), " not loaded]\n");
  }
  
  return nullptr;
}

Section *DocumentsImpl::find_style_definition(Document *style_doc, int index, String style_name) {
  static int max_recursion = 10;
  
  AutoValueReset<int> recursion_guard(max_recursion);
  if(max_recursion-- <= 0) {
    pmath_debug_print("[find_style_definition: recursion limit reached]\n");
    return nullptr;
  }
  
  while(--index >= 0) {
    Section *sect = style_doc->section(index);
    StyleDataSection *style_sect = dynamic_cast<StyleDataSection*>(sect);
    if(!style_sect) {
      if(EditSection *edit = dynamic_cast<EditSection*>(sect))
        style_sect = dynamic_cast<StyleDataSection*>(edit->original);
    }
    
    if(!style_sect)
      continue;
    
    if(String name = style_sect->style_data[1]) {
      if(name == style_name) {
        style_doc->select(sect, 0, 0);
        style_doc->native()->bring_to_front();
        return sect;
      }
    }
    else {
      Expr def = style_sect->style_data[1];
      if(def.is_rule() && def[1] == richmath_System_StyleDefinitions) {
        if(auto result = find_style_definition(def[2], style_name))
          return result;
      }
    }
  }
  
  return nullptr;
}

bool DocumentsImpl::find_style_definition_cmd(Expr cmd) {
  if(cmd[0] == richmath_FrontEnd_FindStyleDefinition) {
    cmd = richmath_eval_FrontEnd_FindStyleDefinition(std::move(cmd));
    return cmd != richmath_System_DollarFailed;
  }
  
  return false;
}

void DocumentsImpl::collect_selections(Array<SelectionReference> &sels, Expr expr) {
  if(expr[0] == richmath_System_List) {
    for(auto item : expr.items())
      collect_selections(sels, std::move(item));
    return;
  }
  
  if(auto sel = SelectionReference::from_pmath(expr)) {
    sels.add(sel);
    return;
  }
  
  if(FrontEndReference id = FrontEndReference::from_pmath(expr)) {
    FrontEndObject *obj = FrontEndObject::find(id);
    if(auto doc = dynamic_cast<Document*>(obj)) {
      sels.add(doc->selection());
      return;
    }
    
    if(auto box = dynamic_cast<Box*>(obj)) {
      if(box->find_parent<Document>(false)) {
        sels.add(SelectionReference(box->parent(), box->index(), box->index() + 1));
      }
      return;
    }
    
    return;
  }
}

//} ... class DocumentsImpl

//{ class DocumentCurrentValueProvider ...

void DocumentCurrentValueProvider::init() {
  CurrentValue::register_provider(strings::DocumentDirectory,          get_DocumentDirectory,    put_DocumentDirectory);
  CurrentValue::register_provider(strings::DocumentFileName,           get_DocumentFileName,     put_DocumentFileName);
  CurrentValue::register_provider(strings::DocumentFullFileName,       get_DocumentFullFileName, put_DocumentFullFileName);
  CurrentValue::register_provider(strings::PageWidthCharacters,        get_PageWidthCharacters);
  CurrentValue::register_provider(Symbol(richmath_System_WindowTitle), get_WindowTitle,           Style::put_current_style_value);
}

void DocumentCurrentValueProvider::done() {
}

Expr DocumentCurrentValueProvider::get_DocumentDirectory(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
    
  String result = doc->native()->directory();
  if(!result.is_valid())
    return Symbol(richmath_System_None);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentDirectory(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == richmath_System_None) {
    doc->native()->directory(String{});
    return true;
  }
  
  if(!rhs.is_string())
    return false;
  
  // TODO: do not require that the directory already exists.
  String dir = FileSystem::to_possibly_nonexisting_absolute_file_name(String{rhs});
  if(dir.is_null())
    return false;
  
  doc->native()->directory(std::move(dir));
  return true;
}

Expr DocumentCurrentValueProvider::get_DocumentFileName(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(richmath_System_None);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentFileName(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == richmath_System_None) {
    doc->native()->filename(String{});
    return true;
  }
  
  if(!rhs.is_string())
    return false;
  
  String name{std::move(rhs)};
  if(!FileSystem::is_filename_without_directory(name))
    return false;
  
  doc->native()->filename(std::move(name));
  return true;
}

Expr DocumentCurrentValueProvider::get_DocumentFullFileName(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
    
  String result = doc->native()->full_filename();
  if(!result.is_valid())
    return Symbol(richmath_System_None);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentFullFileName(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == richmath_System_None) {
    doc->native()->full_filename(String{});
    return true;
  }
  
  if(!rhs.is_string())
    return false;
  
  String path = FileSystem::to_possibly_nonexisting_absolute_file_name(String(std::move(rhs)));
  if(path.is_null())
    return false;
  
  doc->native()->full_filename(std::move(path));
  return true;
}

Expr DocumentCurrentValueProvider::get_PageWidthCharacters(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  
  if(doc) {
    // TODO: consider ImageSize inside a PaneBox?
    float page_width_points = doc->native()->page_size().x;
    if(page_width_points <= 0)
      return Expr(1);
    
    if(!isfinite(page_width_points))
      return Symbol(richmath_System_Infinity);
    
    int section_bracket_nesting = 0;
    
    Length font_size = SymbolicSize::Invalid;
    
    SharedPtr<Style> output_style = nullptr;
    if(auto section = box->find_parent<Section>(true)) {
      output_style = section->style;
      
      font_size = Length(section->get_em());
      
      if(section->parent() == doc) {
        section_bracket_nesting = section->group_info().nesting;
      }
    }
      
    if(!output_style)
      output_style = doc->stylesheet()->styles[strings::Output];
    if(!output_style)
      output_style = doc->stylesheet()->base;
    
    if(!font_size.is_valid())
      font_size = doc->stylesheet()->get_or_default(output_style, FontSize, SymbolicSize::Automatic);
    
    float em    = font_size.resolve(1.0f, LengthConversionFactors::FontSizeInPt);
    float left  = doc->stylesheet()->get_or_default(output_style, SectionMarginLeft ).resolve(em, LengthConversionFactors::SectionMargins);
    float right = doc->stylesheet()->get_or_default(output_style, SectionMarginRight).resolve(em, LengthConversionFactors::SectionMargins);
    
    // TODO: frame margin is only applied if a frame is visible (or Background is given)
    left+=  doc->stylesheet()->get_or_default(output_style, SectionFrameLeft ).resolve(em, LengthConversionFactors::SectionMargins);
    right+= doc->stylesheet()->get_or_default(output_style, SectionFrameRight).resolve(em, LengthConversionFactors::SectionMargins);
    
    left+=  doc->stylesheet()->get_or_default(output_style, SectionFrameMarginLeft ).resolve(em, LengthConversionFactors::SectionMargins);
    right+= doc->stylesheet()->get_or_default(output_style, SectionFrameMarginRight).resolve(em, LengthConversionFactors::SectionMargins);
    
    page_width_points-= left + right;
    if(doc->get_own_style(ShowSectionBracket, true)) {
      page_width_points-= doc->section_bracket_right_margin + doc->section_bracket_width * section_bracket_nesting;
    }
    
    float average_char_width = 0.5f * em;
    float chars_per_line = roundf(page_width_points / average_char_width);
    if(chars_per_line <= 0)
      return Expr(1);
    
    if(!(chars_per_line < 0xFFFF))
      return Symbol(richmath_System_Infinity);
    
    return Expr((int)chars_per_line);
  }
  
  return Expr(80);
}

Expr DocumentCurrentValueProvider::get_WindowTitle(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  
  if(doc) {
    if(item == richmath_System_WindowTitle) {
      auto result = doc->native()->window_title();
      if(!result.is_null())
        return result;
    }
  }
  
  return Style::get_current_style_value(obj, std::move(item));
}

//} ... class DocumentCurrentValueProvider

//{ class StylesMenuImpl ...

StylesMenuImpl StylesMenuImpl::cache;

StylesMenuImpl::StylesMenuImpl() 
  : FrontEndObject(),
    latest_stylesheet {nullptr}
{
}

void StylesMenuImpl::init() {
  Menus::register_command(String("SelectStyle1"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle2"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle3"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle4"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle5"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle6"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle7"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle8"), set_style, can_set_style);
  Menus::register_command(String("SelectStyle9"), set_style, can_set_style);

  Menus::register_dynamic_submenu(     strings::MenuListStyles, enum_styles_menu);
  Menus::register_submenu_item_locator(strings::MenuListStyles, find_style_definition);
}

void StylesMenuImpl::done() {
  cache.clear_cache();
}

MenuCommandStatus StylesMenuImpl::can_set_style(Expr cmd) {
  Document * const doc = Documents::selected_document();
  if(!doc)
    return false;
  
  if(String name = cache.style_name_from_command(cmd)) {
    return doc->can_do_scoped(
      Rule(Symbol(richmath_System_BaseStyle), std::move(name)), 
      Symbol(richmath_System_Section));
  }
  
  return false;
}

bool StylesMenuImpl::set_style(Expr cmd) {
  Document * const doc = Documents::selected_document();
  if(!doc)
    return false;
    
  if(String name = cache.style_name_from_command(cmd)) {
    return doc->do_scoped(
      Rule(Symbol(richmath_System_BaseStyle), std::move(name)), 
      Symbol(richmath_System_Section));
  }
  
  return false;
}

Expr StylesMenuImpl::enum_styles_menu(Expr name) {
  const Array<StyleItem> &styles = cache.enum_styles();
  
  Expr commands = MakeCall(Symbol(richmath_System_List), (size_t)styles.length());
  for(int i = 0; i < styles.length(); ++i) {
    const StyleItem &item = styles[i];
    
    Expr cmd;
    if(item.command_key >= '1' && item.command_key <= '9') {
      // Alt+1 ... Alt+9 are mapped to "SelectStyle1" ... "SelectStyle9" commands
      cmd = String("SelectStyle") + String::FromChar((unsigned)item.command_key);
    }
    else {
      cmd = Call(
              Symbol(richmath_FE_ScopedCommand), 
              Rule(Symbol(richmath_System_BaseStyle), item.name),
              Symbol(richmath_System_Section));
    }
    commands.set(i + 1, Call(Symbol(richmath_System_MenuItem), item.name, cmd));
  }
  
  return commands;
}

bool StylesMenuImpl::find_style_definition(Expr submenu_cmd, Expr item_cmd) {
  if(String style_name = cache.style_name_from_command(item_cmd)) {
    // Handle message later, or Windows might crash with error 0xc0000409 (stack overrun)
    // during GetMessageW(). 
    // Last received message is WM_USER on a "OleMainThreadWndClass" window, with
    // wParam = 47806, lParam = some pointer.
    // The debugger also mentions an access violation (reading NULL pointer)
    //
    // FIXME: Application::notify still does not delay the message long enough.
    
    Expr expr = Call(Symbol(richmath_FrontEnd_FindStyleDefinition), std::move(style_name));
    //expr = richmath_eval_FrontEnd_FindStyleDefinition(std::move(expr));
    //return expr != richmath_System_DollarFailed;
    Application::notify(ClientNotification::MenuCommand, std::move(expr));
    return true;
  }
  
  return false;
}

void StylesMenuImpl::clear_cache() {
  // Array.length(0) does not destroy the old items
  latest_items = Array<StyleItem>();
  latest_stylesheet = nullptr;
}

void StylesMenuImpl::dynamic_updated() {
  clear_cache();
}

String StylesMenuImpl::style_name_from_command(Expr cmd) {
  if(cmd[0] == richmath_FE_ScopedCommand)
    cmd = cmd[1];
  
  if(String str = cmd) {
    if(str.starts_with("SelectStyle") && str.length() == 12) {
      uint16_t ch = str[11];
      if(ch >= '1' && ch <= '9')
        return style_name_from_command_key(ch);
    }
    return {};
  }
  
  if(cmd.is_rule() && cmd[1] == richmath_System_BaseStyle)
    return cmd[2];
  
  return {};
}

String StylesMenuImpl::style_name_from_command_key(int command_key) {
  for(auto &item : enum_styles()) {
    if(item.command_key == command_key)
      return item.name;
  }
  
  return {};
}
      
const Array<StylesMenuImpl::StyleItem> &StylesMenuImpl::enum_styles() {
  SharedPtr<Stylesheet> stylesheet;
  if(Document *doc = Documents::selected_document())
    stylesheet = doc->stylesheet();
  
  if(!stylesheet) {
    static const Array<StyleItem> empty;
    return empty;
  }
  
  if(latest_stylesheet == stylesheet.ptr())
    return latest_items;
  
  clear_cache();
  latest_stylesheet = stylesheet.ptr();
  latest_stylesheet->add_user(this);
  
  for(const auto &entry : latest_stylesheet->styles.entries()) {
    StyleItem item;
    
    if(entry.value->get(MenuSortingValue, &item.sorting_value) && item.sorting_value > 0) {
      if(!entry.value->get(MenuCommandKey, &item.command_key))
        item.command_key = 0;
      
      item.name = entry.key;
      latest_items.add(item);
    }
  }
  
  std::sort(latest_items.items(), latest_items.items() + latest_items.length());
  
  unsigned used_keys = 0;
  for(auto &style_item : latest_items) {
    if(style_item.command_key) {
      if(style_item.command_key >= '1' && style_item.command_key <= '9') {
        unsigned flag = 1U << (unsigned)(style_item.command_key - '1');
        if(used_keys & flag)
          style_item.command_key = 0; // duplicate MenuCommandKey
        else
          used_keys |= flag;
      }
      else
        style_item.command_key = 0;
    }
  }
  
  return latest_items;
}

//} ... class StylesMenuImpl

//{ class SelectDocumentMenuImpl ...

void SelectDocumentMenuImpl::init() {
  Menus::register_command(Symbol(richmath_FrontEnd_SetSelectedDocument), set_selected_document_cmd, can_set_selected_document);

  Menus::register_dynamic_submenu(     strings::MenuListWindows, enum_windows_menu);
  Menus::register_submenu_item_locator(strings::MenuListWindows, locate_window, can_locate_window);
  Menus::register_submenu_item_deleter(strings::MenuListWindows, remove_window);
}

void SelectDocumentMenuImpl::done() {
}

MenuCommandStatus SelectDocumentMenuImpl::can_set_selected_document(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_SetSelectedDocument)
    return MenuCommandStatus{ false };
  
  if(cmd.expr_length() != 1) 
    return MenuCommandStatus{ false };
  
  Document *doc = Documents::selected_document();
  MenuCommandStatus status { true };
  
  FrontEndReference id = FrontEndReference::from_pmath(cmd[1]);
  if(id)
    status.checked = doc && doc->id() == id;
  else
    status.enabled = false;
  
  return status;
}

bool SelectDocumentMenuImpl::set_selected_document_cmd(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_SetSelectedDocument)
    return false;
  
  cmd = richmath_eval_FrontEnd_SetSelectedDocument(std::move(cmd));
  if(cmd == richmath_System_DollarFailed)
    return false;
  
  return true;
}

Expr SelectDocumentMenuImpl::enum_windows_menu(Expr name) {
  Gather g;
  int i = 1;
  for(auto win : CommonDocumentWindow::All) {
    g.emit(
      Call(
        Symbol(richmath_System_MenuItem), 
        win->title(), 
        //List(name, i)
        Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
          win->content()->to_pmath_id())));
    ++i;
  }
  return g.end();
}

MenuCommandStatus SelectDocumentMenuImpl::can_locate_window(Expr submenu_cmd, Expr item_cmd) {
  if(item_cmd.expr_length() == 1 && item_cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
    auto doc = FrontEndObject::find_cast<Document>(FrontEndReference::from_pmath(item_cmd[1]));
    if(doc) {
      if(doc->native()->full_filename()) 
        return MenuCommandStatus{ true };
      
      if(doc->native()->directory())
        return MenuCommandStatus{ true };
    }
  }
  return MenuCommandStatus{ false };
}

bool SelectDocumentMenuImpl::locate_window(Expr submenu_cmd, Expr item_cmd) {  
  if(item_cmd.expr_length() == 1 && item_cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
    auto doc = FrontEndObject::find_cast<Document>(FrontEndReference::from_pmath(item_cmd[1]));
    if(doc) {
      Expr expr;
      if(String path = doc->native()->full_filename()) 
        expr = Call(Symbol(richmath_FrontEnd_SystemOpenDirectory), std::move(path));
      else if(String dir = doc->native()->directory())
        expr = Call(Symbol(richmath_FrontEnd_SystemOpenDirectory), std::move(dir), List());
      else
        return false;
      
      expr = Evaluate(std::move(expr));
      return !expr.is_null();
    }
  }
  return false;
}

bool SelectDocumentMenuImpl::remove_window(Expr submenu_cmd, Expr item_cmd) {  
  if(item_cmd.expr_length() == 1 && item_cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
    auto doc = FrontEndObject::find_cast<Document>(FrontEndReference::from_pmath(item_cmd[1]));
    if(doc) {
      doc->native()->close();
      return true;
    }
  }
  return false;
}

//} ... class SelectDocumentMenuImpl

//{ class OpenDocumentMenuImpl ...

void OpenDocumentMenuImpl::init() {
  Menus::register_command(Symbol(richmath_FrontEnd_DocumentOpen), document_open_cmd);
  
  Menus::register_dynamic_submenu(     strings::MenuListPalettesMenu, enum_palettes_menu);
  Menus::register_submenu_item_locator(strings::MenuListPalettesMenu, locate_document);
  
  Menus::register_dynamic_submenu(     strings::MenuListRecentDocuments, enum_recent_documents_menu);
  Menus::register_submenu_item_locator(strings::MenuListRecentDocuments, locate_document);
  Menus::register_submenu_item_deleter(strings::MenuListRecentDocuments, remove_recent_document);
}

void OpenDocumentMenuImpl::done() {
}

bool OpenDocumentMenuImpl::document_open_cmd(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_DocumentOpen)
    return false;
  
  size_t exprlen = cmd.expr_length();
  if(exprlen < 1 || exprlen > 2) 
    return false;
  
  Expr result = richmath_eval_FrontEnd_DocumentOpen(std::move(cmd));
  return result != richmath_System_DollarFailed;
}

Expr OpenDocumentMenuImpl::enum_palettes_menu(Expr name) {
  Expr list = Application::interrupt_wait_cached(
                Call(
                  Symbol(richmath_System_FileNames), 
                  Application::palette_search_path, 
                  String("*.pmathdoc")),
                 Application::dynamic_timeout);
  
  if(list[0] == richmath_System_List) {
    for(size_t i = 1; i <= list.expr_length(); ++i) {
      String full = list[i];
      String name = full;
      FileSystem::extract_directory_path(&name);
      
      Expr item;
      if(name.is_string()) {
        int len = name.length();
        if(name.part(len - 9).equals(".pmathdoc"))
          name = name.part(0, len - 9);
        
        item = RecentDocuments::open_document_menu_item(std::move(name), std::move(full), false);
      }
      
      list.set(i, std::move(item));
    }
    
    return list;
  }
  
  return List();
}

Expr OpenDocumentMenuImpl::enum_recent_documents_menu(Expr name) {
  return RecentDocuments::as_menu_list();
}

bool OpenDocumentMenuImpl::locate_document(Expr submenu_cmd, Expr item_cmd) {
  return Documents::locate_document_from_command(std::move(item_cmd));
}

bool OpenDocumentMenuImpl::remove_recent_document(Expr submenu_cmd, Expr item_cmd) {
  if(item_cmd.expr_length() >= 1 && item_cmd[0] == richmath_FrontEnd_DocumentOpen) {
    String path{ item_cmd[1] };
    if(path.length() > 0)
      return RecentDocuments::remove(std::move(path));
  }
  return false;
}

//} ... class OpenDocumentMenuImpl

Expr richmath_eval_FrontEnd_AttachBoxes(Expr expr) {
  /*  FrontEnd`AttachBoxes(sel, anchor, boxes, options)
   */
  
  if(expr.expr_length() < 3)
    return Symbol(richmath_System_DollarFailed);
    
  SelectionReference sel;
  {
    if((sel.id = FrontEndReference::from_pmath(expr[1]))) {
      if(Box *box = sel.get()) {
        sel.start = 0;
        sel.end = box->length();
      }
    }
    else {
      sel = SelectionReference::from_pmath(expr[1]);
    }
  }
  
  Box *anchor_box = sel.get();
  if(!anchor_box)
    return Symbol(richmath_System_DollarFailed);
  
  Document *owner_doc = anchor_box->find_parent<Document>(true);
  if(!owner_doc)
    return Symbol(richmath_System_DollarFailed);
  
  Document *popup_doc = owner_doc->native()->try_create_popup_window(sel);
  if(!popup_doc)
    return Symbol(richmath_System_DollarFailed);
  
  popup_doc->style->set_pmath(ControlPlacement, expr[2]);
  
  // FIXME: this is duplicated in Application::try_create_document():
  Expr options(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_expr())
    popup_doc->style->add_pmath(options);
    
  Expr sections = expr[3];
  if(sections[0] != richmath_System_List)
    sections = List(sections);
    
  for(auto item : sections.items()) {
    int pos = popup_doc->length();
    popup_doc->insert_pmath(&pos, Documents::make_section_boxes(std::move(item), popup_doc));
  }
  
  owner_doc->attach_popup_window(sel, popup_doc);
  popup_doc->on_style_changed(true);
  return popup_doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_CreateDocument(Expr expr) {
  /* FrontEnd`CreateDocument({sections...}, options...)
  */
  
  // TODO: respect window-related options (WindowTitle...)
  
  expr.set(0, Symbol(richmath_System_CreateDocument));
  Document *doc = Application::try_create_document(expr);
  
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
  
  if(!doc->selectable())
    doc->select(nullptr, 0, 0);
    
  doc->on_style_changed(true);
  if(doc->selectable())
    doc->native()->bring_to_front();
  
  return doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_DocumentClose(Expr expr) {
  /*  FrontEnd`DocumentClose()
      FrontEnd`DocumentClose(doc)
   */
  
  size_t exprlen = expr.expr_length();
  if(exprlen > 1)
    return Symbol(richmath_System_DollarFailed);
  
  FrontEndReference docid = FrontEndReference::None;
  if(exprlen == 1) 
    docid = FrontEndReference::from_pmath(expr[1]);
  else
    docid = Documents::selected_document_id;
  
  Document *doc = FrontEndObject::find_cast<Document>(docid);
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
  
  doc->native()->close();
  return Expr();
}

Expr richmath_eval_FrontEnd_DocumentDelete(Expr expr) {
  /*  FrontEnd`DocumentDelete()
      FrontEnd`DocumentDelete(doc)
      FrontEnd`DocumentDelete(box)
      FrontEnd`DocumentDelete(debuginfo)
   */
  size_t exprlen = expr.expr_length();
  if(exprlen > 1)
    return Symbol(richmath_System_DollarFailed);
    
  AutoMemorySuspension mem_suspend;
  
  Array<SelectionReference> sels;
  
  if(exprlen == 0) {
    if(auto doc = Documents::selected_document())
      sels.add(doc->selection());
  }
  else {
    DocumentsImpl::collect_selections(sels, expr[1]);
  }
  
  if(sels.length() == 0 && expr[1][0] != richmath_System_List)
    return Symbol(richmath_System_DollarFailed);
  
  std::sort(sels.items(), sels.items() + sels.length());
  
  int i = 0;
  while(i < sels.length()) {
    SelectionReference &sel = sels[i];
    int j = i + 1;
    for(; j < sels.length(); ++j) {
      SelectionReference &next = sels[j];
      if(sel.id != next.id)
        break;
      
      if(sel.end <= next.start)
        break;
      
      sel.end = next.end;
      next.id = FrontEndReference::None;
    }
    i = j;
  }
  
  for(int i = sels.length(); i > 0; --i) {
    SelectionReference &sel = sels[i-1];
    if(Box *box = sel.get()) {
      if(auto doc = box->find_parent<Document>(true)) {
        doc->remove_selection(sel);
      }
    }
  }
  
  return Expr();
}

Expr richmath_eval_FrontEnd_DocumentGet(Expr expr) {
  /*  FrontEnd`DocumentGet()
      FrontEnd`DocumentGet(selectionOrBox)
   */
  
  size_t exprlen = expr.expr_length();
  if(exprlen > 1)
    return Symbol(richmath_System_DollarFailed);
  
  FrontEndReference docid;
  if(exprlen == 1) {
    docid = FrontEndReference::from_pmath(expr[1]);
    
    if(docid == FrontEndReference::None) {
      if(VolatileSelection sel = SelectionReference::from_pmath(expr[1]).get_all()) {
        return sel.to_pmath(BoxOutputFlags::WithDebugInfo);
      }
    }
  }
  else
    docid = Documents::selected_document_id;
  
  Box *box = FrontEndObject::find_cast<Box>(docid);
  if(!box)
    return Symbol(richmath_System_DollarFailed);
  
  return box->to_pmath(BoxOutputFlags::WithDebugInfo);
}

Expr richmath_eval_FrontEnd_DocumentOpen(Expr expr) {
  /* FrontEnd`DocumentOpen(filename)
     FrontEnd`DocumentOpen(filename, addtorecent)
  */
  size_t exprlen = expr.expr_length();
  if(exprlen < 1 || exprlen > 2)
    return Symbol(richmath_System_DollarFailed);
  
  String filename{expr[1]};
  if(filename.is_null()) 
    return Symbol(richmath_System_DollarFailed);
  
  bool add_to_recent_documents = true;
  if(exprlen == 2) {
    Expr obj = expr[2];
    if(obj == richmath_System_True)
      add_to_recent_documents = true;
    else if(obj == richmath_System_False)
      add_to_recent_documents = false;
    else
      return Symbol(richmath_System_DollarFailed);
  }
  
  filename = FileSystem::to_existing_absolute_file_name(filename);
  if(filename.is_null()) 
    return Symbol(richmath_System_DollarFailed);
  
  Document *doc = Application::find_open_document(filename);
  if(!doc) {
    doc = Application::open_new_document(filename);
    if(!doc)
      return Symbol(richmath_System_DollarFailed);
    
    if(add_to_recent_documents)
      RecentDocuments::add(filename);
  }
  if(doc->selectable())
    doc->native()->bring_to_front();
  return doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_Documents(Expr expr) {
  Gather gather;
  
  for(auto win : CommonDocumentWindow::All) {
    Gather::emit(win->content()->to_pmath_id());
  }
  
  return gather.end();
}

Expr richmath_eval_FrontEnd_FindStyleDefinition(Expr expr) {
  /*  FrontEnd`FindStyleDefinition()
      FrontEnd`FindStyleDefinition(doc)
      FrontEnd`FindStyleDefinition("style")
      FrontEnd`FindStyleDefinition(doc, "style")
   */
   
  size_t exprlen = expr.expr_length();
  if(exprlen > 2)
    return Symbol(richmath_System_DollarFailed);
  
  Document *doc = nullptr;
  VolatileSelection sel;
  
  String style_name;
  
  if(exprlen == 0) {
    doc = Documents::selected_document();
    if(!doc)
      return Symbol(richmath_System_DollarFailed);
    
    sel = doc->selection_now();
  }
  else {
    style_name = String(expr[exprlen]);
    if(exprlen == 2 && !style_name)
      return Symbol(richmath_System_DollarFailed);
    
    if(exprlen == 2 || !style_name) {
      auto id = FrontEndReference::from_pmath(expr[1]);
      sel.box = FrontEndObject::find_cast<Box>(id);
      if((doc = dynamic_cast<Document*>(sel.box))) {
        sel = doc->selection_now();
      }
      else
        doc = sel.box ? sel.box->find_parent<Document>(true) : nullptr;
    }
    else {
      doc = Documents::selected_document();
      if(!doc)
        return Symbol(richmath_System_DollarFailed);
      
      sel = doc->selection_now();
    }
  }
  
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
  
  if(!style_name) {
    style_name = DocumentsImpl::get_style_name_at(sel);
    if(!style_name)
      return Symbol(richmath_System_DollarFailed);
  }
  
  int index = sel.start;
  for(Box *tmp = sel.box; tmp && tmp != doc; tmp = tmp->parent()) {
    index = tmp->index();
  }
  
  if(auto result = DocumentsImpl::find_style_definition(doc, index, style_name))
    return result->to_pmath_id();
  
  if(!doc->stylesheet()->styles.search(style_name))
    return Symbol(richmath_System_DollarFailed);
  
  if(Document *style_doc = DocumentsImpl::open_private_style_definitions(doc, false)) {
    if(auto result = DocumentsImpl::find_style_definition(style_doc, style_doc->length(), style_name))
      return result->to_pmath_id();
  }
  
  if(auto result = DocumentsImpl::find_style_definition(doc->get_own_style(StyleDefinitions, {}), style_name))
    return result->to_pmath_id();
  
  return Symbol(richmath_System_DollarFailed);
}

Expr richmath_eval_FrontEnd_KeyboardInputBox(Expr expr) {
  if(auto box = Documents::keyboard_input_box())
    return box->to_pmath_id();
  
  return Symbol(richmath_System_None);
}

Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr) {
  auto doc = Documents::selected_document();
  if(doc)
    return doc->to_pmath_id();
  
  return Symbol(richmath_System_None);
}

Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr) {
  /*  FrontEnd`SetSelectedDocument(doc)
      FrontEnd`SetSelectedDocument(doc,       selectionOrBox)
      FrontEnd`SetSelectedDocument(Automatic, selectionOrBox)
   */
  size_t exprlen = expr.expr_length();
  if(exprlen < 1 || exprlen > 2)
    return Symbol(richmath_System_DollarFailed);
  
  auto docid = FrontEndReference::from_pmath(expr[1]);
  Box *docbox = FrontEndObject::find_cast<Box>(docid);
  Document *doc = docbox ? docbox->find_parent<Document>(true) : nullptr;
  
  if(exprlen == 2) {
    Expr sel_expr = expr[2];
    SelectionReference sel = SelectionReference::from_pmath(expr[2]);
    Box *selbox = sel.get();
    if(!selbox) {
      sel.id = FrontEndReference::from_pmath(expr[2]);
      if((selbox = sel.get())) {
        sel.start = 0;
        sel.end = selbox->length();
      }
    }
  
    if(selbox) {
      if(Document *seldoc = selbox->find_parent<Document>(true)) {
        if(expr[1] == richmath_System_Automatic) 
          doc = seldoc;
        
        if(seldoc == doc) 
          doc->select(selbox, sel.start, sel.end);
      }
    }
  }
  
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
    
  //Documents::selected_document(doc);
  doc->native()->bring_to_front();
  
  return doc->to_pmath_id();
}
