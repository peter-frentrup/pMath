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

// deprecated, any thread:
pmath_t builtin_selecteddocument(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return current_document_id.to_pmath().release();
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
