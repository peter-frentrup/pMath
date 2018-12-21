#include <eval/binding.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace pmath;
using namespace richmath;

static FrontEndReference current_document_id = FrontEndReference::None;

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

Expr richmath_eval_FrontEnd_SetSelectedDocument(Expr expr) {
  auto id = FrontEndReference::from_pmath(expr[1]);
  Box *box = FrontEndObject::find_cast<Box>(id);
  if(!box)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  //set_current_document(doc);
  doc->native()->bring_to_front();
  //return doc->id().to_pmath();
  return current_document_id.to_pmath();
}
