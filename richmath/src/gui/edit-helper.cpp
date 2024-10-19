#include <gui/edit-helper.h>

#include <gui/menus.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_DocumentApply;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_List;

//{ class EditHelper ...

bool EditHelper::insert_new_table_into_current_document() {
  return insert_new_table_into_current_document(0, 0);
}

bool EditHelper::insert_new_table_into_current_document(int num_rows, int num_columns) {
  Document *doc = Menus::current_document();
  if(!doc)
    return false;
  
  size_t rows = num_rows    < 1 ? 1 : (size_t)num_rows;
  size_t cols = num_columns < 1 ? 1 : (size_t)num_columns;
  
  if(rows == 1 && cols == 1) {
    rows = 2;
    cols = 2;
  }
  
  Expr pl = String::FromChar(PMATH_CHAR_PLACEHOLDER);
  Expr row = MakeCall(Symbol(richmath_System_List), cols);
  for(size_t i = 1; i <= cols; ++i)
    row.set(i, pl);
  
  Expr matrix = MakeCall(Symbol(richmath_System_List), rows);
  for(size_t i = rows; i > 1; --i)
    matrix.set(i, row);
  
  //row.set(1, String::FromChar(PMATH_CHAR_SELECTIONPLACEHOLDER));
  matrix.set(1, row);
  
  Expr grid = Call(Symbol(richmath_System_GridBox), matrix);
  Expr new_cmd = Call(Symbol(richmath_System_DocumentApply), Symbol(richmath_System_Automatic), grid);
  
  //doc->paste_from_boxes(PMATH_CPP_MOVE(grid));
  return Menus::run_command_now(PMATH_CPP_MOVE(new_cmd));
}

//} ... class EditHelper
