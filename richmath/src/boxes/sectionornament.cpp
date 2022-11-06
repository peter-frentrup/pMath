#include <boxes/sectionornament.h>

#include <boxes/box-factory.h>
#include <boxes/errorbox.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;

//{ class SectionOrnament ...

SectionOrnament::SectionOrnament()
  : _box(nullptr)
{
}

SectionOrnament::~SectionOrnament() {
  if(_box)
    _box->safe_destroy();
}

bool SectionOrnament::reload_if_necessary(BoxAdopter owner, Expr expr, BoxInputFlags flags) {
  if(expr == _expr)
    return false;
  
  _expr = expr;
  if(expr.is_null() || expr == richmath_System_None) {
    if(!_box)
      return false;
    
    _box->safe_destroy();
    _box = nullptr;
    return true;
  }
  
  if(_box) {
    if(_box->try_load_from_object(expr, flags))  // TODO: call after_insertion
      return true;
  
    _box->safe_destroy();
    _box = nullptr;
  }
  
  _box = BoxFactory::create_empty_box(LayoutKind::Text, expr);
  owner.adopt(_box, 1);
  if(!_box->try_load_from_object(expr, flags)) {
    _box->safe_destroy();
    _box = new ErrorBox(expr);
    owner.adopt(_box, 1);
  }
  
  return true;
}

//} ... class SectionOrnament
