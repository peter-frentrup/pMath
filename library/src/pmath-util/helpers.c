#include <pmath-util/helpers.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/control/definitions-private.h>


PMATH_API pmath_bool_t pmath_is_expr_of(pmath_t obj, pmath_symbol_t head){
  if(pmath_is_expr(obj)){
    pmath_t h = pmath_expr_get_item(obj, 0);
    pmath_unref(h);

    return pmath_same(h, head);
  }

  return FALSE;
}

PMATH_API pmath_bool_t pmath_is_expr_of_len(
  pmath_t        obj,
  pmath_symbol_t head,
  size_t         length
){
  if(pmath_is_expr(obj)
  && pmath_expr_length(obj) == length){
    pmath_t h = pmath_expr_get_item(obj, 0);
    pmath_unref(h);

    return pmath_same(h, head);
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
        return PMATH_FROM_DOUBLE(d == 0 ? +0.0 : d); // convert -0.0 to +0.0
      
      if(d > 0)
        return pmath_ref(_pmath_object_infinity);
      
      if(d < 0)
        return pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
          pmath_integer_new_si(-1));
      
    } return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    
    case 'o': return va_arg(*args, pmath_t);
    
    case 'c': {
      int c = va_arg(*args, int);
      
      if(c <= 0xffff){
        uint16_t uc = (uint16_t)c;
        
        return pmath_string_insert_ucs2(PMATH_NULL, 0, &uc, 1);
      }
      
      if(c <= 0x10ffff){
        uint16_t uc[2];
        
        c-= 0x10000;
        uc[0] = 0xD800 | (c >> 10);
        uc[1] = 0xDC00 | (c & 0x03FF);
        
        return pmath_string_insert_ucs2(PMATH_NULL, 0, uc, 2);
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
      
      return pmath_string_insert_latin1(PMATH_NULL, 0, s, len);
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
      
      return pmath_string_insert_ucs2(PMATH_NULL, 0, s, len);
    }
    
    case 'C': {
      pmath_t real = next_value(format, args);
      pmath_t imag = next_value(format, args);
      
      if(pmath_same(real, PMATH_UNDEFINED) 
      || pmath_same(imag, PMATH_UNDEFINED)){
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
      
      if(pmath_same(num, PMATH_UNDEFINED)
      || pmath_same(den, PMATH_UNDEFINED)){
        pmath_debug_print("unclosed complex\n");
        assert(0 && "unclosed complex");
      }
      
      if(pmath_is_integer(num) && pmath_is_integer(den)){
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
//      if(pmath_same(head, PMATH_UNDEFINED)
//      || pmath_same(expr, PMATH_UNDEFINED)){
//        pmath_debug_print("unfinished function\n");
//        assert(0 && "unfinished functionx");
//      }
//      
//      if(!pmath_is_expr(expr))
//        return pmath_expr_new_extended(head, 1, expr);
//      
//      return pmath_expr_set_item(expr, 0, head);
//    }
    
    case '(': {
      pmath_gather_begin(PMATH_NULL);
      
      for(;;){
        pmath_t obj = next_value(format, args);
        if(pmath_same(obj, PMATH_UNDEFINED))
          break;
          
        pmath_emit(obj, PMATH_NULL);
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
  
  return PMATH_NULL;
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
    
  pmath_gather_begin(PMATH_NULL);
  
  pmath_emit(obj, PMATH_NULL);
  while(*format)
    pmath_emit(next_value(&format, &args), PMATH_NULL);
  
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
  size_t       last_nonoption
){
  pmath_t option;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(last_nonoption > len){
    pmath_message_argxxx(len, last_nonoption, last_nonoption);
    return PMATH_NULL;
  }

  if(last_nonoption == len)
    return pmath_ref(_pmath_object_emptylist);

  option = pmath_expr_get_item(expr, last_nonoption + 1);
  if(_pmath_is_list_of_rules(option)){
    if(last_nonoption + 1 == len)
      return option;

    pmath_unref(option);
    pmath_message(
      PMATH_NULL, "nonopt", 3,
      pmath_expr_get_item(expr, last_nonoption + 2),
      pmath_integer_new_ui(last_nonoption),
      pmath_ref(expr));
    return PMATH_NULL;
  }
  pmath_unref(option);

  for(i = last_nonoption + 1;i <= len;++i){
    option = pmath_expr_get_item(expr, i);

    if(!_pmath_is_rule(option)){
      pmath_message(
        PMATH_NULL, "nonopt", 3,
        option,
        pmath_integer_new_ui(last_nonoption),
        pmath_ref(expr));
      return PMATH_NULL;
    }
    pmath_unref(option);
  }

  return pmath_expr_set_item(
    pmath_expr_get_item_range(expr, last_nonoption + 1, SIZE_MAX),
    0, pmath_ref(PMATH_SYMBOL_LIST));
}

static pmath_bool_t find_option_value(
  pmath_t       fn,         // wont be freed; if PMATH_UNDEFINED, no message is printed on error
  pmath_expr_t  fnoptions,  // wont be freed; must have form {a->b, c->d, ...});
  pmath_t      *optionvalue // input & output
){
  size_t i;
  for(i = 1;i <= pmath_expr_length(fnoptions);++i){
    pmath_expr_t rule = pmath_expr_get_item(fnoptions, i);
    pmath_t rulelhs   = pmath_expr_get_item(rule, 1);
    
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

  if(!pmath_same(fn, PMATH_UNDEFINED)){
    pmath_message(
      PMATH_SYMBOL_OPTIONVALUE, "optnf", 2,
      pmath_ref(*optionvalue),
      pmath_ref(fn));
  }

  return FALSE;
}

static pmath_bool_t verify_subset_is_options_subset(
  pmath_t     fn,      // wont be freed;
  pmath_expr_t fnoptions, // wont be freed; must have form {a->b, c->d, ...}
  pmath_expr_t subset   // wont be freed; must have form {a->b, c->d, ...}
){
  size_t i;
  
  if(pmath_is_magic(fn))
    return TRUE;
    
  for(i = 1;i <= pmath_expr_length(subset);++i){
    pmath_expr_t rule = pmath_expr_get_item(subset, i);
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
  pmath_t fnoptions;
  pmath_t result;
  pmath_symbol_t head;
  
  if(pmath_is_null(fn))
    fn = pmath_current_head();
  else
    fn = pmath_ref(fn);
  
  head = PMATH_NULL;
  if(pmath_is_symbol(fn)){
    head = pmath_ref(fn);
  }
  else if(pmath_is_expr(fn)){
    head = pmath_expr_get_item(fn, 0);
    if(!pmath_is_symbol(fn)){
      pmath_unref(head);
      head = PMATH_NULL;
    }
  }
  
  fnoptions = PMATH_NULL;
  if(!pmath_is_null(head)){
    rules = _pmath_symbol_get_rules(head, RULES_READ);
    
    if(rules){
      fnoptions = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_OPTIONS), 1, pmath_ref(head));
      
      if(!_pmath_rulecache_find(&rules->default_rules, &fnoptions)){
        pmath_unref(fnoptions);
        fnoptions = PMATH_NULL;
      }
    }
    
    pmath_unref(head);
  }
  
  if(pmath_is_null(fnoptions)){
    fnoptions = pmath_ref(_pmath_object_emptylist);
  }
  
  if(!_pmath_is_list_of_rules(fnoptions)){
    pmath_message(PMATH_SYMBOL_OPTIONVALUE, "reps", 1, fnoptions);
    pmath_unref(fn);
    //pmath_unref(extra);
    return pmath_ref(name);
  }

  if(!pmath_same(extra, PMATH_UNDEFINED)){
    extra = pmath_ref(extra); 
    if(!_pmath_is_list_of_rules(extra))
      extra = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 1, extra);

    if(_pmath_is_list_of_rules(extra)){
      pmath_t result;
      
      if(pmath_is_symbol(fn))
        verify_subset_is_options_subset(fn, fnoptions, extra);
      
      result = pmath_ref(name);
      if(find_option_value(PMATH_UNDEFINED, extra, &result)){
        pmath_unref(fn);
        pmath_unref(fnoptions);
        pmath_unref(extra);
        return result;
      }
      pmath_unref(result);
      
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
    return PMATH_NULL;

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
  if(!pmath_same(exception, PMATH_UNDEFINED)
  && !pmath_same(exception, PMATH_ABORT_EXCEPTION)){
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
    return PMATH_NULL;
  }
  
  _pmath_clear(PMATH_SYMBOL_MESSAGECOUNT, FALSE);
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
