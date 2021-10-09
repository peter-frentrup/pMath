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
#include <pmath-builtins/control/definitions-private.h>


#ifndef va_copy
#  define va_copy(dst, src) do{ dst = (src); }while(0)
#endif

extern pmath_symbol_t pmath_System_DollarAborted;
extern pmath_symbol_t pmath_System_DollarHistory;
extern pmath_symbol_t pmath_System_DollarHistoryLength;
extern pmath_symbol_t pmath_System_DollarLine;
extern pmath_symbol_t pmath_System_DollarMessageCount;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Throw;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Unassign;
extern pmath_symbol_t pmath_System_Undefined;


PMATH_API pmath_bool_t pmath_is_expr_of(pmath_t obj, pmath_symbol_t head) {
  if(pmath_is_expr(obj)) {
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
) {
  if( pmath_is_expr(obj) &&
      pmath_expr_length(obj) == length)
  {
    pmath_t h = pmath_expr_get_item(obj, 0);
    pmath_unref(h);
    
    return pmath_same(h, head);
  }
  
  return FALSE;
}

/*============================================================================*/

static pmath_t next_value(const char **format, va_list *args) {
  while(**format > '\0' && **format <= ' ')
    ++*format;
    
  switch(*(*format)++) {
    case 'b': {
        if(va_arg(*args, int))
          return pmath_ref(pmath_System_True);
        return pmath_ref(pmath_System_False);
      }
      
    case 'i': return pmath_integer_new_slong(va_arg(*args, int));
    case 'I': return pmath_integer_new_ulong(va_arg(*args, unsigned int));
    
    case 'l': return pmath_integer_new_slong(va_arg(*args, long));
    case 'L': return pmath_integer_new_ulong(va_arg(*args, unsigned long));
    
    case 'k': {
        long long i = va_arg(*args, long long);
        
        if(i < 0) {
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
        
        if(i < 0) {
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
          return pmath_ref(_pmath_object_pos_infinity);
          
        if(d < 0)
          return pmath_ref(_pmath_object_neg_infinity);
                   
      } return pmath_ref(pmath_System_Undefined);
      
    case 'o': return va_arg(*args, pmath_t);
    
    case 'c': {
        int c = va_arg(*args, int);
        
        if(c <= 0xffff) {
          uint16_t uc = (uint16_t)c;
          
          return pmath_string_insert_ucs2(PMATH_NULL, 0, &uc, 1);
        }
        
        if(c <= 0x10ffff) {
          uint16_t uc[2];
          
          c -= 0x10000;
          uc[0] = 0xD800 | (c >> 10);
          uc[1] = 0xDC00 | (c & 0x03FF);
          
          return pmath_string_insert_ucs2(PMATH_NULL, 0, uc, 2);
        }
        
        return PMATH_C_STRING("");
      }
      
    case 's': {
        const char *s = va_arg(*args, const char *);
        int len = -1;
        
        if(**format == '#') {
          ++*format;
          len = va_arg(*args, int);
        }
        
        return pmath_string_insert_latin1(PMATH_NULL, 0, s, len);
      }
      
    case 'z': return pmath_symbol_find(
                         PMATH_C_STRING(va_arg(*args, const char *)),
                         TRUE);
                         
    case 'u': {
        const char *s = va_arg(*args, const char *);
        int len = -1;
        
        if(**format == '#') {
          ++*format;
          len = va_arg(*args, int);
        }
        
        return pmath_string_from_utf8(s, len);
      }
      
    case 'U': {
        const uint16_t *s = va_arg(*args, const uint16_t *);
        int len = -1;
        
        if(**format == '#') {
          ++*format;
          len = va_arg(*args, int);
        }
        
        return pmath_string_insert_ucs2(PMATH_NULL, 0, s, len);
      }
      
    case 'C': {
        pmath_t real = next_value(format, args);
        pmath_t imag = next_value(format, args);
        
        if( pmath_same(real, PMATH_UNDEFINED) ||
            pmath_same(imag, PMATH_UNDEFINED))
        {
          pmath_debug_print("unclosed complex\n");
          assert(0 && "unclosed complex");
        }
        
        return pmath_expr_new_extended(
                 pmath_ref(pmath_System_Complex), 2,
                 real,
                 imag);
      }
      
    case 'Q': {
        pmath_t num = next_value(format, args);
        pmath_t den = next_value(format, args);
        
        if( pmath_same(num, PMATH_UNDEFINED) ||
            pmath_same(den, PMATH_UNDEFINED))
        {
          pmath_debug_print("unclosed rational\n");
          assert(0 && "unclosed rational");
        }
        
        if(pmath_is_integer(num) && pmath_is_integer(den)) {
          return pmath_rational_new(num, den);
        }
        
        return pmath_expr_new_extended(
                 pmath_ref(pmath_System_Times), 2,
                 num,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_Power), 2,
                   den,
                   PMATH_FROM_INT32(-1)));
      }
      
    case '(': {
        pmath_gather_begin(PMATH_NULL);
        
        while(**format && **format != ')') {
          pmath_t obj = next_value(format, args);
          //if(pmath_same(obj, PMATH_UNDEFINED))
          //  break;
          
          pmath_emit(obj, PMATH_NULL);
        }
        
        if(*(*format)++ != ')') {
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
) {
  va_list tmp_args;
  pmath_t obj;
  
  if(*format == '\0')
    return pmath_ref(_pmath_object_emptylist);
    
  va_copy(tmp_args, args);
  
  obj = next_value(&format, &tmp_args);
  if(*format == '\0') {
    va_end(tmp_args);
    return obj;
  }
  
  pmath_gather_begin(PMATH_NULL);
  
  pmath_emit(obj, PMATH_NULL);
  while(*format)
    pmath_emit(next_value(&format, &tmp_args), PMATH_NULL);
    
  va_end(tmp_args);
  return pmath_gather_end();
}

PMATH_API
pmath_t pmath_build_value(
  const char *format,
  ...
) {
  va_list args;
  pmath_expr_t result;
  
  va_start(args, format);
  
  result = pmath_build_value_v(format, args);
  
  va_end(args);
  
  return result;
}

/*============================================================================*/

PMATH_API pmath_t pmath_current_head(void) {
  pmath_thread_t thread = pmath_thread_get_current();
  if(!thread || !thread->stack_info)
    return PMATH_NULL;
    
  return pmath_ref(thread->stack_info->head);
}

PMATH_API void pmath_walk_stack(
  pmath_bool_t (*walker)(pmath_t head, void *closure), 
  void *closure
) {
  pmath_thread_t thread = pmath_thread_get_current();
  
  while(thread) {
    struct _pmath_stack_info_t *stack_info = thread->stack_info;
    while(stack_info) {
      if(!walker(stack_info->head, closure))
        return;
      stack_info = stack_info->next;
    }
    
    thread = thread->parent;
  }
}

PMATH_API 
void pmath_walk_stack_2(
  pmath_bool_t (*walker)(pmath_t head, pmath_t debug_info, void *closure), 
  void *closure
) {
  pmath_thread_t thread = pmath_thread_get_current();
  
  while(thread) {
    struct _pmath_stack_info_t *stack_info = thread->stack_info;
    while(stack_info) {
      if(!walker(stack_info->head, stack_info->debug_info, closure))
        return;
      stack_info = stack_info->next;
    }
    
    thread = thread->parent;
  }
}

/*============================================================================*/

static void warn_uncaught_exception(void) {
  pmath_t exception = pmath_catch();
  if( !pmath_same(exception, PMATH_UNDEFINED) &&
      !pmath_same(exception, PMATH_ABORT_EXCEPTION))
  {
    pmath_message(pmath_System_Throw, "nocatch", 1, exception);
  }
}

PMATH_API
pmath_t pmath_session_execute(pmath_t input, pmath_bool_t *aborted) {
  pmath_t output;
  pmath_bool_t did_abort = FALSE;
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // Increment($Line)
  pmath_unref(pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_Increment), 1,
                  pmath_ref(pmath_System_DollarLine))));
                  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  output = pmath_evaluate(input);
  
  if(pmath_aborting()) {
    pmath_unref(output);
    output = pmath_ref(pmath_System_DollarAborted);
  }
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // $History($Line)::= output  [output is already evaluated]
  pmath_unref(pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_AssignDelayed), 2,
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_DollarHistory), 1,
                    pmath_ref(pmath_System_DollarLine)),
                  pmath_ref(output))));
                  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  // $History($Line - $HistoryLength):= .
  pmath_unref(pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_Unassign), 1,
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_DollarHistory), 1,
                    pmath_expr_new_extended(
                      pmath_ref(pmath_System_Plus), 2,
                      pmath_ref(pmath_System_DollarLine),
                      pmath_expr_new_extended(
                        pmath_ref(pmath_System_Times), 2,
                        PMATH_FROM_INT32(-1),
                        pmath_ref(pmath_System_DollarHistoryLength)))))));
                        
  pmath_collect_temporary_symbols();
  
  warn_uncaught_exception();
  did_abort = pmath_continue_after_abort() || did_abort;
  
  if(aborted)
    *aborted = did_abort;
    
  return output;
}

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_session_start(void) {
  pmath_t old_dlvl = pmath_evaluate(
                       pmath_parse_string(PMATH_C_STRING("$DialogLevel++")));
                       
  pmath_t old_line = pmath_evaluate(pmath_ref(pmath_System_DollarLine));
  
  pmath_t old_msgcnt = pmath_evaluate(
                         pmath_parse_string(PMATH_C_STRING("DownRules($MessageCount)")));
                         
  if(pmath_aborting()) {
    pmath_unref(old_dlvl);
    pmath_unref(old_line);
    pmath_unref(old_msgcnt);
    return PMATH_NULL;
  }
  
  _pmath_clear(pmath_System_DollarMessageCount, PMATH_CLEAR_BASIC_RULES);
  return pmath_build_value("(ooo)", old_dlvl, old_line, old_msgcnt);
}

PMATH_API
void pmath_session_end(pmath_t old_state) {
  if(pmath_is_expr_of_len(old_state, pmath_System_List, 3)) {
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
