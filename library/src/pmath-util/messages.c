#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/concurrency/threads.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-builtins/control/messages-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>

PMATH_API void pmath_message(
  pmath_symbol_t symbol,
  const char *tag,
  size_t argcount,
  ...
){
  pmath_expr_t expr;
  pmath_thread_t current_thread = pmath_thread_get_current();
  size_t i;
  va_list items;

  va_start(items, argcount);

  if(!current_thread){
    for(i = 0;i < argcount;i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    return;
  }
 
  // expr = Message(symbol::tag, ...)
  expr = pmath_expr_new(
    pmath_ref(PMATH_SYMBOL_MESSAGE), 1+argcount);

  symbol = pmath_ref(symbol);

  if(!symbol){
    pmath_t head = pmath_current_head();
    symbol = _pmath_topmost_symbol(head);
    pmath_unref(head);
  }

  expr = pmath_expr_set_item(
    expr, 1,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
      symbol,
      PMATH_C_STRING(tag)));

  for(i = 0;i < argcount;i++){
    pmath_t item = va_arg(items, pmath_t);
    if(!pmath_is_evaluated(item))
      item = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_HOLDFORM), 1,
        item);

    expr = pmath_expr_set_item(expr, 2+i, item);
  }

  va_end(items);

  pmath_unref(pmath_evaluate(expr));
}

PMATH_API void pmath_message_argxxx(size_t given, size_t min, size_t max){
  pmath_symbol_t head;

  if(max < min)
    max = min;

  if(given == 1){
    if(min == max){
      head = pmath_current_head();
      pmath_message(
        NULL, "argxu", 2, 
        head,
        pmath_integer_new_size(min));
    }
    else if(min + 1 == max){
      head = pmath_current_head();
      pmath_message(
        NULL, "argtu", 3,
        head,
        pmath_integer_new_size(min),
        pmath_integer_new_size(max));
    }
    else if(max == SIZE_MAX){
      head = pmath_current_head();
      pmath_message(
        NULL, "argmu", 2,
        head,
        pmath_integer_new_size(min));
    }
    else{
      head = pmath_current_head();
      pmath_message(
        NULL, "argru", 3,
        head,
        pmath_integer_new_size(min),
        pmath_integer_new_size(max));
    }
  }
  else{
    if(min == max){
      if(min == 1){
        head = pmath_current_head();
        pmath_message(
          NULL, "arg1", 2,
          head,
          pmath_integer_new_size(given));
      }
      else{
        head = pmath_current_head();
        pmath_message(
          NULL, "argx", 3,
          head,
          pmath_integer_new_size(given),
          pmath_integer_new_size(min));
      }
    }
    else if(min + 1 == max){
      head = pmath_current_head();
      pmath_message(
        NULL, "argt", 4,
        head,
        pmath_integer_new_size(given),
        pmath_integer_new_size(min),
        pmath_integer_new_size(max));
    }
    else if(max == SIZE_MAX){
      head = pmath_current_head();
      pmath_message(
        NULL, "argm", 3,
        head,
        pmath_integer_new_size(given),
        pmath_integer_new_size(min));
    }
    else{
      head = pmath_current_head();
      pmath_message(
        NULL, "argr", 4,
        head,
        pmath_integer_new_size(given),
        pmath_integer_new_size(min),
        pmath_integer_new_size(max));
    }
  }
}

PMATH_API pmath_string_t pmath_message_find_text(pmath_t name){
  struct _pmath_symbol_rules_t  *rules;
  struct _pmath_object_entry_t  *entry;
  pmath_hashtable_t              messages;
  pmath_symbol_t                 sym;
  int                            loop;
  
  if(!_pmath_is_valid_messagename(name)){
    pmath_message(PMATH_SYMBOL_MESSAGE, "name", 1, name);
    return PMATH_UNDEFINED;
  }
  
  sym = pmath_expr_get_item(name, 1);
  assert(pmath_instance_of(sym, PMATH_TYPE_SYMBOL));
  
  for(loop = 0;loop < 2;++loop){
    rules = _pmath_symbol_get_rules(sym, RULES_READ);
    
    if(rules){
      pmath_t obj = NULL;
      messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
      
      entry = pmath_ht_search(messages, name);
      if(entry)
        obj = pmath_ref(entry->value);
      
      _pmath_atomic_unlock_ptr(&rules->_messages, messages);
      
      if(obj){
        pmath_unref(name);
        pmath_unref(sym);
        return obj;
      }
    }
    
    if(sym != PMATH_SYMBOL_GENERAL){
      pmath_t obj = NULL;
      rules = _pmath_symbol_get_rules(PMATH_SYMBOL_GENERAL, RULES_READ);
      
      if(rules){
        name = pmath_expr_set_item(name, 1, pmath_ref(PMATH_SYMBOL_GENERAL));
        
        messages = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&rules->_messages);
        
        entry = pmath_ht_search(messages, name);
        if(entry)
          obj = pmath_ref(entry->value);
        
        _pmath_atomic_unlock_ptr(&rules->_messages, messages);
        
        if(obj){
          pmath_unref(name);
          pmath_unref(sym);
          return obj;
        }
      }
    }
    
    if(loop == 0){
      pmath_t value = pmath_symbol_get_value(PMATH_SYMBOL_NEWMESSAGE);
      
      if(value == PMATH_UNDEFINED)
        break;
      
      pmath_unref(pmath_evaluate(
        pmath_expr_new_extended(
          value, 2,
          pmath_ref(sym),
          pmath_expr_get_item(name, 2))));
    }
  }
  
  pmath_unref(name);
  pmath_unref(sym);
  return NULL;
}

PMATH_API void pmath_message_syntax_error(
  pmath_string_t  code, // wont be freed
  int             position,
  pmath_string_t  filename,
  int             lines_before_code
){
  int len = pmath_string_length(code);
  const uint16_t *str = pmath_string_buffer(code);
  
  if(position == 0){
    pmath_string_t start;
    
    int eol = position;
    while(eol < len && str[eol] != '\n')
      ++eol;
    
    start = pmath_string_part(pmath_ref(code), position, eol - position);
    if(filename){
      pmath_message(
        PMATH_SYMBOL_SYNTAX, "bgnf", 3,
        start,
        pmath_integer_new_si(lines_before_code),
        filename);
    }
    else{
      pmath_message(
        PMATH_SYMBOL_SYNTAX, "bgn", 1,
        start);
    }
  }
  else{
    if(filename){
      int i;
      for(i = 0;i < position;++i)
        if(str[i] == '\n')
          ++lines_before_code;
    }
    
    if(position == len){
      if(filename){
        pmath_message(PMATH_SYMBOL_SYNTAX, "moref", 2, 
          pmath_integer_new_si(lines_before_code),
          filename);
      }
      else{
        pmath_message(PMATH_SYMBOL_SYNTAX, "more", 0);
      }
    }
    else{
      pmath_string_t before;
      int eol1, eol, bol;

      eol1 = position;
      while(eol1 > 0 && str[eol1] == '\n')
        --eol1;

      bol = eol1;
      while(bol > 0 && str[bol] != '\n')
        --bol;
      while(bol <= position && str[bol] <= ' ')
        ++bol;

      eol = position;
      while(eol < len && str[eol] != '\n')
        ++eol;
      
      before = pmath_string_part(pmath_ref(code), bol, eol1 - bol); // eol1-bol+1
      if(str[position] == '\n'){
        if(filename){
          pmath_message(
            PMATH_SYMBOL_SYNTAX, "newlf", 3,
            before,
            pmath_integer_new_si(lines_before_code),
            filename);
        }
        else{
          pmath_message(
            PMATH_SYMBOL_SYNTAX, "newl", 1,
            before);
        }
      }
      else{
        pmath_string_t after  = pmath_string_part(pmath_ref(code), position, eol - position); // pos+1, eol-pos-1
        
        if(filename){
          pmath_message(
            PMATH_SYMBOL_SYNTAX, "nxtf", 4,
            before,
            after,
            pmath_integer_new_si(lines_before_code),
            filename);
        }
        else{
          pmath_message(
            PMATH_SYMBOL_SYNTAX, "nxt", 2,
            before,
            after);
        }
      }
    }
  }
}
