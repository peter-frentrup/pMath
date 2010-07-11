#include <gui/menu.h>

#include <boxes/section.h>

using namespace richmath;

//{ class Menu ...

Menu::Menu()
: Document()
{
  border_visible = false;
}

Menu::~Menu(){
  
}

Box *Menu::move_logical(
  LogicalDirection  direction, 
  bool              jumping, 
  int              *index
){
  if(direction == Forward){
    if(*index + 1 < length())
      ++*index;
    
    return this;
  }
  
  if(*index > 0)
    --*index;
  
  return this;
}

Box *Menu::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  return move_logical(direction, true, index);
}
  
Box *Menu::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  *start = 0;
  while(*start < length()){
    if(y <= section(*start)->y_offset + section(*start)->extents().height())
      break;
    
    ++*start;
  }
  
  if(length() == 0){
    *end = *start;
  }
  else
    *end = *start + 1;
  
  return this;
}
      
Box *Menu::normalize_selection(int *start, int *end){
  if(*start == *end){
    if(*start >= length()){
      if(*start == 0)
        return 0;
      
      --*start;
      return this;
    }
    
    ++*end;
    return this;
  }
  
  if(*start + 1 != *end){
    *start = *end - 1;
  }
  
  return this;
}

//} ... class Menu
