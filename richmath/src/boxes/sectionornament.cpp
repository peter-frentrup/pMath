#include <boxes/sectionornament.h>

#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>


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

bool SectionOrnament::reload_if_necessary(Expr expr, BoxInputFlags flags) {
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
  
  if(expr.is_string() || expr[0] == richmath_System_List) {
    auto seq = new TextSequence();
    seq->load_from_object(expr, flags);
    _box = seq;
  }
  else {
    // TODO: use create_box() from mathsequence.cpp directly instead of wrapping int MathSequence
    auto seq = new MathSequence();
    seq->load_from_object(expr, flags);
    _box = seq;
  }
  
  return true;
}

//} ... class SectionOrnament
