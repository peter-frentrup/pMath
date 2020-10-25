#include <gui/documents.h>

#include <gui/common-document-windows.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/recent-documents.h>

#include <eval/application.h>

#include <util/filesystem.h>

#include <algorithm>


namespace richmath {
  struct DocumentsImpl {
    static bool show_hide_menu_cmd(Expr cmd);
    static MenuCommandStatus can_show_hide_menu(Expr cmd);

    static bool open_selection_help_cmd(Expr cmd);

    static void collect_selections(Array<SelectionReference> &sels, Expr expr);
  };
}

using namespace richmath;

namespace {
  class DocumentCurrentValueProvider {
    public:
      static void init();
      static void done();
      
    private:
      static String s_DocumentDirectory;
      static String s_DocumentFileName;
      static String s_DocumentFullFileName;
      
      static Expr get_DocumentDirectory(FrontEndObject *obj, Expr item);
      static bool put_DocumentDirectory(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_DocumentFileName(FrontEndObject *obj, Expr item);
      static bool put_DocumentFileName(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_DocumentFullFileName(FrontEndObject *obj, Expr item);
      static bool put_DocumentFullFileName(FrontEndObject *obj, Expr item, Expr rhs);
  };
  
  class StylesMenuImpl : public FrontEndObject {
    public:
      static void init();
      static void done();
    
    private:
      static MenuCommandStatus can_set_style(Expr cmd);
      static bool set_style(Expr cmd);
      
      static Expr enum_styles_menu(Expr name);
      
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
Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr);
Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr);
Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr);

extern pmath_symbol_t richmath_Documentation_FindSymbolDocumentationByFullName;
extern pmath_symbol_t richmath_FE_MenuItem;
extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;

extern pmath_symbol_t richmath_System_BaseStyle;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;

//{ class Documents ...

bool Documents::init() {
  Application::register_menucommand(String("ShowHideMenu"),      Impl::show_hide_menu_cmd, Impl::can_show_hide_menu);
  Application::register_menucommand(String("OpenSelectionHelp"), Impl::open_selection_help_cmd);

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

ObservableValue<FrontEndReference> Documents::current_document_id { FrontEndReference::None };

Document *Documents::current() {
  return FrontEndObject::find_cast<Document>(current_document_id);
}

void Documents::current(Document *document) {
  FrontEndReference id = document ? document->id() : FrontEndReference::None;
  if(Documents::current_document_id.unobserved_equals(id))
    return;
  
  if(auto old = Documents::current()) 
    old->focus_killed();
    
  if(document)
    document->focus_set();
    
  current_document_id = id;
}

Expr Documents::make_section_boxes(Expr boxes, Document *doc) {
  if(boxes[0] == richmath_System_Section)
    return boxes;
    
  if(boxes[0] == richmath_System_SectionGroup)
    return boxes;
  
  if(boxes[0] != richmath_System_BoxData) {
    boxes = Application::interrupt_wait(Call(Symbol(PMATH_SYMBOL_TOBOXES), std::move(boxes)));
    boxes = Call(Symbol(richmath_System_BoxData), std::move(boxes));
  }
  
  return Call(Symbol(richmath_System_Section), 
              std::move(boxes), 
              doc ? doc->get_own_style(DefaultNewSectionStyle, String("Input")) : String("Input"));
}

//} ... class Documents

//{ class DocumentsImpl ...

bool DocumentsImpl::show_hide_menu_cmd(Expr cmd) {
  Document * const doc = Documents::current();
  if(!doc)
    return false;
  
  if(!doc->native()->can_toggle_menubar())
    return false;
  return doc->native()->try_set_menubar(!doc->native()->has_menubar());
}

MenuCommandStatus DocumentsImpl::can_show_hide_menu(Expr cmd) {
  Document * const doc = Documents::current();
  if(!doc)
    return false;
  
  MenuCommandStatus status{ doc->native()->can_toggle_menubar() };
  status.checked = doc->native()->has_menubar();
  return status;
}

bool DocumentsImpl::open_selection_help_cmd(Expr cmd) {
  Document * const doc = Documents::current();
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
    helpfile = Call(Symbol(PMATH_SYMBOL_TIMECONSTRAINED), std::move(helpfile), Application::button_timeout);
    helpfile = Evaluate(std::move(helpfile));
    
    if(helpfile.is_string()) {
      Document *helpdoc = Application::find_open_document(helpfile);
      if(!helpdoc) 
        helpdoc = Application::open_new_document(helpfile);
      
      if(helpdoc) {
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
//      call = Call(Symbol(PMATH_SYMBOL_TIMECONSTRAINED), std::move(call), Application::button_timeout);
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

void DocumentsImpl::collect_selections(Array<SelectionReference> &sels, Expr expr) {
  if(expr[0] == PMATH_SYMBOL_LIST) {
    for(auto item : expr.items())
      collect_selections(sels, std::move(item));
    return;
  }
  
  if(auto sel = SelectionReference::from_debug_info(expr)) {
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
      if(auto doc = box->find_parent<Document>(false)) {
        sels.add(SelectionReference(box->parent(), box->index(), box->index() + 1));
      }
      return;
    }
    
    return;
  }
}

//} ... class DocumentsImpl

//{ class DocumentCurrentValueProvider ...

String DocumentCurrentValueProvider::s_DocumentDirectory;
String DocumentCurrentValueProvider::s_DocumentFileName;
String DocumentCurrentValueProvider::s_DocumentFullFileName;

void DocumentCurrentValueProvider::init() {
  s_DocumentDirectory    = String("DocumentDirectory");
  s_DocumentFileName     = String("DocumentFileName");
  s_DocumentFullFileName = String("DocumentFullFileName");
  
  Application::register_currentvalue_provider(s_DocumentDirectory,    get_DocumentDirectory,    put_DocumentDirectory);
  Application::register_currentvalue_provider(s_DocumentFileName,     get_DocumentFileName,     put_DocumentFileName);
  Application::register_currentvalue_provider(s_DocumentFullFileName, get_DocumentFullFileName, put_DocumentFullFileName);
  
}

void DocumentCurrentValueProvider::done() {
  s_DocumentDirectory    = String {};
  s_DocumentFileName     = String {};
  s_DocumentFullFileName = String {};
}

Expr DocumentCurrentValueProvider::get_DocumentDirectory(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  String result = doc->native()->directory();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentDirectory(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == PMATH_SYMBOL_NONE) {
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
    return Symbol(PMATH_SYMBOL_FAILED);
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentFileName(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == PMATH_SYMBOL_NONE) {
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
    return Symbol(PMATH_SYMBOL_FAILED);
    
  String result = doc->native()->full_filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return std::move(result); // Not literally the return type Expr, hence std::move.
}

bool DocumentCurrentValueProvider::put_DocumentFullFileName(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
    
  if(rhs == PMATH_SYMBOL_NONE) {
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

//} ... class DocumentCurrentValueProvider

//{ class StylesMenuImpl ...

StylesMenuImpl StylesMenuImpl::cache;

StylesMenuImpl::StylesMenuImpl() 
  : FrontEndObject(),
    latest_stylesheet {nullptr}
{
}

void StylesMenuImpl::init() {
  Application::register_menucommand(String("SelectStyle1"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle2"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle3"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle4"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle5"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle6"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle7"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle8"), set_style, can_set_style);
  Application::register_menucommand(String("SelectStyle9"), set_style, can_set_style);

  Application::register_dynamic_submenu(String("MenuListStyles"), enum_styles_menu);
}

void StylesMenuImpl::done() {
  cache.clear_cache();
}

MenuCommandStatus StylesMenuImpl::can_set_style(Expr cmd) {
  Document * const doc = Documents::current();
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
  Document * const doc = Documents::current();
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
  
  Expr commands = MakeList((size_t)styles.length());
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
    commands.set(i + 1, Call(Symbol(richmath_FE_MenuItem), item.name, cmd));
  }
  
  return commands;
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
  if(Document *doc = Documents::current())
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
  Application::register_menucommand(Symbol(richmath_FrontEnd_SetSelectedDocument), set_selected_document_cmd, can_set_selected_document);

  String s_MenuListWindows {"MenuListWindows"};
  Application::register_dynamic_submenu(               s_MenuListWindows,  enum_windows_menu);
  Application::register_submenu_item_deleter(std::move(s_MenuListWindows), remove_window);
}

void SelectDocumentMenuImpl::done() {
}

MenuCommandStatus SelectDocumentMenuImpl::can_set_selected_document(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_SetSelectedDocument)
    return MenuCommandStatus{ false };
  
  if(cmd.expr_length() != 1) 
    return MenuCommandStatus{ false };
  
  Document *doc = Documents::current();
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
  if(cmd == PMATH_SYMBOL_FAILED)
    return false;
  
  return true;
}

Expr SelectDocumentMenuImpl::enum_windows_menu(Expr name) {
  Gather g;
  int i = 1;
  for(auto win : CommonDocumentWindow::All) {
    g.emit(
      Call(
        Symbol(richmath_FE_MenuItem), 
        win->title(), 
        //List(name, i)
        Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
          win->content()->to_pmath_id())));
    ++i;
  }
  return g.end();
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
  Application::register_menucommand(Symbol(richmath_FrontEnd_DocumentOpen), document_open_cmd);
  
  Application::register_dynamic_submenu(String("MenuListPalettesMenu"), enum_palettes_menu);
  
  String s_MenuListRecentDocuments {"MenuListRecentDocuments"};
  Application::register_dynamic_submenu(               s_MenuListRecentDocuments,  enum_recent_documents_menu);
  Application::register_submenu_item_deleter(std::move(s_MenuListRecentDocuments), remove_recent_document);
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
  return result != PMATH_SYMBOL_FAILED;
}

Expr OpenDocumentMenuImpl::enum_palettes_menu(Expr name) {
  Expr list = Application::interrupt_wait_cached(
                Call(
                  Symbol(PMATH_SYMBOL_FILENAMES), 
                  Application::palette_search_path, 
                  String("*.pmathdoc")),
                 Application::dynamic_timeout);
  
  if(list[0] == PMATH_SYMBOL_LIST) {
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

bool OpenDocumentMenuImpl::remove_recent_document(Expr submenu_cmd, Expr item_cmd) {
  if(item_cmd.expr_length() == 1 && item_cmd[0] == richmath_FrontEnd_DocumentOpen) {
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
    return Symbol(PMATH_SYMBOL_FAILED);
    
  SelectionReference sel;
  {
    if(sel.id = FrontEndReference::from_pmath(expr[1])) {
      if(Box *box = sel.get()) {
        sel.start = 0;
        sel.end = box->length();
      }
    }
    
    // TODO: allow selections ...
    
//    Array<SelectionReference> sels;
//    collect_selections(sels, expr[1]);
//    if(sels.length() != 1)
//      return Symbol(PMATH_SYMBOL_FAILED);
//    
//    sel = std::move(sels[0]);
  }
  
  Box *anchor_box = sel.get();
  if(!anchor_box)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *owner_doc = anchor_box->find_parent<Document>(true);
  if(!owner_doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *popup_doc = owner_doc->native()->try_create_popup_window(sel);
  if(!popup_doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  // FIXME: this is duplicated in Application::try_create_document():
  Expr options(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_expr())
    popup_doc->style->add_pmath(options);
    
  Expr sections = expr[3];
  if(sections[0] != PMATH_SYMBOL_LIST)
    sections = List(sections);
    
  for(auto item : sections.items()) {
    int pos = popup_doc->length();
    popup_doc->insert_pmath(&pos, Documents::make_section_boxes(std::move(item), popup_doc));
  }
  
  owner_doc->attach_popup_window(sel, popup_doc);
  popup_doc->invalidate_options();
  return popup_doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_CreateDocument(Expr expr) {
  /* FrontEnd`CreateDocument({sections...}, options...)
  */
  
  // TODO: respect window-related options (WindowTitle...)
  
  expr.set(0, Symbol(PMATH_SYMBOL_CREATEDOCUMENT));
  Document *doc = Application::try_create_document(expr);
  
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  if(!doc->selectable())
    doc->select(nullptr, 0, 0);
    
  doc->invalidate_options();
  
  return doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_DocumentClose(Expr expr) {
  /*  FrontEnd`DocumentClose()
      FrontEnd`DocumentClose(doc)
   */
  
  size_t exprlen = expr.expr_length();
  if(exprlen > 1)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  FrontEndReference docid = FrontEndReference::None;
  if(exprlen == 1) 
    docid = FrontEndReference::from_pmath(expr[1]);
  else
    docid = Documents::current_document_id;
  
  Document *doc = FrontEndObject::find_cast<Document>(docid);
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
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
    return Symbol(PMATH_SYMBOL_FAILED);
    
  AutoMemorySuspension mem_suspend;
  
  Array<SelectionReference> sels;
  
  if(exprlen == 0) {
    if(auto doc = Documents::current())
      sels.add(doc->selection());
  }
  else {
    DocumentsImpl::collect_selections(sels, expr[1]);
  }
  
  if(sels.length() == 0 && expr[1][0] != PMATH_SYMBOL_LIST)
    return Symbol(PMATH_SYMBOL_FAILED);
  
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
    return Symbol(PMATH_SYMBOL_FAILED);
  
  FrontEndReference docid;
  if(exprlen == 1) {
    docid = FrontEndReference::from_pmath(expr[1]);
    
    if(docid == FrontEndReference::None) {
      if(VolatileSelection sel = SelectionReference::from_debug_info(expr[1]).get_all()) {
        return sel.to_pmath(BoxOutputFlags::WithDebugInfo);
      }
    }
  }
  else
    docid = Documents::current_document_id;
  
  Box *box = FrontEndObject::find_cast<Box>(docid);
  if(!box)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  return box->to_pmath(BoxOutputFlags::WithDebugInfo);
}

Expr richmath_eval_FrontEnd_DocumentOpen(Expr expr) {
  /* FrontEnd`DocumentOpen(filename)
     FrontEnd`DocumentOpen(filename, addtorecent)
  */
  size_t exprlen = expr.expr_length();
  if(exprlen < 1 || exprlen > 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  String filename{expr[1]};
  if(filename.is_null()) 
    return Symbol(PMATH_SYMBOL_FAILED);
  
  bool add_to_recent_documents = true;
  if(exprlen == 2) {
    Expr obj = expr[2];
    if(obj == PMATH_SYMBOL_TRUE)
      add_to_recent_documents = true;
    else if(obj == PMATH_SYMBOL_FALSE)
      add_to_recent_documents = false;
    else
      return Symbol(PMATH_SYMBOL_FAILED);
  }
  
  filename = FileSystem::to_existing_absolute_file_name(filename);
  if(filename.is_null()) 
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *doc = Application::find_open_document(filename);
  if(!doc) {
    doc = Application::open_new_document(filename);
    if(!doc)
      return Symbol(PMATH_SYMBOL_FAILED);
    
    if(add_to_recent_documents)
      RecentDocuments::add(filename);
  }
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

Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr) {
  auto doc = Documents::current();
  if(doc)
    return doc->to_pmath_id();
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr) {
  /*  FrontEnd`SetSelectedDocument(doc)
      FrontEnd`SetSelectedDocument(doc,       selectionOrBox)
      FrontEnd`SetSelectedDocument(Automatic, selectionOrBox)
   */
  size_t exprlen = expr.expr_length();
  if(exprlen < 1 || exprlen > 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  auto docid = FrontEndReference::from_pmath(expr[1]);
  Box *docbox = FrontEndObject::find_cast<Box>(docid);
  Document *doc = docbox ? docbox->find_parent<Document>(true) : nullptr;
  
  if(exprlen == 2) {
    Expr sel_expr = expr[2];
    SelectionReference sel = SelectionReference::from_debug_info(expr[2]);
    Box *selbox = sel.get();
    if(!selbox) {
      sel.id = FrontEndReference::from_pmath(expr[2]);
      if(selbox = sel.get()) {
        sel.start = 0;
        sel.end = selbox->length();
      }
    }
  
    if(selbox) {
      if(Document *seldoc = selbox->find_parent<Document>(true)) {
        if(expr[1] == PMATH_SYMBOL_AUTOMATIC) 
          doc = seldoc;
        
        if(seldoc == doc) 
          doc->select(selbox, sel.start, sel.end);
      }
    }
  }
  
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  //Documents::current(doc);
  doc->native()->bring_to_front();
  
  return doc->to_pmath_id();
}
