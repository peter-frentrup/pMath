#ifndef RICHMATH__GUI__EVENT_HANDLERS_H__INCLUDED
#define RICHMATH__GUI__EVENT_HANDLERS_H__INCLUDED

#include <boxes/box.h>
#include <util/pmath-extra.h>
#include <util/styled-object.h>

namespace richmath {
  enum class EventHandlerResult: uint8_t {
    Continue,
    StopPropagation,
  };
  
  class EventHandlers {
      class Impl;
    public:
      static String key_name(SpecialKey key);
      
      static EventHandlerResult decode_handler_result(Expr result);
      
      static EventHandlerResult execute_key_down_handler(FrontEndObject *obj, const SpecialKeyEvent &event, Expr handler);
      static EventHandlerResult execute_key_down_handler(StyledObject *obj, ObjectStyleOptionName handlers, const SpecialKeyEvent &event);

      static EventHandlerResult execute_key_press_handler(FrontEndObject *obj, uint32_t ch, Expr handler);
      static EventHandlerResult execute_key_press_handler(StyledObject *obj, ObjectStyleOptionName handlers, uint32_t ch);
  };
}

#endif // RICHMATH__GUI__EVENT_HANDLERS_H__INCLUDED
