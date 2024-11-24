#include <gui/event-handlers.h>

#include <eval/application.h>
#include <eval/eval-contexts.h>


namespace richmath { namespace strings {
  extern String Backspace;
  extern String Delete;
  extern String DownArrow;
  extern String End;
  extern String Escape;
  extern String F1;
  extern String F2;
  extern String F3;
  extern String F4;
  extern String F5;
  extern String F6;
  extern String F7;
  extern String F8;
  extern String F9;
  extern String F10;
  extern String F11;
  extern String F12;
  extern String Home;
  extern String KeyDown;
  extern String LeftArrow;
  extern String PageDown;
  extern String PageUp;
  extern String Return;
  extern String RightArrow;
  extern String Tab;
  extern String UpArrow;
}}

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Break;
extern pmath_symbol_t richmath_System_Continue;
extern pmath_symbol_t richmath_System_None;

namespace richmath {
  class EventHandlers::Impl {
    public:
      static Expr prepare_handler_function(StyledObject *obj, Expr handler);
      static Expr prepare_handler_function(StyledObject *obj, Expr handler, Expr arg);
      
      static Expr find_key_down_handler(StyledObject *obj, ObjectStyleOptionName handlers, const SpecialKeyEvent &event);
      static Expr find_key_down_handler(StyledObject *obj, Expr                  handlers, const SpecialKeyEvent &event);
     
      static Expr find_key_press_handler(StyledObject *obj, ObjectStyleOptionName handlers, uint32_t ch);
      static Expr find_key_press_handler(StyledObject *obj, Expr                  handlers, uint32_t ch);
  };
}

using namespace richmath;

//{ class EventHandlers ...

String EventHandlers::key_name(SpecialKey key) {
  switch(key) {
    case SpecialKey::Unknown:     break;
    case SpecialKey::Left:        return strings::LeftArrow;
    case SpecialKey::Right:       return strings::RightArrow;
    case SpecialKey::Up:          return strings::UpArrow;
    case SpecialKey::Down:        return strings::DownArrow;
    case SpecialKey::Home:        return strings::Home;
    case SpecialKey::End:         return strings::End;
    case SpecialKey::PageUp:      return strings::PageUp;
    case SpecialKey::PageDown:    return strings::PageDown;
    case SpecialKey::Backspace:   return strings::Backspace;
    case SpecialKey::Delete:      return strings::Delete;
    case SpecialKey::Return:      return strings::Return;
    case SpecialKey::Tab:         return strings::Tab;
    case SpecialKey::Escape:      return strings::Escape;
    case SpecialKey::F1:          return strings::F1;
    case SpecialKey::F2:          return strings::F2;
    case SpecialKey::F3:          return strings::F3;
    case SpecialKey::F4:          return strings::F4;
    case SpecialKey::F5:          return strings::F5;
    case SpecialKey::F6:          return strings::F6;
    case SpecialKey::F7:          return strings::F7;
    case SpecialKey::F8:          return strings::F8;
    case SpecialKey::F9:          return strings::F9;
    case SpecialKey::F10:         return strings::F10;
    case SpecialKey::F11:         return strings::F11;
    case SpecialKey::F12:         return strings::F12;
  }
  return String();
}

EventHandlerResult EventHandlers::decode_handler_result(Expr result) {
  if(result.item_equals(0, richmath_System_Break))
    return EventHandlerResult::StopPropagation;
  
  return EventHandlerResult::Continue;
}

EventHandlerResult EventHandlers::execute_key_down_handler(FrontEndObject *obj, const SpecialKeyEvent &event, Expr handler) {
  if(!handler)
    return EventHandlerResult::Continue;
  
  if(handler.expr_length() == 0) {
    if(handler.item_equals(0, richmath_System_Break))
      return EventHandlerResult::StopPropagation;
    if(handler.item_equals(0, richmath_System_Continue))
      return EventHandlerResult::Continue;
  }
  
  // TODO: set up CurrentValue("ModifierKeys"), CurrentValue("EventKey")
  //       or is this already done by the windowing system ?
  return decode_handler_result(
           Application::interrupt_wait_for_interactive(
             PMATH_CPP_MOVE(handler), obj, Application::button_timeout));
}

EventHandlerResult EventHandlers::execute_key_down_handler(StyledObject *obj, ObjectStyleOptionName handlers, const SpecialKeyEvent &event) {
  return execute_key_down_handler(obj, event, Impl::find_key_down_handler(obj, handlers, event));
}

EventHandlerResult EventHandlers::execute_key_press_handler(FrontEndObject *obj, uint32_t ch, Expr handler) {
  if(!handler)
    return EventHandlerResult::Continue;
  
  if(handler.expr_length() == 0) {
    if(handler.item_equals(0, richmath_System_Break))
      return EventHandlerResult::StopPropagation;
    if(handler.item_equals(0, richmath_System_Continue))
      return EventHandlerResult::Continue;
  }
  
  // TODO: set up CurrentValue("ModifierKeys"), CurrentValue("EventKey")
  //       or is this already done by the windowing system ?
  return decode_handler_result(
           Application::interrupt_wait_for_interactive(
             PMATH_CPP_MOVE(handler), obj, Application::button_timeout));
}

EventHandlerResult EventHandlers::execute_key_press_handler(StyledObject *obj, ObjectStyleOptionName handlers, uint32_t ch) {
  return execute_key_press_handler(obj, ch, Impl::find_key_press_handler(obj, handlers, ch));
}

//} ... class EventHandlers

//{ class EventHandlers::Impl ...

Expr EventHandlers::Impl::prepare_handler_function(StyledObject *obj, Expr handler) {
  if(!handler)
    return handler;
  
  if(handler == richmath_System_None)
    return Call(Symbol(richmath_System_Break));
    
  if(handler == richmath_System_Automatic)
    return Call(Symbol(richmath_System_Continue));
  
  return EvaluationContexts::prepare_namespace_for(obj->prepare_dynamic(Call(PMATH_CPP_MOVE(handler))), obj);
}

Expr EventHandlers::Impl::prepare_handler_function(StyledObject *obj, Expr handler, Expr arg) {
  if(!handler)
    return handler;
  
  if(handler == richmath_System_None)
    return Call(Symbol(richmath_System_Break));
    
  if(handler == richmath_System_Automatic)
    return Call(Symbol(richmath_System_Continue));
  
  return EvaluationContexts::prepare_namespace_for(obj->prepare_dynamic(Call(PMATH_CPP_MOVE(handler), PMATH_CPP_MOVE(arg))), obj);
}

Expr EventHandlers::Impl::find_key_down_handler(StyledObject *obj, ObjectStyleOptionName handlers, const SpecialKeyEvent &event) {
  if(!obj)
    return Expr();
  
  return find_key_down_handler(obj, obj->get_own_style(handlers, Expr()), event);
}

Expr EventHandlers::Impl::find_key_down_handler(StyledObject *obj, Expr handlers, const SpecialKeyEvent &event) {
  if(!obj || handlers.is_null())
    return Expr();
  
  if(event.key == SpecialKey::Unknown)
    return Expr();
  
  String key_name = EventHandlers::key_name(event.key);
  Expr rhs;
  if(!handlers.try_lookup(key_name + "KeyDown", rhs)) {
    if(!handlers.try_lookup(strings::KeyDown, rhs))
      return Expr();
  }
  
  return prepare_handler_function(obj, PMATH_CPP_MOVE(rhs), PMATH_CPP_MOVE(key_name));
}

Expr EventHandlers::Impl::find_key_press_handler(StyledObject *obj, ObjectStyleOptionName handlers, uint32_t ch) {
  if(!obj)
    return Expr();
  
  return find_key_press_handler(obj, obj->get_own_style(handlers, Expr()), ch);
}

Expr EventHandlers::Impl::find_key_press_handler(StyledObject *obj, Expr handlers, uint32_t ch) {
  if(!obj || handlers.is_null())
    return Expr();
  
  if(ch < ' ')
    return Expr();
  
  String char_str = String::FromChar(ch);
  Expr rhs;
  if(!handlers.try_lookup(List(strings::KeyDown, char_str), rhs)) {
    if(!handlers.try_lookup(strings::KeyDown, rhs))
      return Expr();
  }
  
  return prepare_handler_function(obj, PMATH_CPP_MOVE(rhs), PMATH_CPP_MOVE(char_str));
}

//} ... class EventHandlers::Impl
