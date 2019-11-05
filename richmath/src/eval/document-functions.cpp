#include <eval/binding.h>
#include <eval/observable.h>
#include <gui/common-document-windows.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/recent-documents.h>

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
      static Expr get_DocumentFileName(FrontEndObject *obj, Expr item);
      static Expr get_DocumentFullFileName(FrontEndObject *obj, Expr item);
 };
}

static ObservableValue<FrontEndReference> current_document_id { FrontEndReference::None };


extern pmath_symbol_t richmath_Documentation_FindSymbolDocumentationByFullName;

extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;

static MenuCommandStatus can_set_selected_document(Expr cmd);
static bool set_selected_document_cmd(Expr cmd);
static bool document_open_cmd(Expr cmd);
static bool open_selection_help_cmd(Expr cmd);

static Expr menu_list_windows_enum(Expr name);
static Expr menu_list_recent_documents_enum(Expr name);

static bool remove_recent_document(Expr submenu_cmd, Expr item_cmd);

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

Expr richmath_eval_FrontEnd_DocumentOpen(Expr expr) {
  /* FrontEnd`DocumentOpen(filename)
  */
  
  if(expr.expr_length() != 1)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  String filename{expr[1]};
  if(filename.is_null()) 
    return Symbol(PMATH_SYMBOL_FAILED);
  
  filename = Application::to_absolute_file_name(filename);
  if(filename.is_null()) 
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *doc = Application::find_open_document(filename);
  if(!doc) {
    doc = Application::open_new_document(filename);
    if(!doc)
      return Symbol(PMATH_SYMBOL_FAILED);
  }
  doc->native()->bring_to_front();
  return doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr) {
  auto doc = get_current_document();
  if(doc)
    return doc->to_pmath_id();
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

static Document *set_selected_document(FrontEndReference id) {
  Box *box = FrontEndObject::find_cast<Box>(id);
  if(!box)
    return nullptr;
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return nullptr;
  
  //set_current_document(doc);
  doc->native()->bring_to_front();
  
  return doc;
}

Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr) {
  auto id = FrontEndReference::from_pmath(expr[1]);
  Document *doc = set_selected_document(id);
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  return doc->to_pmath_id();
}

Expr richmath_eval_FrontEnd_Documents(Expr expr) {
  Gather gather;
  
  for(auto win : CommonDocumentWindow::All) {
    Gather::emit(win->content()->to_pmath_id());
  }
  
  return gather.end();
}

bool richmath::impl::init_document_functions() {
  Application::register_menucommand(String("OpenSelectionHelp"),                   open_selection_help_cmd);

  Application::register_menucommand(Symbol(richmath_FrontEnd_SetSelectedDocument), set_selected_document_cmd, can_set_selected_document);
  Application::register_menucommand(Symbol(richmath_FrontEnd_DocumentOpen),        document_open_cmd);
  
  Application::register_dynamic_submenu(     String("MenuListWindows"), menu_list_windows_enum);
  Application::register_dynamic_submenu(     String("MenuListRecentDocuments"), menu_list_recent_documents_enum);
  Application::register_submenu_item_deleter(String("MenuListRecentDocuments"), remove_recent_document);
  
  DocumentCurrentValueProvider::init();
  
  return true;
}

void richmath::impl::done_document_functions() {
  DocumentCurrentValueProvider::done();
}

static MenuCommandStatus can_set_selected_document(Expr cmd) {
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

static bool set_selected_document_cmd(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_SetSelectedDocument)
    return false;
  
  if(cmd.expr_length() != 1) 
    return false;
  
  FrontEndReference id = FrontEndReference::from_pmath(cmd[1]);
  
  return set_selected_document(id);
}

static bool document_open_cmd(Expr cmd) {
  if(cmd[0] != richmath_FrontEnd_DocumentOpen)
    return false;
  
  if(cmd.expr_length() != 1) 
    return false;
  
  Expr result = richmath_eval_FrontEnd_DocumentOpen(std::move(cmd));
  return result != PMATH_SYMBOL_FAILED;
}

static Expr menu_list_windows_enum(Expr name) {
  Gather g;
  int i = 1;
  for(auto win : CommonDocumentWindow::All) {
    g.emit(
      Call(
        Symbol(richmath_FE_Item), 
        win->title(), 
        //List(name, i)
        Call(Symbol(richmath_FrontEnd_SetSelectedDocument),
          win->content()->to_pmath_id())));
    ++i;
  }
  return g.end();
}

static Expr menu_list_recent_documents_enum(Expr name) {
  return RecentDocuments::as_menu_list();
}

static bool remove_recent_document(Expr submenu_cmd, Expr item_cmd) {
  if(item_cmd.expr_length() == 1 && item_cmd[0] == richmath_FrontEnd_DocumentOpen) {
    String path{ item_cmd[1] };
    if(path.length() > 0)
      return RecentDocuments::remove(std::move(path));
  }
  return false;
}

static bool open_selection_help_cmd(Expr cmd) {
  Document * const doc = get_current_document();
  if(!doc)
    return false;
    
  Box *box = doc->selection_box();
  int start = doc->selection_start();
  int end = doc->selection_end();
  int word_start = start;
  int word_end = word_start;
  Box *word_box = box;
  do {
    word_box = expand_selection(word_box, &word_start, &word_end);
  } while(word_box && !(word_start <= start && end <= word_end));
  
  if(!word_box) {
    doc->native()->beep();
    return false;
  }
  
  doc->select(word_box, word_start, word_end);
  
  // TODO: give context-dependent help
  
  if(auto seq = dynamic_cast<AbstractSequence *>(word_box)) {
    String word = seq->raw_substring(word_start, word_end - word_start);
    
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
  
  Application::register_currentvalue_provider(s_DocumentDirectory,    get_DocumentDirectory);
  Application::register_currentvalue_provider(s_DocumentFileName,     get_DocumentFileName);
  Application::register_currentvalue_provider(s_DocumentFullFileName, get_DocumentFullFileName);
  
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
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return Application::get_directory_path(std::move(result));
}

Expr DocumentCurrentValueProvider::get_DocumentFileName(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  Application::extract_directory_path(&result);
  return result;
}

Expr DocumentCurrentValueProvider::get_DocumentFullFileName(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return result;
}

//} ... class DocumentCurrentValueProvider
