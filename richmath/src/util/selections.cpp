#include <util/selections.h>

#include <boxes/box.h>


using namespace richmath;

//{ class SelectionReference ...

SelectionReference::SelectionReference(int _id, int _start, int _end)
  : id(_id),
    start(_start),
    end(_end)
{
}

Box *SelectionReference::get() {
  if(!id)
    return 0;
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return 0;
    
  if(start > result->length())
    start = result->length();
    
  if(end > result->length())
    end = result->length();
    
  return result;
}

void SelectionReference::set(Box *box, int _start, int _end) {
  if(box)
    box = box->normalize_selection(&_start, &_end);
    
  set_raw(box, _start, _end);
}

void SelectionReference::set_raw(Box *box, int _start, int _end) {
  if(box) {
    id = box->id();
    start = _start;
    end = _end;
  }
  else
    id = 0;
}

bool SelectionReference::equals(Box *box, int _start, int _end) const {
  if(!box)
    return id == 0;
    
  SelectionReference other;
  other.set(box, _start, _end);
  return equals(other);
}

bool SelectionReference::equals(const SelectionReference &other) const {
  return other.id == id && other.start == start && other.end == end;
}

//} ... class SelectionReference
