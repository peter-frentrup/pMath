#include <pmath-core/strings.h>
#include <pmath-core/numbers.h>

#include <pmath-language/scanner.h>
#include <pmath-language/source-location.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files/abstract-file.h>
#include <pmath-util/files/filesystem.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/language-private.h>
#include <pmath-builtins/io-private.h>

#include <string.h>


extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_DollarInput;
extern pmath_symbol_t pmath_System_DollarNamespace;
extern pmath_symbol_t pmath_System_DollarNamespacePath;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_CharacterEncoding;
extern pmath_symbol_t pmath_System_Directory;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_File;
extern pmath_symbol_t pmath_System_FileType;
extern pmath_symbol_t pmath_System_Get;
extern pmath_symbol_t pmath_System_Head;
extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_Identity;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_OpenRead;
extern pmath_symbol_t pmath_System_ParseSymbols;
extern pmath_symbol_t pmath_System_Path;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_TrackSourceLocations;
extern pmath_symbol_t pmath_System_True;

static pmath_bool_t check_path(pmath_t path) {
  size_t i;
  
  if(!pmath_is_expr_of(path, pmath_System_List)) {
    pmath_message(PMATH_NULL, "path", 1, pmath_ref(path));
    return FALSE;
  }
  
  for(i = 1; i <= pmath_expr_length(path); ++i) {
    pmath_t item = pmath_expr_get_item(path, i);
    
    if(!pmath_is_string(item)) {
      pmath_message(PMATH_NULL, "path", 1, item);
      return FALSE;
    }
    
    pmath_unref(item);
  }
  
  return TRUE;
}

struct _get_file_info {
  pmath_t        ns;
  pmath_t        nspath;
  pmath_t        file;
  pmath_string_t filename;
  int            startline;
  int            codelines;
  pmath_t        current_code;
  pmath_bool_t   err;
};

static pmath_string_t scanner_read(void *data) {
  struct _get_file_info *info = (struct _get_file_info *)data;
  pmath_string_t line;
  
  if(pmath_aborting())
    return PMATH_NULL;
    
  line = pmath_evaluate(
           pmath_parse_string_args("Read(`1`, String)",
                                   "(o)", pmath_ref(info->file)));
                                   
  if(!pmath_is_string(line)) {
    pmath_unref(line);
    return PMATH_NULL;
  }
  
  info->codelines++;
  return line;
}

static void scanner_error(
  pmath_string_t  code,
  int             pos,
  void           *data,
  pmath_bool_t    critical
) {
  struct _get_file_info *info = data;
  
  if(!info->err)
    pmath_message_syntax_error(code, pos, pmath_ref(info->filename), info->startline);
    
  if(critical)
    info->err = TRUE;
}

static pmath_t add_debug_metadata(
  pmath_t                             token_or_span, 
  const struct pmath_text_position_t *start, 
  const struct pmath_text_position_t *end, 
  void                               *_data
) {
  pmath_t debug_metadata;
  struct _get_file_info *data = _data;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  assert(end->index <= pmath_string_length(data->current_code));
  
  if(!pmath_is_expr(token_or_span) && !pmath_is_string(token_or_span))
    return token_or_span;
    
  int startline = start->line + data->startline;
  int startcol  = start->index - start->line_start_index;
  
  int endline   = end->line + data->startline;
  int endcol    = end->index - end->line_start_index;
  
  debug_metadata = pmath_language_new_file_location(
                     pmath_ref(data->filename), 
                     startline, startcol, 
                     endline, endcol);
  
  return pmath_try_set_debug_metadata(token_or_span, debug_metadata);
}

static pmath_t get_file(
  pmath_t        file,              // will be freed and closed
  pmath_string_t name,              // will be freed
  pmath_t        head,              // will be freed
  pmath_bool_t   track_source_locations,
  pmath_t        may_parse_symbols  // will be freed
) {
  struct _get_file_info              info;
  struct pmath_boxes_from_spans_ex_t parse_settings;
  pmath_thread_t                     me = pmath_thread_get_current();
  
  pmath_span_array_t *spans;
  pmath_hashtable_t old_parser_cache            = me ? me->parser_cache : NULL;
  pmath_t old_input;
  pmath_t result = PMATH_NULL;
  pmath_bool_t changed_allow_parse_syms = FALSE;
  
  if(!pmath_same(may_parse_symbols, pmath_System_Automatic)) {
    changed_allow_parse_syms = TRUE;
    may_parse_symbols = pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, may_parse_symbols);
  }
  
  pmath_message(pmath_System_Get, "load", 1, pmath_ref(name));
  
  info.ns           = pmath_evaluate(pmath_ref(pmath_System_DollarNamespace));
  info.nspath       = pmath_evaluate(pmath_ref(pmath_System_DollarNamespacePath));
  info.file         = file;
  info.filename     = pmath_to_absolute_file_name(pmath_ref(name));
  info.startline    = 1;
  info.codelines    = 0;
  info.current_code = PMATH_NULL;
  info.err          = FALSE;
  
  old_input = pmath_evaluate(pmath_ref(pmath_System_DollarInput));
  pmath_unref(pmath_thread_local_save(
                pmath_System_DollarInput,
                pmath_ref(info.filename)));
                
  memset(&parse_settings, 0, sizeof(parse_settings));
  parse_settings.size           = sizeof(parse_settings);
  parse_settings.flags          = PMATH_BFS_PARSEABLE;
  parse_settings.data           = &info;
  if(track_source_locations) {
    parse_settings.add_debug_metadata = add_debug_metadata;
    me->parser_cache                  = NULL;
  }
  else if(!me->parser_cache) {
    me->parser_cache                  = pmath_ht_create_ex(&pmath_ht_obj_class, 0);
  }
  
  do {
    pmath_unref(result);
    result = PMATH_NULL;
    
    info.current_code = scanner_read(&info);
    spans = pmath_spans_from_string(
              &info.current_code,
              scanner_read,
              NULL,
              NULL,
              scanner_error,
              &info);
              
    if(!info.err && pmath_string_length(info.current_code) > 0) {
      result = pmath_boxes_from_spans_ex(
                 spans,
                 info.current_code,
                 &parse_settings);
                 
      result = _pmath_makeexpression_with_debugmetadata(result);
      
      if(pmath_is_expr_of(result, pmath_System_HoldComplete)) {
        if(pmath_expr_length(result) == 1) {
          pmath_t tmp = result;
          result = pmath_expr_get_item(tmp, 1);
          pmath_unref(tmp);
        }
        else {
          pmath_t debug_metadata = pmath_get_debug_metadata(result);
          result = pmath_expr_set_item(
                     result, 0,
                     pmath_ref(pmath_System_Sequence));
          result = pmath_try_set_debug_metadata(result, debug_metadata);
        }
      }
      
      if(!pmath_same(head, pmath_System_Identity)) {
        pmath_t debug_metadata = pmath_get_debug_metadata(result);
        result = pmath_expr_new_extended(
                   pmath_ref(head), 1,
                   result);
        result = pmath_try_set_debug_metadata(result, debug_metadata);
      }
      
      result = pmath_evaluate(result);
    }
    
    pmath_unref(info.current_code); info.current_code = PMATH_NULL;
    pmath_span_array_free(spans);
    
    info.startline += info.codelines;
    info.codelines = 0;
  } while(!pmath_aborting() && pmath_file_status(file) == PMATH_FILE_OK);
  
  if(info.err) {
    pmath_symbol_set_value(pmath_System_DollarNamespace,     info.ns);
    pmath_symbol_set_value(pmath_System_DollarNamespacePath, info.nspath);
  }
  else {
    pmath_unref(info.ns);
    pmath_unref(info.nspath);
  }
  
  if(me && old_parser_cache != me->parser_cache) {
#ifdef PMATH_DEBUG_LOG
    if(me->parser_cache) {
      unsigned num_interned = pmath_ht_count(   me->parser_cache);
      unsigned capacity     = pmath_ht_capacity(me->parser_cache);
      pmath_debug_print("[%u cached objects during ", num_interned);
      pmath_debug_print_object("Get: ", info.filename, "]\n");
      
      unsigned num_multi[5] = {0};
      for(unsigned i = 0, k = 0; i < capacity; ++i) {
        struct _pmath_object_entry_t *e = pmath_ht_entry(me->parser_cache, i);
        if(e) {
          pmath_t val = e->value;
          if(pmath_is_expr_of_len(val, pmath_System_HoldComplete, 1))
            val = pmath_expr_get_item(val, 1);
          else
            val = pmath_ref(val);
          intptr_t rc = pmath_refcount(val);
          if(rc > 1) {
            if(rc < sizeof(num_multi)/sizeof(num_multi[0]))
              ++num_multi[rc];
            else
              ++num_multi[0];
              
            pmath_debug_print("  [%u]: ", k);
            pmath_debug_print_object("", val, "\n");
          }
          else
            ++num_multi[1];
          ++k;
          pmath_unref(val);
        }
      }
      
      pmath_debug_print("[ ref=1: %u, ref=2: %u, ref=3: %u, ref=4: %u, ref>4: %u]\n", 
        num_multi[1], num_multi[2], num_multi[3], num_multi[4], num_multi[0]);
    }
#endif // PMATH_DEBUG_LOG
    pmath_ht_destroy(me->parser_cache);
    me->parser_cache = old_parser_cache;
  }
  
  //pmath_unref(pmath_thread_local_save(pmath_System_DollarInput, old_input));
  pmath_unref(_pmath_thread_local_save_with(pmath_System_DollarInput, old_input, me));
  pmath_file_close(file);
  pmath_unref(info.filename);
  pmath_unref(name);
  pmath_unref(head);
  
//  if(package_check){
//    name = pmath_evaluate(
//      pmath_parse_string_args(
//          "IsFreeOf($Packages, `1`)",
//        "(o)",
//        pmath_ref(package_check)));
//    pmath_unref(name);
//
//    if(pmath_same(name, pmath_System_True)){
//      pmath_message(PMATH_NULL, "nons", 1, pmath_ref(package_check));
//    }
//
//    pmath_unref(package_check);
//  }

  if(changed_allow_parse_syms) {
    may_parse_symbols = pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, may_parse_symbols);
  }
  
  pmath_unref(may_parse_symbols);
  
  return result;
}

static pmath_t open_read(pmath_t filename, pmath_t character_encoding) { // both will be freed
  return pmath_evaluate(
           pmath_expr_new_extended(
             pmath_ref(pmath_System_OpenRead), 2,
             filename,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_CharacterEncoding),
               character_encoding)));
}


PMATH_PRIVATE pmath_t builtin_get(pmath_expr_t expr) {
  pmath_t options, character_encoding, head, path, name, file, file_type, may_parse_syms;
  pmath_bool_t track_source_locations = TRUE;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)) {
    pmath_unref(name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
    
  character_encoding = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_CharacterEncoding, options));
  head               =                pmath_option_value(PMATH_NULL, pmath_System_Head,              options);
  may_parse_syms     =                pmath_option_value(PMATH_NULL, pmath_System_ParseSymbols,      options);
  path               = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_Path,              options));
  
  pmath_t obj = pmath_option_value(PMATH_NULL, pmath_System_TrackSourceLocations, options);
  if(pmath_same(obj, pmath_System_True)) {
    track_source_locations = TRUE;
  }
  else if(pmath_same(obj, pmath_System_False)) {
    track_source_locations = FALSE;
  }
  else {
    pmath_message(PMATH_NULL, "opttf", 2, pmath_ref(pmath_System_TrackSourceLocations), obj);
    pmath_unref(path);
    pmath_unref(may_parse_syms);
    pmath_unref(character_encoding);
    pmath_unref(head);
    return expr;
  }
  pmath_unref(obj);
  
  pmath_unref(options);
  if(pmath_is_string(path))
    path = pmath_build_value("(o)", path);
    
  if(!check_path(path)) {
    pmath_unref(path);
    pmath_unref(may_parse_syms);
    pmath_unref(character_encoding);
    pmath_unref(head);
    return expr;
  }
  
  pmath_unref(expr); expr = PMATH_NULL;
  
  if(pmath_is_namespace(name)) {
    pmath_string_t fname;
    uint16_t *buf;
    size_t i;
    int j, len;
    
    expr = pmath_evaluate(
             pmath_parse_string_args(
               "IsFreeOf($Packages, `1`)",
               "(o)",
               pmath_ref(name)));
               
    pmath_unref(expr);
    if(!pmath_same(expr, pmath_System_True)) {
    
      PMATH_RUN_ARGS(
        "If(IsFreeOf($NamespacePath, `1`),"
        "  $NamespacePath:= Prepend($NamespacePath, `1`))",
        "(o)",
        name);
        
      pmath_unref(path);
      pmath_unref(may_parse_syms);
      pmath_unref(character_encoding);
      pmath_unref(head);
      return PMATH_NULL;
    }
    
    len = pmath_string_length(name) - 1;
    fname = pmath_string_new_raw(len);
    if(!pmath_string_begin_write(&fname, &buf, NULL)) {
      pmath_unref(fname);
      pmath_unref(name);
      pmath_unref(path);
      pmath_unref(may_parse_syms);
      pmath_unref(character_encoding);
      pmath_unref(head);
      return PMATH_NULL;
    }
    memcpy(buf, pmath_string_buffer(&name), len * sizeof(uint16_t));
    
    for(j = 0; j < len; ++j) {
      if(buf[j] == '`') {
#ifdef PMATH_OS_WIN32
        buf[j] = '\\';
#else
        buf[j] = '/';
#endif
      }
    }
    pmath_string_end_write(&fname, &buf);
    
    for(i = 1; i <= pmath_expr_length(path); ++i) {
      pmath_t testname, test;
      
      testname = pmath_evaluate(
                   pmath_parse_string_args(
                     "ToFileName(`1`, `2`)",
                     "(oo)",
                     pmath_expr_get_item(path, i),
                     pmath_ref(fname)));
                     
      test = pmath_evaluate(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_FileType), 1,
                 pmath_ref(testname)));
      pmath_unref(test);
      if(pmath_same(test, pmath_System_Directory)) {
        testname = pmath_evaluate(
                     pmath_parse_string_args(
                       "ToFileName(`1`, \"init.pmath\")",
                       "(o)",
                       testname));
                       
        test = pmath_evaluate(
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_FileType), 1,
                   pmath_ref(testname)));
                   
        pmath_unref(test);
        if(pmath_same(test, pmath_System_File)) {
          pmath_unref(fname);
          pmath_unref(path);
          pmath_unref(name);
          
          file = open_read(pmath_ref(testname), character_encoding);
          if(!pmath_same(file, pmath_System_DollarFailed))
            return get_file(file, testname, head, track_source_locations, may_parse_syms);
            
          pmath_unref(testname);
          pmath_unref(file);
          pmath_unref(may_parse_syms);
          pmath_unref(head);
          return pmath_ref(pmath_System_DollarFailed);
        }
      }
      pmath_unref(testname);
      
      
      testname = pmath_evaluate(
                   pmath_parse_string_args(
                     "ToFileName(`1`, `2` ++ \".pmath\")",
                     "(oo)",
                     pmath_expr_get_item(path, i),
                     pmath_ref(fname)));
                     
      test = pmath_evaluate(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_FileType), 1,
                 pmath_ref(testname)));
                 
      pmath_unref(test);
      if(pmath_same(test, pmath_System_File)) {
        pmath_unref(fname);
        pmath_unref(path);
        pmath_unref(name);
        
        file = open_read(pmath_ref(testname), character_encoding);
        if(!pmath_same(file, pmath_System_DollarFailed))
          return get_file(file, testname, head, track_source_locations, may_parse_syms);
          
        pmath_unref(testname);
        pmath_unref(file);
        pmath_unref(may_parse_syms);
        pmath_unref(head);
        return pmath_ref(pmath_System_DollarFailed);
      }
      pmath_unref(testname);
    }
    
    pmath_unref(fname);
    pmath_unref(character_encoding);
    pmath_unref(head);
    pmath_unref(path);
    pmath_unref(may_parse_syms);
    pmath_message(PMATH_NULL, "noopen", 1, name);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(path);
  
  file_type = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_FileType), 1,
                  pmath_ref(name)));
                  
  pmath_unref(file_type);
  if(!pmath_same(file_type, pmath_System_File)) {
    pmath_unref(may_parse_syms);
    pmath_unref(character_encoding);
    pmath_unref(head);
    pmath_message(PMATH_NULL, "noopen", 1, name);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  file = open_read(pmath_ref(name), character_encoding);
  if(!pmath_same(file, pmath_System_DollarFailed))
    return get_file(file, name, head, track_source_locations, may_parse_syms);
    
  pmath_unref(file);
  pmath_unref(name);
  pmath_unref(may_parse_syms);
  pmath_unref(head);
  return pmath_ref(pmath_System_DollarFailed);
}
