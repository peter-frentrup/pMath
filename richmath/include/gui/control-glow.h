#ifndef RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED
#define RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED

#include <gui/control-painter.h>
#include <graphics/margins.h>
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
      Margins<float> outside;
      Margins<float> inside;
  };
}

#endif // RICHMATH__GUI__CONTROL_GLOW_H__INCLUDED
