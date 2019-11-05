#include <util/filesystem.h>


using namespace richmath;

//{ class FileSystem ...


String FileSystem::to_absolute_file_name(String filename) {
  if(filename.is_null())
    return String();
  
  Expr info = Evaluate(Call(Symbol(PMATH_SYMBOL_FILEINFORMATION), filename));
  if(info[0] == PMATH_SYMBOL_LIST) {
    size_t len = info.expr_length();
    for(size_t i = 1; i <= len; ++i) {
      Expr rule = info[i];
      if(rule.is_rule() && rule[1] == PMATH_SYMBOL_FILE) 
        return rule[2];
    }
  }
  
  return String();
}

String FileSystem::extract_directory_path(String *filename) {
  assert(filename != nullptr);
  
  if(filename->is_null())
    return String();
  
  int             i   = filename->length() - 1;
  const uint16_t *buf = filename->buffer();
  while(i > 0 && buf[i] != '\\' && buf[i] != '/')
    --i;
    
  if(i > 0) {
    String dir = filename->part(0, i);
    *filename = filename->part(i + 1);
    return dir;
  }
  else
    return String("");
}


//} ... class FileSystem
