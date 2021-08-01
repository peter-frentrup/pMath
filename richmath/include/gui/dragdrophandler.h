#ifndef RICHMATH__GUI__DRAGDROPHANDLER_H__INCLUDED
#define RICHMATH__GUI__DRAGDROPHANDLER_H__INCLUDED

#include <util/base.h>
#include <util/sharedptr.h>
#include <gui/menus.h>

namespace richmath {
  enum class DropAction {
    Copy,
    Move,
    Link,
  };
  
  class DragDropHandler: public Shareable {
    private:
      class Impl;
    public:
      DragDropHandler();
      virtual ~DragDropHandler();
      
      virtual MenuCommandStatus can_drop(DropAction action) = 0;
      virtual bool do_drop(DropAction action) = 0;
  };
}

#endif // RICHMATH__GUI__DRAGDROPHANDLER_H__INCLUDED
