#include <util/selections.h>

#include <boxes/box.h>


using namespace richmath;

//{ class LocationReference ...

LocationReference::LocationReference()
  : id(FrontEndReference::None),
    index(0)
{
}

LocationReference::LocationReference(Box *_box, int _index)
  : id(_box ? _box->id() : FrontEndReference::None),
    index(_box ? _index : 0)
{
}

LocationReference::LocationReference(FrontEndReference _id, int _index)
  : id(_id),
    index(_index)
{
}

Box *LocationReference::get() {
  if(!id)
    return nullptr;
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return nullptr;
    
  if(index > result->length())
    index = result->length();
  
  return result;
}

void LocationReference::set_raw(Box *box, int _index) {
  if(box) {
    id = box->id();
    index = _index;
  }
  else {
    id = FrontEndReference::None;
    index = 0;
  }
}

bool LocationReference::equals(Box *box, int _index) const {
  if(!box)
    return !id.is_valid();
    
  return id == box->id() && index == _index;
}

//} ... class LocationReference

//{ class SelectionReference ...

SelectionReference::SelectionReference()
  : id(FrontEndReference::None),
    start(0),
    end(0)
{
}

SelectionReference::SelectionReference(Box *_box, int _start, int _end)
  : id(_box ? _box->id() : FrontEndReference::None),
    start(_box ? _start : 0),
    end(_box ? _end : 0)
{
}

SelectionReference::SelectionReference(FrontEndReference _id, int _start, int _end)
  : id(_id),
    start(_start),
    end(_end)
{
}

Box *SelectionReference::get() {
  if(!id)
    return nullptr;
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return nullptr;
    
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
    id = FrontEndReference::None;
}

bool SelectionReference::equals(Box *box, int _start, int _end) const {
  if(!box)
    return !id.is_valid();
    
  SelectionReference other;
  other.set(box, _start, _end);
  return equals(other);
}

bool SelectionReference::equals(const SelectionReference &other) const {
  return other.id == id && other.start == start && other.end == end;
}

Expr SelectionReference::to_debug_info() const {
  if(id == FrontEndReference::None)
    return Expr();
  
  return Call(
           Symbol(PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE),
           id.to_pmath(),
           Call(Symbol(PMATH_SYMBOL_RANGE), start, end));
}

SelectionReference SelectionReference::from_debug_info(Expr expr) {
  SelectionReference result;
  
  if(expr[0] != PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE)
    return result;
  
  if(expr.expr_length() != 2)
    return result;
  
  result.id = FrontEndReference::from_pmath(expr[1]);
  Expr range = expr[2];
  if( range.expr_length() == 2 && 
      range[0] == PMATH_SYMBOL_RANGE &&
      range[1].is_int32() &&
      range[2].is_int32())
  {
    result.start = PMATH_AS_INT32(range[1].get());
    result.end   = PMATH_AS_INT32(range[2].get());
    
    return result;
  }
  
  result.id = FrontEndReference::None;
  return result; 
}

SelectionReference SelectionReference::from_debug_info_of(Expr expr) {
  return from_debug_info(Expr{pmath_get_debug_info(expr.get())});
}

SelectionReference SelectionReference::from_debug_info_of(pmath_t expr) {
  return from_debug_info(Expr{pmath_get_debug_info(expr)});
}

//} ... class SelectionReference
