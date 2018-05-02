#include <boxes/templatebox.h>
#include <boxes/mathsequence.h>

#include <eval/application.h>


using namespace richmath;
using namespace pmath;

namespace richmath {
  class TemplateBoxImpl {
    public:
      TemplateBoxImpl(TemplateBox &_self);
      
      void load_content(Expr dispfun);
      Expr display_function_body(Expr dispfun);
      
    private:
      TemplateBox &self;
  };
  
  class TemplateBoxSlotImpl {
    public:
      TemplateBoxSlotImpl(TemplateBoxSlot &_self);
      
      static TemplateBox *find_owner(Box *box);
      
      void reload_content();
      Expr get_content();
      
      void assign_content();
      
      static Expr prepare_boxes(Expr boxes);
      
    private:
      static Expr prepare_pure_arg(Expr expr);
      static Expr prepare_map(Expr expr);
      static Expr prepare_riffle(Expr expr);
      
    private:
      TemplateBoxSlot &self;
  };
  
  class TemplateBoxSlotSequenceImpl {
    public:
      TemplateBoxSlotSequenceImpl(TemplateBoxSlotSequence &_self);
      
      void reload_content();
      Expr get_content();
      Expr get_single_mapped_argument(size_t i);
      
    private:
      TemplateBoxSlotSequence &self;
  };
}

static int get_box_item_number(Box *parent, Box *box) { // -1 on error
  assert(parent);
  assert(box);
  
  int count = parent->count();
  if(count == parent->length()) {
    int i = box->index();
    if(/*i >= 0 && i < count && */parent->item(i) == box)
      return i;
    return -1;
  }
  
  for(int i = 0; i < count; ++i) {
    if(parent->item(i) == box)
      return i;
  }
  return -1;
}

static Box *next_box_outside(Box *box, LogicalDirection direction, Box *stop_parent) {
  int delta = (direction == LogicalDirection::Forward) ? +1 : -1;
  
  while(box && box != stop_parent) {
    Box *parent = box->parent();
    if(!parent)
      return nullptr;
      
    int num = get_box_item_number(parent, box);
    if(num < 0)
      return nullptr;
      
    num += delta;
    if(0 <= num && num < parent->count())
      return parent->item(num);
      
    box = parent;
  }
  return nullptr;
}

static Box *next_box(Box *box, LogicalDirection direction, Box *stop_parent) {
  if(!box)
    return nullptr;
    
  int count = box->count();
  if(count > 0) {
    if(direction == LogicalDirection::Forward)
      return box->item(0);
    else
      return box->item(count - 1);
  }
  
  return next_box_outside(box, direction, stop_parent);
}

template<typename T>
static T *search_box(Box *start, LogicalDirection direction, Box *stop_parent) {
  while(start) {
    if(T *result = dynamic_cast<T*>(start))
      return result;
      
    start = next_box(start, direction, stop_parent);
  }
  return nullptr;
}

template<typename T>
static T *search_next_box(Box *start, LogicalDirection direction, Box *stop_parent) {
  return search_box<T>(next_box(start, direction, stop_parent), direction, stop_parent);
}

//{ class TemplateBox ...

TemplateBox::TemplateBox()
  : base(),
    _is_content_loaded(false)
{
  style = new Style();
}

bool TemplateBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != PMATH_SYMBOL_TEMPLATEBOX)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr args = expr[1];
  if(args[0] != PMATH_SYMBOL_LIST)
    return false;
    
  Expr tag = expr[2];
  Expr options = Expr(pmath_options_extract(expr.get(), 2));
  if(options.is_null())
    return false;
    
  arguments = args;
  _tag = tag;
  _is_content_loaded = false;
  style->clear();
  style->add_pmath(options);
  style->set_pmath(BaseStyleName, tag);
  return true;
}

bool TemplateBox::selectable(int i) {
  if(i >= 0)
    return false;
  
  return base::selectable(i);
}

Box *TemplateBox::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(*index < 0 || *index > length()) {
    TemplateBoxSlot *slot = search_next_box<TemplateBoxSlot>(this, direction, this);
    while(slot) {
      if(slot->find_owner() == this) {
        if(direction == LogicalDirection::Forward)
          *index = -1;
        else
          *index = slot->length() + 1;
        
        return slot->move_logical(direction, false, index);
      }
      slot = search_next_box<TemplateBoxSlot>(slot, direction, this);
    }
    
    if(!_parent) {
      *index = 0;
      return this;
    }
    
    if(direction == LogicalDirection::Forward) {
      *index = _index;
      return _parent->move_logical(direction, true, index);
    }
    else {
      *index = _index + 1;
      return _parent->move_logical(direction, true, index);
    }
  }
  
  return base::move_logical(direction, jumping, index);
}

void TemplateBox::resize(Context *context) {
  base::resize(context);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void TemplateBox::paint_content(Context *context) {
//  if(must_resize) {
//    context->canvas->save();
//    base::resize(context);
//    must_resize = false;
//    context->canvas->restore();
//  }

  base::paint_content(context);
  
  if(!_is_content_loaded) {
    Expr dispfun = get_own_style(DisplayFunction);
    TemplateBoxImpl(*this).load_content(dispfun);
    _is_content_loaded = true;
    invalidate();
  }
}

Expr TemplateBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(arguments);
  g.emit(_tag);
  
  style->emit_to_pmath(false);
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_TEMPLATEBOX));
  return e;
}

//} ... class TemplateBox

//{ class TemplateBoxSlot ...

TemplateBoxSlot::TemplateBoxSlot()
  : base(),
    _argument(0),
    _is_content_loaded(false),
    _has_changed_content(false)
{
}

TemplateBox *TemplateBoxSlot::find_owner() {
  return TemplateBoxSlotImpl::find_owner(this);
}

Expr TemplateBoxSlot::prepare_boxes(Expr boxes) {
  return TemplateBoxSlotImpl::prepare_boxes(boxes);
}

bool TemplateBoxSlot::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != PMATH_SYMBOL_TEMPLATESLOT || expr.expr_length() != 1)
    return false;
    
  Expr arg = expr[1];
  if(arg.is_int32()) {
    _argument = PMATH_AS_INT32(arg.get());
    _is_content_loaded = false;
    return true;
  }
  
  return false;
}

bool TemplateBoxSlot::selectable(int i) {
  if(i >= 0) {
    TemplateBox *owner = find_owner();
    if(owner) {
      if(!owner->selectable())
        return false;
    }
    return true;
  }
  
  return base::selectable(i);
}

Box *TemplateBoxSlot::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(*index < 0 || *index > length()) 
    return base::move_logical(direction, jumping, index);
  
  TemplateBox *owner = find_owner();
  if(!owner)
    return base::move_logical(direction, jumping, index);
  
  TemplateBoxSlot *next_slot = search_next_box<TemplateBoxSlot>(this, direction, owner);
  while(next_slot) {
    if(next_slot->find_owner() == owner) {
      if(direction == LogicalDirection::Forward)
        *index = -1;
      else
        *index = next_slot->length() + 1;
      return next_slot->move_logical(direction, false, index);
    }
    
    next_slot = search_next_box<TemplateBoxSlot>(next_slot, direction, owner);
  }
  
  *index = 0;
  return owner->move_logical(direction, true, index);
}

Box *TemplateBoxSlot::remove(int *index) {
  TemplateBox *owner = find_owner();
  if(!owner) 
    return base::remove(index);
  
  if(content()->length() == 0) 
    content()->insert(0, PMATH_CHAR_PLACEHOLDER);
  
  TemplateBoxSlot *prev = search_next_box<TemplateBoxSlot>(this, LogicalDirection::Backward, owner);
  while(prev && prev->find_owner() != owner)
    prev = search_next_box<TemplateBoxSlot>(prev, LogicalDirection::Backward, owner);
  
  if(prev) {
    *index = prev->content()->length();
    return prev->content();
  }
  
  if(!owner->parent()) {
    *index = 0;
    return owner;
  }
  
  bool all_empty_or_placeholders = true;
  
  TemplateBoxSlot *next = this;
  while(next) {
    if(next->find_owner() == owner) {
      if(next->content()->length() > 0 && !next->content()->is_placeholder()) {
        all_empty_or_placeholders = false;
        break;
      } 
    }
    next = search_next_box<TemplateBoxSlot>(next, LogicalDirection::Forward, owner);
  }
  
  if(all_empty_or_placeholders) {
    *index = owner->index();
    return owner->parent()->remove(index);
  }
  
  *index = owner->index();
  return owner->parent();
}

void TemplateBoxSlot::invalidate() {
  base::invalidate();
  
  if(_is_content_loaded)
    _has_changed_content = true;
}

void TemplateBoxSlot::resize(Context *context) {
  base::resize(context);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void TemplateBoxSlot::paint_content(Context *context) {
  base::paint_content(context);
  
  if(!_is_content_loaded) {
    invalidate();
    TemplateBoxSlotImpl(*this).reload_content();
  }
}

void TemplateBoxSlot::on_exit() {
  if(_has_changed_content)
    TemplateBoxSlotImpl(*this).assign_content();
}

void TemplateBoxSlot::on_finish_editing() {
  if(_has_changed_content)
    TemplateBoxSlotImpl(*this).assign_content();
}

//} ... class TemplateBoxSlot

//{ class TemplateBoxSlotSequence ...

TemplateBoxSlotSequence::TemplateBoxSlotSequence()
  : base(),
    _separator(Expr()),
    _modifier_function(Expr()),
    _first_arg(1),
    _last_arg(-1),
    _is_content_loaded(false)
{
}

TemplateBox *TemplateBoxSlotSequence::find_owner() {
  return TemplateBoxSlotImpl::find_owner(this);
}

bool TemplateBoxSlotSequence::try_load_from_object(Expr expr, BoxInputFlags opts) {
  // TemplateBoxSlotSequence(range [[, modifier], separator])
  size_t exprlen = expr.expr_length();
  if(expr[0] != PMATH_SYMBOL_TEMPLATESLOTSEQUENCE || exprlen < 1 || exprlen > 3)
    return false;
    
  _first_arg = 1;
  _last_arg = -1;
  Expr range = expr[1];
  if(range[0] == PMATH_SYMBOL_PUREARGUMENT && range.expr_length() == 1)
    range = range[1];
    
  if(range.is_int32()) {
    _first_arg = PMATH_AS_INT32(range.get());
  }
  else if(range[0] == PMATH_SYMBOL_RANGE) {
    if(range.expr_length() != 2)
      return false;
      
    Expr start = range[1];
    Expr end = range[2];
    
    if(start.is_int32())
      _first_arg = PMATH_AS_INT32(start.get());
    else if(start != PMATH_SYMBOL_AUTOMATIC)
      return false;
      
    if(end.is_int32())
      _last_arg = PMATH_AS_INT32(end.get());
    else if(end != PMATH_SYMBOL_AUTOMATIC)
      return false;
  }
  else
    return false;
    
  if(exprlen >= 2)
    _separator = expr[exprlen];
  else
    _separator = String(" ");
    
  if(exprlen >= 3) {
    _modifier_function = expr[2];
  }
  else
    _modifier_function = Expr();
    
  return true;
}

void TemplateBoxSlotSequence::resize(Context *context) {
  base::resize(context);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void TemplateBoxSlotSequence::paint_content(Context *context) {
  base::paint_content(context);
  
  if(!_is_content_loaded) {
    TemplateBoxSlotSequenceImpl(*this).reload_content();
    _is_content_loaded = true;
    invalidate();
  }
}

//} ... class TemplateBoxSlotSequence

//{ class TemplateBoxImpl ...

TemplateBoxImpl::TemplateBoxImpl(TemplateBox &_self)
  : self(_self)
{
}

void TemplateBoxImpl::load_content(Expr dispfun) {
  self.content()->load_from_object(
    display_function_body(dispfun),
    BoxInputFlags::AllowTemplateSlots | BoxInputFlags::FormatNumbers);
}

Expr TemplateBoxImpl::display_function_body(Expr dispfun) {
  if(dispfun[0] == PMATH_SYMBOL_FUNCTION && dispfun.expr_length() == 1)
    return dispfun[1];
    
  Expr head = self._tag.to_string();
  dispfun = Application::interrupt_wait(
              Parse("FE`Styles`$DefaultDisplayFunction(`1`)", head),
              Application::button_timeout);
              
  if(dispfun[0] == PMATH_SYMBOL_FUNCTION && dispfun.expr_length() == 1)
    return dispfun[1];
    
  return String("??");
}

//} ... class TemplateBoxImpl

//{ class TemplateBoxSlotImpl ...

TemplateBoxSlotImpl::TemplateBoxSlotImpl(TemplateBoxSlot &_self)
  : self(_self)
{
}

TemplateBox *TemplateBoxSlotImpl::find_owner(Box *box) {
  int nesting = 0;
  box = box->parent();
  while(box) {
    if(auto slot = dynamic_cast<TemplateBoxSlot*>(box)) {
      ++nesting;
    }
    else if(auto tb = dynamic_cast<TemplateBox*>(box)) {
      if(nesting-- == 0)
        return tb;
    }
    box = box->parent();
  }
  return nullptr;
}

void TemplateBoxSlotImpl::reload_content() {
  BoxInputFlags flags = BoxInputFlags::Default;
  
  TemplateBox *owner = self.find_owner();
  if(owner) {
    flags |= BoxInputFlags::FormatNumbers;
    if(owner->find_parent<TemplateBox>(false))
      flags |= BoxInputFlags::AllowTemplateSlots;
  }
  
  self.content()->load_from_object(get_content(), flags);
  self._is_content_loaded = true;
  self._has_changed_content = false;
}

Expr TemplateBoxSlotImpl::get_content() {
  if(self._argument < 1)
    return String::FromChar(PMATH_CHAR_PLACEHOLDER);
    
  TemplateBox *tb = self.find_owner();
  if(!tb)
    return String::FromChar(PMATH_CHAR_PLACEHOLDER);
    
  if(tb->arguments.expr_length() < (size_t)self._argument)
    return String::FromChar(PMATH_CHAR_PLACEHOLDER);
    
  return tb->arguments[self._argument];
}

void TemplateBoxSlotImpl::assign_content() {
  self._has_changed_content = false;
  
  TemplateBox *tb = self.find_owner();
  if(!tb)
    return;
  
  tb->arguments.set(self._argument, self.content()->to_pmath(BoxOutputFlags::Default));
  
  TemplateBoxSlot *slot = search_box<TemplateBoxSlot>(tb, LogicalDirection::Forward, tb);
  while(slot) {
    if(slot != &self && slot->_argument == self._argument && slot->find_owner() == tb) {
      //slot->_is_content_loaded = false;
      slot->invalidate();
      TemplateBoxSlotImpl(*slot).reload_content();
    }
    slot = search_next_box<TemplateBoxSlot>(slot, LogicalDirection::Forward, tb);
  }
}

Expr TemplateBoxSlotImpl::prepare_boxes(Expr boxes) {
  if(boxes[0] == PMATH_SYMBOL_PUREARGUMENT)
    return prepare_pure_arg(boxes);
    
//  if(boxes[0] == PMATH_SYMBOL_MAP)
//    return prepare_map(boxes);
//
//  if(boxes[0] == PMATH_SYMBOL_RIFFLE)
//    return prepare_riffle(boxes);

  if(boxes[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 0; i <= boxes.expr_length(); ++i) {
      boxes.set(i, prepare_boxes(boxes[i]));
    }
    return boxes;
  }
  
  return boxes;
}

Expr TemplateBoxSlotImpl::prepare_pure_arg(Expr expr) {
  if(expr.expr_length() != 1)
    return expr;
    
  Expr num = expr[1];
  if(num.is_integer())
    return Call(Symbol(PMATH_SYMBOL_TEMPLATESLOT), num);
    
  return Call(Symbol(PMATH_SYMBOL_TEMPLATESLOTSEQUENCE), expr);
}
/*
Expr TemplateBoxSlotImpl::prepare_map(Expr expr) {
  return expr;
}

Expr TemplateBoxSlotImpl::prepare_riffle(Expr expr) {
  if(expr.expr_length() != 2)
    return expr;

  Expr sep = expr[2];
  if(sep[0] == PMATH_SYMBOL_LIST) {
    if(sep.expr_length() != 1)
      return expr;

    sep = sep[1];
  }

  Expr inner = prepare_boxes(expr[1]);
  if(inner[0] == PMATH_SYMBOL_LIST && inner.expr_length() == 1)
    inner = inner[1];

  if(inner[0] == PMATH_SYMBOL_TEMPLATESLOTSEQUENCE && 1 <= inner.expr_length() && inner.expr_length() <= 2) {
    // TemplateSlotSequence(1..)
    inner.append()
  }

  return expr;
}*/

//} ... class TemplateBoxSlotImpl

//{ class TemplateBoxSlotSequenceImpl ...

TemplateBoxSlotSequenceImpl::TemplateBoxSlotSequenceImpl(TemplateBoxSlotSequence &_self)
  : self(_self)
{
}

static size_t to_unsigned_index(int signed_index, size_t len) {
  if(signed_index >= 0)
    return (size_t)signed_index;
    
  int neg_offset = -1 - signed_index;
  if((size_t)neg_offset < len)
    return len - (size_t)neg_offset;
    
  return 0;
}

void TemplateBoxSlotSequenceImpl::reload_content() {
  self.content()->load_from_object(get_content(), BoxInputFlags::AllowTemplateSlots | BoxInputFlags::FormatNumbers);
}

Expr TemplateBoxSlotSequenceImpl::get_content() {
  TemplateBox *tb = self.find_owner();
  if(!tb)
    return String("");
    
  size_t len = tb->arguments.expr_length();
  size_t start = to_unsigned_index(self._first_arg, len);
  size_t end = to_unsigned_index(self._last_arg, len);
  
  if(end < start)
    return List();
    
  Gather g;
  Gather::emit(get_single_mapped_argument(start));
  
  for(size_t i = start; i < end; ++i) {
    Gather::emit(self._separator);
    Gather::emit(get_single_mapped_argument(i + 1));
  }
  
  return g.end();
}

Expr TemplateBoxSlotSequenceImpl::get_single_mapped_argument(size_t i) {
  Expr result = Call(Symbol(PMATH_SYMBOL_TEMPLATESLOT), Expr(i));
  
  if(self._modifier_function.is_null())
    return result;
    
  if(self._modifier_function[0] == PMATH_SYMBOL_FUNCTION && self._modifier_function.expr_length() == 1) {
    Expr body = self._modifier_function[1];
    body = Call(Symbol(PMATH_SYMBOL_HOLDCOMPLETE), body);
    Expr func = Call(Symbol(PMATH_SYMBOL_FUNCTION), body);
    
    result = Application::interrupt_wait(Call(func, result), Application::button_timeout);
    
    if(result[0] == PMATH_SYMBOL_HOLDCOMPLETE && result.expr_length() == 1)
      return result[1];
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

//} ... class TemplateBoxSlotSequenceImpl
