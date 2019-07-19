#include <gui/recent-documents.h>


using namespace pmath;
using namespace richmath;



extern pmath_symbol_t richmath_FE_Item;
extern pmath_symbol_t richmath_FrontEnd_DocumentOpen;

Expr RecentDocuments::open_document_menu_item(Expr label, Expr path) {
  return Call(
           Symbol(richmath_FE_Item), 
           std::move(label), 
           Call(Symbol(richmath_FrontEnd_DocumentOpen), std::move(path)));
}


