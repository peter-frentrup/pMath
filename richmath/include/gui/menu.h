#ifndef __GUI__MENU_H__
#define __GUI__MENU_H__

#include <gui/document.h>

namespace richmath{
  class Menu: public Document{
    public:
      Menu();
      virtual ~Menu();
      
      virtual Box *move_logical(
        LogicalDirection  direction, 
        bool              jumping, 
        int              *index);
      
      virtual Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
        
      virtual Box *mouse_selection(
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *eol);
      
      virtual Box *normalize_selection(int *start, int *end);
  };
};

#endif // __GUI__MENU_H__
