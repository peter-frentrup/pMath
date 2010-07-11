#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/custom.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/control-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>

PMATH_API void pmath_gather_begin(pmath_t pattern){
  struct _pmath_gather_info_t *info;
  pmath_thread_t thread = pmath_thread_get_current();
  if(!thread){
    pmath_unref(pattern);
    return;
  }
  
  if(thread->gather_failed){
    pmath_unref(pattern);
    thread->gather_failed++;
    return;
  }
    
  info = (struct _pmath_gather_info_t*)
    pmath_mem_alloc(sizeof(struct _pmath_gather_info_t));
  if(!info){
    pmath_unref(pattern);
    pmath_mem_free(info);
    thread->gather_failed++;
    return;
  }

  info->next               = thread->gather_info;
  info->pattern            = pattern;
  info->value_count        = 0;
  info->emitted_values.ptr = NULL;
  thread->gather_info      = info;
}

PMATH_API pmath_expr_t pmath_gather_end(void){
  struct _pmath_gather_info_t *info;
  struct _pmath_stack_info_t  *item;
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_expr_t result;
  size_t i;
   
  if(!thread)
    return NULL;
  
  if(thread->gather_failed){
    --thread->gather_failed;
    return NULL;
  }
  
  if(!thread->gather_info)
    return NULL;

  info = thread->gather_info;
  thread->gather_info = info->next;
  pmath_unref(info->pattern);

  result = pmath_expr_new(
    pmath_ref(PMATH_SYMBOL_LIST),
    info->value_count);
  i = info->value_count + 1;

  while(info->emitted_values.ptr){
    item = info->emitted_values.ptr;
    info->emitted_values.ptr = item->next;

    result = pmath_expr_set_item(result, --i, item->value);
    pmath_mem_free(item);
  }

  pmath_mem_free(info);
  return result;
}

PMATH_API void pmath_emit(pmath_t object, pmath_t tag){
  pmath_bool_t at_least_one_gather = FALSE;
  pmath_thread_t thread = pmath_thread_get_current();
  struct _pmath_gather_info_t *info;
  struct _pmath_stack_info_t  *item = (struct _pmath_stack_info_t*)
    pmath_mem_alloc(sizeof(struct _pmath_stack_info_t));
  if(!thread || !item){
    pmath_unref(object);
    pmath_unref(tag);
    pmath_mem_free(item);
    return;
  }

  item->value = object;
  while(thread){
    info = thread->gather_info;
    at_least_one_gather = at_least_one_gather || info != NULL;
    while(info && !_pmath_pattern_match(tag, pmath_ref(info->pattern), NULL))
      info = info->next;

    if(info){
      item->next = (struct _pmath_stack_info_t*)
        pmath_atomic_fetch_set(
          &info->emitted_values.intptr,
          (intptr_t)item);
      (void)pmath_atomic_fetch_add(&info->value_count, 1);
      pmath_unref(tag);
      return;
    }
    thread = thread->parent;
  }
  if(at_least_one_gather)
    pmath_message(PMATH_SYMBOL_EMIT, "nogather2", 1, tag);
  else{
    pmath_unref(tag);
    pmath_message(PMATH_SYMBOL_EMIT, "nogather", 0);
  }
  pmath_unref(object);
  pmath_mem_free(item);
}

/*============================================================================*/

PMATH_API pmath_bool_t pmath_is_expr_of(
  pmath_t obj,
  pmath_symbol_t head
){
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    pmath_t h = pmath_expr_get_item(obj, 0);
    pmath_unref(h);

    return h == head;
  }

  return FALSE;
}

PMATH_API pmath_bool_t pmath_is_expr_of_len(
  pmath_t obj,
  pmath_symbol_t head,
  size_t         length
){
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)
  && pmath_expr_length(obj) == length){
    pmath_t h = pmath_expr_get_item(obj, 0);
    pmath_unref(h);

    return h == head;
  }

  return FALSE;
}

/*============================================================================*/

static pmath_t next_value(const char **format, va_list *args){
  while(**format > '\0' && **format <= ' ')
    ++*format;
  
  switch(*(*format)++){
    case 'b': {
      if(va_arg(*args, int))
        return pmath_ref(PMATH_SYMBOL_TRUE);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }
    
    case 'i': return pmath_integer_new_si(va_arg(*args, int));
    case 'I': return pmath_integer_new_ui(va_arg(*args, unsigned int));
    
    case 'l': return pmath_integer_new_si(va_arg(*args, long));
    case 'L': return pmath_integer_new_ui(va_arg(*args, unsigned long));
    
    case 'k': {
      long long i = va_arg(*args, long long);
      
      if(i < 0){
        i = -i;
        return pmath_number_neg(
          pmath_integer_new_data(1, 1, sizeof(long long), 0, 0, &i));
      }
      
      return pmath_integer_new_data(1, 1, sizeof(long long), 0, 0, &i);
    } break;
    
    case 'K': {
      unsigned long long i = va_arg(*args, unsigned long long);
      
      return pmath_integer_new_data(1, 1, sizeof(unsigned long long), 0, 0, &i);
    } break;
    
    case 'n': {
      intptr_t i = va_arg(*args, intptr_t); // ssize_t
      
      if(i < 0){
        i = -i;
        return pmath_number_neg(
          pmath_integer_new_data(1, 1, sizeof(i), 0, 0, &i));
      }
      
      return pmath_integer_new_data(1, 1, sizeof(i), 0, 0, &i);
    } break;
    
    case 'N': {
      size_t i = va_arg(*args, size_t);
      
      return pmath_integer_new_data(1, 1, sizeof(size_t), 0, 0, &i);
    } break;
    
    case 'f': {
      double d = va_arg(*args, double);
      
      if(isfinite(d))
        return pmath_float_new_d(d == 0 ? +0.0 : d); // convert -0.0 to +0.0
      
      if(d > 0)
        return pmath_ref(_pmath_object_infinity);
      
      if(d < 0)
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
          pmath_integer_new_si(-1));
      
    } return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
    
    case 'o': return (pmath_t)va_arg(*args, void*);
    
    case 'c': {
      int c = va_arg(*args, int);
      
      if(c <= 0xffff){
        uint16_t uc = (uint16_t)c;
        
        return pmath_string_insert_ucs2(NULL, 0, &uc, 1);
      }
      
      if(c <= 0x10ffff){
        uint16_t uc[2];
        
        c-= 0x10000;
        uc[0] = 0xD800 | (c >> 10);
        uc[1] = 0xDC00 | (c & 0x03FF);
        
        return pmath_string_insert_ucs2(NULL, 0, uc, 2);
      }
      
      return PMATH_C_STRING("");
    }
    
    case 's': {
      const char *s = va_arg(*args, const char*);
      int len = -1;
      
      if(**format == '#'){
        ++*format;
        len = va_arg(*args, int);
      }
      
      return pmath_string_insert_latin1(NULL, 0, s, len);
    }
    
    case 'z': return pmath_symbol_find(
      PMATH_C_STRING(va_arg(*args, const char*)), 
      TRUE);
    
    case 'u': {
      const char *s = va_arg(*args, const char*);
      int len = -1;
      
      if(**format == '#'){
        ++*format;
        len = va_arg(*args, int);
      }
      
      return pmath_string_from_utf8(s, len);
    }
    
    case 'U': {
      const uint16_t *s = va_arg(*args, const uint16_t*);
      int len = -1;
      
      if(**format == '#'){
        ++*format;
        len = va_arg(*args, int);
      }
      
      return pmath_string_insert_ucs2(NULL, 0, s, len);
    }
    
    case 'C': {
      pmath_t real = next_value(format, args);
      pmath_t imag = next_value(format, args);
      
      if(real == PMATH_UNDEFINED || imag == PMATH_UNDEFINED){
        pmath_debug_print("unclosed complex\n");
        assert(0 && "unclosed complex");
      }
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
        real,
        imag);
    }
    
    case 'Q': {
      pmath_t num = next_value(format, args);
      pmath_t den = next_value(format, args);
      
      if(num == PMATH_UNDEFINED || den == PMATH_UNDEFINED){
        pmath_debug_print("unclosed complex\n");
        assert(0 && "unclosed complex");
      }
      
      if(pmath_instance_of(num, PMATH_TYPE_INTEGER)
      && pmath_instance_of(den, PMATH_TYPE_INTEGER)){
        return pmath_rational_new(num, den);
      }
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        num,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          den,
          pmath_integer_new_si(-1)));
    }
    
//    case '@': {
//      pmath_t head = next_value(format, args);
//      pmath_t expr = next_value(format, args);
//      
//      if(head == PMATH_UNDEFINED || expr == PMATH_UNDEFINED){
//        pmath_debug_print("unfinished function\n");
//        assert(0 && "unfinished functionx");
//      }
//      
//      if(!pmath_instance_of(expr, PMATH_TYPE_EXPRESSION))
//        return pmath_expr_new_extended(head, 1, expr);
//      
//      return pmath_expr_set_item(expr, 0, head);
//    }
    
    case '(': {
      pmath_gather_begin(NULL);
      
      for(;;){
        pmath_t obj = next_value(format, args);
        if(obj == PMATH_UNDEFINED)
          break;
          
        pmath_emit(obj, NULL);
      }
      
      if(*(*format)++ != ')'){
        pmath_debug_print("unclosed list\n");
        assert(0 && "unclosed list");
      }
      
      return pmath_gather_end();
    }
    
    case '\0': 
    case ')': 
      --*format;
      return PMATH_UNDEFINED;
  }
  
  --*format;
  pmath_debug_print("invalid format char `%c`\n", **format);
  assert(0 && "invalid format char");
  
  return NULL;
}

PMATH_API
pmath_t pmath_build_value_v(
  const char *format,
  va_list     args
){
  pmath_t obj;
  
  if(*format == '\0')
    return pmath_ref(_pmath_object_emptylist);
  
  obj = next_value(&format, &args);
  if(*format == '\0')
    return obj;
    
  pmath_gather_begin(NULL);
  
  pmath_emit(obj, NULL);
  while(*format)
    pmath_emit(next_value(&format, &args), NULL);
  
  return pmath_gather_end();
}

PMATH_API
pmath_t pmath_build_value(
  const char *format,
  ...
){
  va_list args;
  pmath_expr_t result;
  
  va_start(args, format);
  
  result = pmath_build_value_v(format, args);
  
  va_end(args);
  
  return result;
}

/*============================================================================*/

PMATH_API pmath_expr_t pmath_options_extract(
  pmath_expr_t expr,
  size_t             last_nonoption
){
  pmath_t option;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(last_nonoption > len){
    pmath_message_argxxx(len, last_nonoption, last_nonoption);
    return NULL;
  }

  if(last_nonoption == len)
    return pmath_ref(_pmath_object_emptylist);

  option = pmath_expr_get_item(expr, last_nonoption + 1);
  if(_pmath_is_list_of_rules(option)){
    if(last_nonoption + 1 == len)
      return (pmath_expr_t)option;

    pmath_unref(option);
    pmath_message(
      NULL, "nonopt", 3,
      pmath_expr_get_item(expr, last_nonoption + 2),
      pmath_integer_new_ui(last_nonoption),
      pmath_ref(expr));
    return NULL;
  }
  pmath_unref(option);

  for(i = last_nonoption + 1;i <= len;++i){
    option = pmath_expr_get_item(expr, i);

    if(!_pmath_is_rule(option)){
      pmath_message(
        NULL, "nonopt", 3,
        option,
        pmath_integer_new_ui(last_nonoption),
        pmath_ref(expr));
      return NULL;
    }
    pmath_unref(option);
  }

  return pmath_expr_set_item(
    pmath_expr_get_item_range(expr, last_nonoption + 1, SIZE_MAX),
    0, pmath_ref(PMATH_SYMBOL_LIST));
}

static pmath_bool_t find_option_value(
  pmath_t      fn,       // wont be freed; if PMATH_UNDEFINED, no message is printed on error
  pmath_expr_t  fnoptions,  // wont be freed; must have form {a->b, c->d, ...});
  pmath_t     *optionvalue // input & output
){
  size_t i;
  for(i = 1;i <= pmath_expr_length(fnoptions);++i){
    pmath_expr_t rule = (pmath_expr_t)
      pmath_expr_get_item(fnoptions, i);
    pmath_t rulelhs = pmath_expr_get_item(rule, 1);
    if(pmath_equals(rulelhs, *optionvalue)){
      pmath_unref(rulelhs);
      pmath_unref(*optionvalue);
      *optionvalue = pmath_expr_get_item(rule, 2);
      pmath_unref(rule);
      return TRUE;
    }
    pmath_unref(rulelhs);
    pmath_unref(rule);
  }

  if(fn != PMATH_UNDEFINED){
    pmath_message(
      PMATH_SYMBOL_OPTIONVALUE, "optnf", 2,
      pmath_ref(fn),
      pmath_ref(*optionvalue));
  }

  return FALSE;
}

static pmath_bool_t verify_subset_is_options_subset(
  pmath_t     fn,      // wont be freed;
  pmath_expr_t fnoptions, // wont be freed; must have form {a->b, c->d, ...}
  pmath_expr_t subset   // wont be freed; must have form {a->b, c->d, ...}
){
  size_t i;
  
  if(PMATH_IS_MAGIC(fn))
    return TRUE;
    
  for(i = 1;i <= pmath_expr_length(subset);++i){
    pmath_expr_t rule = (pmath_expr_t)pmath_expr_get_item(subset, i);
    pmath_t rulelhs = pmath_expr_get_item(rule, 1);
    pmath_unref(rule);
    if(!find_option_value(PMATH_UNDEFINED, fnoptions, &rulelhs)){
//      pmath_message(
//        PMATH_SYMBOL_OPTIONVALUE, "optnf", 2,
//        pmath_ref(fn),
//        rulelhs);
      pmath_unref(rulelhs);
      return FALSE;
    }
    pmath_unref(rulelhs);
  }

  return TRUE;
}

PMATH_API pmath_t pmath_option_value(
  pmath_t fn,   // wont be freed;
  pmath_t name, // wont be freed;
  pmath_t extra // wont be freed; may be PMATH_UNDEFINED or `a->b` or `{a->b, ...}`.
){
  struct _pmath_symbol_rules_t  *rules;
  pmath_t                 fnoptions;
  pmath_t                 result;
  
  if(fn)
    fn = pmath_ref(fn);
  else
    fn = pmath_current_head();
  
  fnoptions = NULL;
  if(pmath_instance_of(fn, PMATH_TYPE_SYMBOL)){
    rules = _pmath_symbol_get_rules(fn, RULES_READ);
    
    if(rules){
      fnoptions = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_OPTIONS), 1, pmath_ref(fn));
      
      if(!_pmath_rulecache_find(&rules->default_rules, &fnoptions)){
        pmath_unref(fnoptions);
        fnoptions = NULL;
      }
    }
  }
  else if(pmath_instance_of(fn, PMATH_TYPE_EXPRESSION)){
    size_t start, end;
    pmath_bool_t have_rule_list = FALSE;
    
    end = pmath_expr_length(fn);
    for(start = end;start > 0;--start){
      pmath_t opt = pmath_expr_get_item(fn, start);
      
      if(_pmath_is_list_of_rules(opt)){
        have_rule_list = TRUE;
      }
      else if(!_pmath_is_rule(opt)){
        pmath_unref(opt);
        break;
      }
      
      pmath_unref(opt);
    }
    ++start;
    
    if(start <= end){
      fnoptions = pmath_expr_get_item_range(fn, start, SIZE_MAX);
      
      fnoptions = pmath_expr_set_item(
        fnoptions, 0, pmath_ref(PMATH_SYMBOL_LIST));
      
      if(have_rule_list){
        fnoptions = pmath_expr_flatten(
          fnoptions, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
      }
    }
  }
  
  if(!fnoptions){
    fnoptions = pmath_ref(_pmath_object_emptylist);
  }
  else if(!_pmath_is_list_of_rules(fnoptions)){
    pmath_message(PMATH_SYMBOL_OPTIONVALUE, "reps", 1, fnoptions);
    pmath_unref(fn);
    //pmath_unref(extra);
    return pmath_ref(name);
  }

  if(extra != PMATH_UNDEFINED){
    extra = pmath_ref(extra); 
    if(!_pmath_is_list_of_rules(extra))
      extra = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 1, extra);

    if(_pmath_is_list_of_rules(extra)){
      if(verify_subset_is_options_subset(fn, fnoptions, extra)){
        pmath_t result = pmath_ref(name);
        if(find_option_value(PMATH_UNDEFINED, extra, &result)){
          pmath_unref(fn);
          pmath_unref(fnoptions);
          pmath_unref(extra);
          return result;
        }
        pmath_unref(result);
      }
    }else
      pmath_message(PMATH_SYMBOL_OPTIONVALUE, "reps", 1, pmath_ref(extra));

    pmath_unref(extra);
  }

  result = pmath_ref(name);
  find_option_value(fn, fnoptions, &result);
  pmath_unref(fn);
  pmath_unref(fnoptions);
  return result;
}

/*============================================================================*/

PMATH_API pmath_t pmath_current_head(void){
  pmath_thread_t thread = pmath_thread_get_current();
  if(!thread || !thread->stack_info)
    return NULL;

  return pmath_ref(thread->stack_info->value);
}

PMATH_API void pmath_walk_stack(pmath_stack_walker_t walker, void *closure){
  pmath_thread_t thread = pmath_thread_get_current();
  
  while(thread){
    struct _pmath_stack_info_t *stack_info = thread->stack_info;
    while(stack_info){
      if(!walker(stack_info->value, closure))
        return;
      stack_info = stack_info->next;
    }
    
    thread = thread->parent;
  }
}

/*============================================================================*/

static void warn_uncaught_exception(void){
  pmath_t exception = pmath_catch();
  if(exception != PMATH_UNDEFINED
  && exception != PMATH_ABORT_EXCEPTION){
    pmath_message(PMATH_SYMBOL_THROW, "nocatch", 1, exception);
  }
}

PMATH_API 
pmath_t pmath_session_execute(pmath_t input, pmath_bool_t *aborted){
  pmath_t output;
  pmath_bool_t did_abort = FALSE;
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // Increment($Line)
  pmath_unref(pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INCREMENT), 1,
      pmath_ref(PMATH_SYMBOL_LINE))));
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  output = pmath_evaluate(input);
  
  if(pmath_aborting()){
    pmath_unref(output);
    output = pmath_ref(PMATH_SYMBOL_ABORTED);
  }
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // $History($Line)::= output  [output is already evaluated]
  pmath_unref(pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED),2,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_HISTORY), 1,
        pmath_ref(PMATH_SYMBOL_LINE)),
      pmath_ref(output))));
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // $History($Line - $HistoryLength):= .
  pmath_unref(pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_UNASSIGN), 1,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_HISTORY), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          pmath_ref(PMATH_SYMBOL_LINE),
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            pmath_integer_new_si(-1),
            pmath_ref(PMATH_SYMBOL_HISTORYLENGTH)))))));
  
  pmath_collect_temporary_symbols();
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  if(aborted)
    *aborted = did_abort;
  
  return output;
}

PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_session_start(void){
  pmath_t old_dlvl = pmath_evaluate(
    pmath_parse_string(PMATH_C_STRING("$DialogLevel++")));
    
  pmath_t old_line = pmath_evaluate(pmath_ref(PMATH_SYMBOL_LINE));
  
  pmath_t old_msgcnt = pmath_evaluate(
    pmath_parse_string(PMATH_C_STRING("DownRules($MessageCount)")));
  
  if(pmath_aborting()){
    pmath_unref(old_dlvl);
    pmath_unref(old_line);
    pmath_unref(old_msgcnt);
    return NULL;
  }
  
  _pmath_clear(PMATH_SYMBOL_MESSAGECOUNT);
  return pmath_build_value("(ooo)", old_dlvl, old_line, old_msgcnt);
}

PMATH_API 
void pmath_session_end(pmath_t old_state){
  if(pmath_is_expr_of_len(old_state, PMATH_SYMBOL_LIST, 3)){
//    pmath_debug_print_object("\nend session -> ", old_state, "\n");
    PMATH_RUN_ARGS(
        "$DialogLevel:= `1`;"
        "$Line:= `2`;"
        "DownRules($MessageCount):= `3`;", 
      "o", old_state); // note that this is not "(o)"
  }
  else
    pmath_unref(old_state);
}
