#include <pmath-core/strings-private.h>
#include <pmath-core/numbers.h>

#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/language-private.h>
#include <pmath-builtins/io-private.h>

#include <string.h>

static pmath_bool_t check_path(pmath_t path){
  size_t i;
  
  if(!pmath_is_expr_of(path, PMATH_SYMBOL_LIST)){
    pmath_message(PMATH_NULL, "path", 1, pmath_ref(path));
    return FALSE;
  }
  
  for(i = 1;i <= pmath_expr_length(path);++i){
    pmath_t item = pmath_expr_get_item(path, i);
    
    if(!pmath_is_string(item)){
      pmath_message(PMATH_NULL, "path", 1, item);
      return FALSE;
    }
    
    pmath_unref(item);
  }
  
  return TRUE;
}

struct _get_file_info{
  pmath_t ns;
  pmath_t nspath;
  pmath_t file;
  pmath_string_t name;
  int startline;
  int codelines;
  pmath_bool_t err;
};

static pmath_string_t scanner_read(void *data){
  struct _get_file_info *info = (struct _get_file_info*)data;
  pmath_string_t line;
  
  if(pmath_aborting())
    return PMATH_NULL;
  
  line = pmath_evaluate(
    pmath_parse_string_args("Read(`1`, String)",
      "(o)", pmath_ref(info->file)));
  
  if(!pmath_is_string(line)){
    pmath_unref(line);
    return PMATH_NULL;
  }
  
  info->codelines++;
  return line;
}

static void scanner_error(
  pmath_string_t code, 
  int pos, 
  void *data,
  pmath_bool_t critical
){
  struct _get_file_info *info = (struct _get_file_info*)data;
  if(critical)
    info->err = TRUE;
  pmath_message_syntax_error(code, pos, pmath_ref(info->name), info->startline);
}

static pmath_t get_file(
  pmath_t        file,          // will be freed and closed
  pmath_string_t name           // will be freed
){
  struct _get_file_info  info;
  
  pmath_span_array_t *spans;
  pmath_t old_input;
  pmath_string_t code;
  pmath_t result = PMATH_NULL;
  
  pmath_message(PMATH_SYMBOL_GET, "load", 1, pmath_ref(name));
  
  old_input = pmath_evaluate(pmath_ref(PMATH_SYMBOL_INPUT));
  pmath_unref(pmath_thread_local_save(
    PMATH_SYMBOL_INPUT, 
    _pmath_canonical_file_name(pmath_ref(name))));
  
  info.ns     = pmath_evaluate(pmath_ref(PMATH_SYMBOL_CURRENTNAMESPACE));
  info.nspath = pmath_evaluate(pmath_ref(PMATH_SYMBOL_NAMESPACEPATH));
  info.file = file;
  info.name = name;
  info.startline = 1;
  info.codelines = 0;
  info.err = FALSE;
  
  do{
    pmath_unref(result);
    result = PMATH_NULL;
    
    code = scanner_read(&info);
    spans = pmath_spans_from_string(
      &code, 
      scanner_read, 
      NULL, 
      NULL, 
      scanner_error,
      &info);
    
    if(!info.err){
      result = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
          pmath_boxes_from_spans(
            spans, 
            code, 
            TRUE, 
            NULL, 
            NULL)));
      
      if(pmath_is_expr_of(result, PMATH_SYMBOL_HOLDCOMPLETE)){
        if(pmath_expr_length(result) == 1){
          pmath_t tmp = result;
          result = pmath_expr_get_item(tmp, 1);
          pmath_unref(tmp);
        }
        else
          result = pmath_expr_set_item(
            result, 0, 
            pmath_ref(PMATH_SYMBOL_SEQUENCE));
      }
      
      result = pmath_evaluate(result);
    }
    
    pmath_unref(code);
    pmath_span_array_free(spans);
    
    info.startline+= info.codelines;
    info.codelines = 0;
  }while(!pmath_aborting() && pmath_file_status(file) == PMATH_FILE_OK);

  if(info.err){
    pmath_symbol_set_value(PMATH_SYMBOL_CURRENTNAMESPACE, info.ns);
    pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACEPATH,    info.nspath);
  }
  else{
    pmath_unref(info.ns);
    pmath_unref(info.nspath);
  }
  
  pmath_unref(pmath_thread_local_save(PMATH_SYMBOL_INPUT, old_input));
  pmath_file_close(file);
  pmath_unref(name);
  
//  if(package_check){
//    name = pmath_evaluate(
//      pmath_parse_string_args(
//          "IsFreeOf($Packages, `1`)", 
//        "(o)", 
//        pmath_ref(package_check))); 
//    pmath_unref(name);
//    
//    if(pmath_same(name, PMATH_SYMBOL_TRUE)){
//      pmath_message(PMATH_NULL, "nons", 1, pmath_ref(package_check));
//    }
//    
//    pmath_unref(package_check);
//  }
  
  return result;
}

PMATH_PRIVATE pmath_t builtin_get(pmath_expr_t expr){
  pmath_t options, path, name, file;
  
  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)){
    pmath_unref(name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
  path = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_PATH, options));
  pmath_unref(options); 
  if(pmath_is_string(path))
    path = pmath_build_value("(o)", path);
  
  if(!check_path(path)){
    pmath_unref(path);
    return expr;
  }
  
  pmath_unref(expr); expr = PMATH_NULL;
  
  
  if(_pmath_is_namespace(name)){
    struct _pmath_string_t *fname;
    uint16_t *buf;
    size_t i;
    int j, len;
    
    expr = pmath_evaluate(
      pmath_parse_string_args(
          "IsFreeOf($Packages, `1`)", 
        "(o)", 
        pmath_ref(name)));
    pmath_unref(expr);
    if(!pmath_same(expr, PMATH_SYMBOL_TRUE)){
      pmath_unref(name);
      pmath_unref(path);
      return PMATH_NULL;
    }
    
    len = pmath_string_length(name) - 1;
    fname = _pmath_new_string_buffer(len);
    if(!fname){
      pmath_unref(name);
      pmath_unref(path);
      return PMATH_NULL;
    }
    buf = AFTER_STRING(fname);
    memcpy(buf, pmath_string_buffer(name), len * sizeof(uint16_t));
    
    for(j = 0;j < len;++j){
      if(buf[j] == '`'){
        #ifdef PMATH_OS_WIN32
          buf[j] = '\\';
        #else
          buf[j] = '/';
        #endif
      }
    }
    
    for(i = 1;i <= pmath_expr_length(path);++i){
      pmath_t testname, test;
      
      testname = pmath_evaluate(
        pmath_parse_string_args(
            "ToFileName(`1`, `2`)", 
          "(oo)", 
          pmath_expr_get_item(path, i),
          pmath_ref(PMATH_FROM_PTR(fname))));
      
      test = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
          pmath_ref(testname)));
      pmath_unref(test);
      if(pmath_same(test, PMATH_SYMBOL_DIRECTORY)){
        testname = pmath_evaluate(
          pmath_parse_string_args(
              "ToFileName(`1`, \"init.pmath\")", 
            "(o)", 
            testname));
        
        test = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
            pmath_ref(testname)));
        pmath_unref(test);
        
        if(pmath_same(test, PMATH_SYMBOL_FILE)){
          pmath_unref(PMATH_FROM_PTR(fname));
          pmath_unref(path);
          pmath_unref(name);
          
          file = pmath_evaluate(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
              pmath_ref(testname)));
          if(!pmath_same(file, PMATH_SYMBOL_FAILED))
            return get_file(file, testname);
          
          pmath_unref(testname);
          pmath_unref(file);
          return pmath_ref(PMATH_SYMBOL_FAILED);
        }
      }
      pmath_unref(testname);
      
      
      testname = pmath_evaluate(
        pmath_parse_string_args(
            "ToFileName(`1`, `2` ++ \".pmath\")", 
          "(oo)", 
          pmath_expr_get_item(path, i),
          pmath_ref(PMATH_FROM_PTR(fname))));
        
      test = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
          pmath_ref(testname)));
      pmath_unref(test);
      if(pmath_same(test, PMATH_SYMBOL_FILE)){
        pmath_unref(PMATH_FROM_PTR(fname));
        pmath_unref(path);
        pmath_unref(name);
        
        file = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
            pmath_ref(testname)));
        if(!pmath_same(file, PMATH_SYMBOL_FAILED))
          return get_file(file, testname);
        
        pmath_unref(testname);
        pmath_unref(file);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      pmath_unref(testname);
    }
    
    pmath_unref(PMATH_FROM_PTR(fname));
    pmath_message(PMATH_NULL, "noopen", 1, name);
    pmath_unref(path);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(path);
  
  file = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
      pmath_ref(name)));
  pmath_unref(file);
  if(!pmath_same(file, PMATH_SYMBOL_FILE)){
    pmath_message(PMATH_NULL, "noopen", 1, name);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  file = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
      pmath_ref(name)));
  if(!pmath_same(file, PMATH_SYMBOL_FAILED))
    return get_file(file, name);
  
  pmath_unref(file);
  pmath_unref(name);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
