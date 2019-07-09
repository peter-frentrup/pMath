#include <eval/binding.h>
#include <gui/common-document-windows.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace pmath;
using namespace richmath;

static FrontEndReference current_document_id = FrontEndReference::None;

extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

static MenuCommandStatus can_set_selected_document(Expr cmd);
static bool set_selected_document_cmd(Expr cmd);
static Expr menu_list_windows_enum(Expr name);


void richmath::set_current_document(Document *document) {
  if(auto old = get_current_document())
    old->focus_killed();
    
  if(document)
    document->focus_set();
    
  current_document_id = document ? document->id() : FrontEndReference::None;
}

Document *richmath::get_current_document() {
  return dynamic_cast<Document *>(Box::find(current_document_id));
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
  
  return doc->id().to_pmath();
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
    
    doc->invalidate_options();
  }
  doc->native()->bring_to_front();
  return doc->id().to_pmath();
}

Expr richmath_eval_FrontEnd_SelectedDocument(Expr expr) {
  return current_document_id.to_pmath();
}

static bool set_selected_document(FrontEndReference id) {
  Box *box = FrontEndObject::find_cast<Box>(id);
  if(!box)
    return false;
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return false;
  
  //set_current_document(doc);
  doc->native()->bring_to_front();
  //return doc->id().to_pmath();
  return true;
}

Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr) {
  auto id = FrontEndReference::from_pmath(expr[1]);
  if(!set_selected_document(id))
    return Symbol(PMATH_SYMBOL_FAILED);
  
  return current_document_id.to_pmath();
}

bool richmath::impl::init_document_functions() {
  Application::register_menucommand(Symbol(richmath_FrontEnd_SetSelectedDocument), set_selected_document_cmd, can_set_selected_document);
  
  Application::register_dynamic_submenu(String("MenuListWindows"), menu_list_windows_enum);

  return true;
}

void richmath::impl::done_document_functions() {
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
          win->content()->id().to_pmath())));
    ++i;
  }
  return g.end();
}
