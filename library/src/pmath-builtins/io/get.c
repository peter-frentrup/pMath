#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>

#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/scanner.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>
#include <pmath-builtins/language-private.h>
#include <pmath-builtins/lists-private.h>

static pmath_bool_t check_path(pmath_t path){
  size_t i;
  
  if(!pmath_is_expr_of(path, PMATH_SYMBOL_LIST)){
    pmath_message(NULL, "path", 1, pmath_ref(path));
    return FALSE;
  }
  
  for(i = 1;i <= pmath_expr_length(path);++i){
    pmath_t item = pmath_expr_get_item(path, i);
    
    if(!pmath_instance_of(item, PMATH_TYPE_STRING)){
      pmath_message(NULL, "path", 1, item);
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
    return NULL;
  
  line = pmath_evaluate(
    pmath_parse_string_args("Read(`1`, String)",
      "(o)", pmath_ref(info->file)));
  
  if(pmath_equals(line, PMATH_SYMBOL_ENDOFFILE)){
    pmath_unref(line);
    return NULL;
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
  pmath_t        file, // will be freed and closed
  pmath_string_t name  // will be freed
){
  struct _get_file_info  info;
  
  pmath_span_array_t *spans;
  pmath_t old_input;
  pmath_string_t code;
  pmath_t result = NULL;
  
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
    result = NULL;
    
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
          pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
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
  return result;
}

PMATH_PRIVATE pmath_t builtin_get(pmath_expr_t expr){
  pmath_t options, path, name, file;
  
  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(name, PMATH_TYPE_STRING)){
    pmath_unref(name);
    pmath_message(NULL, "str", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(!options)
    return expr;
  
  path = pmath_evaluate(pmath_option_value(NULL, PMATH_SYMBOL_PATH, options));
  pmath_unref(options); 
  if(pmath_instance_of(path, PMATH_TYPE_STRING))
    path = pmath_build_value("(o)", path);
  
  if(!check_path(path))
    return expr;
  pmath_unref(expr);
  
  
  if(_pmath_is_namespace(name)){
    struct _pmath_string_t *fname;
    uint16_t *buf;
    size_t i;
    int j, len;
    
    len = pmath_string_length(name) - 1;
    fname = _pmath_new_string_buffer(len);
    if(!fname){
      pmath_unref(name);
      pmath_unref(path);
      return NULL;
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
          pmath_ref((pmath_string_t)fname)));
      
      test = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
          pmath_ref(testname)));
      pmath_unref(test);
      if(test == PMATH_SYMBOL_DIRECTORY){
        testname = pmath_evaluate(
          pmath_parse_string_args(
              "ToFileName(testname, \"init.pmath\")", 
            "(o)", 
            testname));
        
        test = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
            pmath_ref(testname)));
        pmath_unref(test);
        
        if(test == PMATH_SYMBOL_FILE){
          pmath_unref((pmath_string_t)fname);
          pmath_unref(name);
          pmath_unref(path);
          
          file = pmath_evaluate(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
              pmath_ref(testname)));
          if(file != PMATH_SYMBOL_FAILED)
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
          pmath_ref((pmath_string_t)fname)));
        
      test = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
          pmath_ref(testname)));
      pmath_unref(test);
      if(test == PMATH_SYMBOL_FILE){
        pmath_unref((pmath_string_t)fname);
        pmath_unref(name);
        pmath_unref(path);
        
        file = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
            pmath_ref(testname)));
        if(file != PMATH_SYMBOL_FAILED)
          return get_file(file, testname);
        
        pmath_unref(testname);
        pmath_unref(file);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      pmath_unref(testname);
    }
    
    pmath_unref((pmath_string_t)fname);
    pmath_message(NULL, "noopen", 1, name);
    pmath_unref(path);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(path);
  
  file = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
      pmath_ref(name)));
  pmath_unref(file);
  if(file != PMATH_SYMBOL_FILE){
    pmath_message(NULL, "noopen", 1, name);
    return pmath_ref(PMATH_SYMBOL_FAILED);;
  }
  
  file = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_OPENREAD), 1,
      pmath_ref(name)));
  if(file != PMATH_SYMBOL_FAILED)
    return get_file(file, name);
  
  pmath_unref(file);
  pmath_unref(name);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
