#ifndef RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED
#define RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED

#include <gui/control-painter.h>
#include <graphics/paint-hook.h>

namespace richmath {
  class ControlGlowHook : public PaintHook {
    public:
      ControlGlowHook(Box *destination, ContainerType type, ControlState state);
      
      virtual void run(Box *box, Context &context) override;
      
    public:
      static bool all_disabled; 
      
    public:
      Box           *destination;
      ContainerType  type;
      ControlState   state;
      float          outside_margin_left;
      float          outside_margin_right;
      float          outside_margin_top;
      float          outside_margin_bottom;
      float          inside_margin_left;
      float          inside_margin_right;
      float          inside_margin_top;
      float          inside_margin_bottom;
  };
}

#endif // RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED
