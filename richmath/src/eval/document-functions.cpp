#include <eval/binding.h>
#include <eval/observable.h>

#include <gui/common-document-windows.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/recent-documents.h>

#include <util/filesystem.h>

using namespace pmath;
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

static ObservableValue<FrontEndReference> current_document_id { FrontEndReference::None };


extern pmath_symbol_t richmath_Documentation_FindSymbolDocumentationByFullName;
extern pmath_symbol_t richmath_FE_MenuItem;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;

static bool open_selection_help_cmd(Expr cmd);

Expr richmath_eval_FrontEnd_CreateDocument(Expr expr);
Expr richmath_eval_FrontEnd_DocumentGet(Expr expr);
Expr richmath_eval_FrontEnd_DocumentOpen(Expr expr);
Expr richmath_eval_FrontEnd_Documents(Expr expr);
Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr);
Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr);

void richmath::set_current_document(Document *document) {
  FrontEndReference id = document ? document->id() : FrontEndReference::None;
  if(current_document_id.unobserved_equals(id))
    return;
  
  if(auto old = get_current_document()) 
    old->focus_killed();
    
  if(document)
    document->focus_set();
    
  current_document_id = id;
}

Document *richmath::get_current_document() {
  return FrontEndObject::find_cast<Document>(current_document_id);
}

Expr richmath_eval_FrontEnd_CreateDocument(Expr expr) {
  /* FrontEnd`CreateDocument({sections...}, options...)
  */
  
  // TODO: respect window-related options (WindowTitle...)
  
  expr.set(0, Symbol(PMATH_SYMBOL_CREATEDOCUMENT));
  Document *doc = Application::create_document(expr);
  
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  if(!doc->selectable())
    doc->select(nullptr, 0, 0);
    
  doc->invalidate_options();
  
  return doc->to_pmath_id();
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
    docid = current_document_id;
  
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
  auto doc = get_current_document();
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
    
  //set_current_document(doc);
  doc->native()->bring_to_front();
  
  return doc->to_pmath_id();
}

bool richmath::impl::init_document_functions() {
  Application::register_menucommand(String("OpenSelectionHelp"),                   open_selection_help_cmd);

  OpenDocumentMenuImpl::init();
  SelectDocumentMenuImpl::init();
  DocumentCurrentValueProvider::init();
  
  return true;
}

void richmath::impl::done_document_functions() {
  DocumentCurrentValueProvider::done();
  SelectDocumentMenuImpl::done();
  OpenDocumentMenuImpl::done();
}

static bool open_selection_help_cmd(Expr cmd) {
  Document * const doc = get_current_document();
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
  return std::move(result);
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
  return std::move(result);
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
  return std::move(result);
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
  
  Document *doc = get_current_document();
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
