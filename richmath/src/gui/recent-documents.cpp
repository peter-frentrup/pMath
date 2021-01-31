#include <gui/recent-documents.h>


using namespace pmath;
using namespace richmath;



extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_System_True;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;

Expr RecentDocuments::open_document_menu_item(Expr label, Expr path) {
  return Call(
           Symbol(richmath_System_MenuItem), 
           std::move(label), 
           Call(
             Symbol(richmath_FrontEnd_DocumentOpen), 
             std::move(path)));
}

Expr RecentDocuments::open_document_menu_item(Expr label, Expr path, bool add_to_recent) {
  return Call(
           Symbol(richmath_System_MenuItem), 
           std::move(label), 
           Call(
             Symbol(richmath_FrontEnd_DocumentOpen), 
             std::move(path),
             add_to_recent ? Symbol(richmath_System_True) : Symbol(richmath_System_False)));
}
