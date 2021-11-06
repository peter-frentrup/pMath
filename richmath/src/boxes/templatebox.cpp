#include <boxes/templatebox.h>
#include <boxes/mathsequence.h>

#include <eval/application.h>
#include <eval/dynamic.h>
#include <eval/eval-contexts.h>

#include <gui/document.h>
#include <gui/native-widget.h>


using namespace richmath;
using namespace pmath;

namespace richmath{ namespace strings {
  extern String DollarFailed;
  extern String HeldTemplateSlot;
}}

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_ComplexStringBox;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Key;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_PureArgument;
extern pmath_symbol_t richmath_System_TemplateBox;
extern pmath_symbol_t richmath_System_TemplateSlot;
extern pmath_symbol_t richmath_System_TimeConstrained;
extern pmath_symbol_t richmath_System_Private_FlattenTemplateSequence;

extern pmath_symbol_t richmath_Internal_DynamicEvaluateMultiple;

extern pmath_symbol_t richmath_FE_Styles_DollarDefaultDisplayFunction;
extern pmath_symbol_t richmath_FE_Styles_DollarDefaultDisplayFunctionTooltip;

namespace richmath {
  class TemplateBoxImpl {
    public:
      TemplateBoxImpl(TemplateBox &_self);
      
      void ensure_content_loaded();
      
      void load_content(Expr dispfun);
      static bool is_valid_display_function(Expr dispfun);
      Expr display_function_body(Expr dispfun);
    
      void reset_argument(int index, Expr new_arg);
      void reset_arguments(Expr new_args);
      
    private:
      TemplateBox &self;
  };
  
  class TemplateBoxSlotImpl {
    public:
      TemplateBoxSlotImpl(TemplateBoxSlot &_self);
      
      static TemplateBox *find_owner(Box *box, bool same_document_only) { return find_owner_or_self(box->parent(), same_document_only); }
      static TemplateBox *find_owner_or_self(Box *box, bool same_document_only = false);
      
      void ensure_content_loaded();
      void reload_content();
      Expr get_content();
      
      void assign_content();
      
      static Expr prepare_boxes(Expr boxes);
      
    private:
      static Expr prepare_pure_arg(Expr expr);
      
    private:
      TemplateBoxSlot &self;
  };
  
  class CurrentValueOfTemplateSlot {
    public:
      static Expr get_next(Expr expr, Expr key, bool allow_options);
      
      static bool set(Expr &expr, const Expr &items, size_t depth, Expr value);
      
    private:
      static Expr get_next_by_index(Expr expr, int index);
      static Expr get_next_by_lookup(Expr expr, Expr name, bool allow_options);
      
      static bool set_by_index(Expr &expr, int index, const Expr &items, size_t depth, Expr value);
      static bool set_by_lookup(Expr &expr, Expr name, const Expr &items, size_t depth, Expr value);
  };
}

template<typename T>
static T *search_box(Box *start, LogicalDirection direction, Box *stop_parent) {
  while(start) {
    if(T *result = dynamic_cast<T*>(start))
      return result;
      
    start = start->next_box(direction, stop_parent);
  }
  return nullptr;
}

template<typename T>
static T *search_next_box(Box *start, LogicalDirection direction, Box *stop_parent) {
  if(!start)
    return nullptr;
  return search_box<T>(start->next_box(direction, stop_parent), direction, stop_parent);
}

//{ class TemplateBox ...

TemplateBox::TemplateBox()
  : base()
{
  style = new Style();
}

bool TemplateBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_TemplateBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr args = expr[1];
  if(args[0] != richmath_System_List)
    return false;
    
  Expr tag = expr[2];
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
  
  bool change_args = arguments != args;
  arguments = args;
  _tag = tag;
  is_content_loaded(false);
  style->clear();
  style->add_pmath(options);
  style->set_pmath(BaseStyleName, tag);
  
  finish_load_from_object(std::move(expr));
  if(change_args)
    notify_all();
    
  return true;
}

MathSequence *TemplateBox::as_inline_span() {
  return dynamic_cast<MathSequence*>(content());
}

bool TemplateBox::edit_selection(SelectionReference &selection) {
  return false;
}

bool TemplateBox::selectable(int i) {
  if(i >= 0)
    return false;
    
  return base::selectable(i);
}

VolatileSelection TemplateBox::normalize_selection(int start, int end) {
  if(start < end)
    return base::normalize_selection(start, end);
  
  return {this, start, end};
}

VolatileSelection TemplateBox::mouse_selection(Point pos, bool *was_inside_start) {
  auto sel = base::mouse_selection(pos, was_inside_start);
  if(!sel)
    return sel;
  
  if(is_parent_of(sel.box->mouse_sensitive()))
    return sel; // a button etc. inside the template definition
  
  for(Box *tmp = sel.box; tmp && tmp != this; tmp = tmp->parent()) {
    if(auto slot = dynamic_cast<TemplateBoxSlot*>(tmp)) {
      if(slot->find_owner() == this)
        return sel; // inside a template slot
    }
  }
  
  if(sel.selectable())
    return sel;
  
  *was_inside_start = true;
  return { this, 0, 0 };
}

Box *TemplateBox::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(*index < 0 || *index > length()) {
    TemplateBoxSlot *slot = search_next_box<TemplateBoxSlot>(this, direction, this);
    while(slot) {
      if(slot->find_owner_in_same_document() == this) {
        if(direction == LogicalDirection::Forward)
          *index = -1;
        else
          *index = slot->length() + 1;
          
        return slot->move_logical(direction, false, index);
      }
      slot = search_next_box<TemplateBoxSlot>(slot, direction, this);
    }
    
    if(auto par = parent()) {
      if(direction == LogicalDirection::Forward) {
        *index = _index;
        return par->move_logical(direction, true, index);
      }
      else {
        *index = _index + 1;
        return par->move_logical(direction, true, index);
      }
    }
    else {
      *index = 0;
      return this;
    }
  }
  
  return base::move_logical(direction, jumping, index);
}

Box *TemplateBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,  // [in/out]
  int              *index,        // [in/out], -1 if called from parent
  bool              called_from_child
) {
  float old_rel_x = *index_rel_x;
  
  Box *box = base::move_vertical(direction, index_rel_x, index, called_from_child);
  if(!box)
    return box;
  
  Box *tmp = box;
  while(tmp && tmp != this) {
    if(auto slot = dynamic_cast<TemplateBoxSlot*>(tmp)) {
      if(slot->find_owner_in_same_document() == this)
        return box;
    }
    
    tmp = tmp->parent();
  }
  
  if(tmp == nullptr)
    return box;
  
  *index_rel_x = old_rel_x;
  
  tmp = box->next_child_or_null(*index, direction);
  TemplateBoxSlot *slot = search_next_box<TemplateBoxSlot>(tmp, direction, this);
  if(slot) {
    if(direction == LogicalDirection::Forward)
      *index = -1;
    else
      *index = slot->length() + 1;
      
    box = slot->move_logical(direction, false, index);
  }
  else
    box = nullptr;
    
  if(!box) {
    if(auto par = parent()) {
      if(direction == LogicalDirection::Forward) {
        box = par;
        *index = _index + 1;
      }
      else {
        box = par;
        *index = _index;
      }
    }
    else {
      box = this;
      *index = 0;
    }
  }
  
  if(box == parent()) {
    if(*index != _index) {
      cairo_matrix_t mat1;
      cairo_matrix_t mat2;
      cairo_matrix_init_identity(&mat1);
      cairo_matrix_init_identity(&mat2);
      box->child_transformation(_index, &mat1);
      box->child_transformation(*index, &mat2);
      
      *index_rel_x = old_rel_x + mat1.x0 - mat2.x0;
    }
  }
  else {
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    box->transformation(this, &mat);
    box->child_transformation(*index, &mat);
    *index_rel_x = old_rel_x - mat.x0;
  }
  
  return box;
} 

void TemplateBox::resize_default_baseline(Context &context) {
  base::resize_default_baseline(context);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void TemplateBox::before_paint_inline(Context &context) {
  Impl(*this).ensure_content_loaded();
  base::before_paint_inline(context);
}

void TemplateBox::paint_content(Context &context) {
//  if(must_resize) {
//    context.canvas().save();
//    base::resize(context);
//    must_resize = false;
//    context.canvas().restore();
//  }

  base::paint_content(context);
  
  Impl(*this).ensure_content_loaded();
}

void TemplateBox::after_insertion() {
  Expr dispfun = get_own_style(DisplayFunction);
  if(!is_content_loaded() || dispfun != _cached_display_function) {
    Impl(*this).load_content(dispfun);
    is_content_loaded(true);
  }
  
  base::after_insertion();
}

Expr TemplateBox::to_pmath_symbol() {
  return Symbol(richmath_System_TemplateBox);
}

Expr TemplateBox::to_pmath(BoxOutputFlags flags) {
  Expr args = arguments;
  if(has(flags, BoxOutputFlags::WithDebugInfo)) {
    TemplateBoxSlot *slot = search_next_box<TemplateBoxSlot>(this, LogicalDirection::Forward, this);
    while(slot) {
      if(slot->find_owner() == this && slot->argument() > 0) 
        args.set(slot->argument(), slot->content()->to_pmath(flags));
      
      slot = search_next_box<TemplateBoxSlot>(slot, LogicalDirection::Forward, this);
    }
  }
  
  if(has(flags, BoxOutputFlags::Parseable)) {
    Expr ifun = get_own_style(InterpretationFunction);
    if(ifun.expr_length() == 1 && ifun[0] == richmath_System_Function) {
      ifun = Call(
               Symbol(richmath_System_Function), 
               Call(
                 Symbol(richmath_System_HoldComplete), 
                 ifun[1]));
      ifun = Call(
        Symbol(richmath_System_Private_FlattenTemplateSequence), 
        ifun, 
        args.expr_length());
      
      Expr boxes = args;
      boxes.set(0, std::move(ifun));
      boxes = Application::interrupt_wait(std::move(boxes), Application::button_timeout);
      if(boxes.expr_length() == 1 && boxes[0] == richmath_System_HoldComplete) 
        return boxes[1];
    }
  }
  
  if(has(flags, BoxOutputFlags::Literal))
    return content()->to_pmath(flags);

  Gather g;
  
  g.emit(args);
  g.emit(_tag);
  
  style->emit_to_pmath(false);
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_TemplateBox));
  return e;
}

void TemplateBox::on_mouse_enter() {
  if(auto doc = find_parent<Document>(false)) {
    Expr tooltip;
    
    if(Impl::is_valid_display_function(_cached_display_function)) {
      tooltip = { get_own_style(Tooltip) };
      
      if(tooltip.is_null() || tooltip == richmath_System_None)
        return;
      
      if(tooltip == richmath_System_Automatic)
        tooltip = _tag.to_string(PMATH_WRITE_OPTIONS_FULLSTR);
    }
    else {
      tooltip = Call(Symbol(richmath_FE_Styles_DollarDefaultDisplayFunctionTooltip), _tag);
      //tooltip = Application::interrupt_wait(std::move(tooltip), Application::button_timeout);
      
      tooltip = Call(Symbol(richmath_System_TimeConstrained), std::move(tooltip), Application::button_timeout);
      tooltip = Evaluate(std::move(tooltip));
    }
    
    doc->native()->show_tooltip(this, tooltip);
  }
}

void TemplateBox::on_mouse_exit() {
  if(auto doc = find_parent<Document>(false)) {
    if(Impl::is_valid_display_function(_cached_display_function)) {
      Expr tooltip { get_own_style(Tooltip) };
      
      if(tooltip.is_null() || tooltip == richmath_System_None)
        return;
    }
    
    doc->native()->hide_tooltip();
  }
}

void TemplateBox::reset_argument(int index, Expr new_arg) {
  Impl(*this).reset_argument(index, std::move(new_arg));
}

Expr TemplateBox::get_current_value_of_TemplateBox(FrontEndObject *obj, Expr item) {
  Box *box = dynamic_cast<Box*>(obj);
  if(item == richmath_System_TemplateBox) {
    if(!box)
      return Symbol(richmath_System_None);
    
    auto tb = TemplateBoxSlotImpl::find_owner_or_self(box);
    if(tb)
      return tb->to_pmath_id();
    
    return Symbol(richmath_System_None);
  }
  
  return Symbol(richmath_System_DollarFailed);
}

//} ... class TemplateBox

//{ class TemplateBoxSlot ...

TemplateBoxSlot::TemplateBoxSlot()
  : base(),
    _argument(0)
{
}
TemplateBox *TemplateBoxSlot::find_owner() {
  return Impl::find_owner(this, false);
}

TemplateBox *TemplateBoxSlot::find_owner_in_same_document() {
  return Impl::find_owner(this, true);
}

Expr TemplateBoxSlot::prepare_boxes(Expr boxes) {
  return Impl::prepare_boxes(boxes);
}

bool TemplateBoxSlot::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_TemplateSlot || expr.expr_length() != 1)
    return false;
    
  Expr arg = expr[1];
  if(arg.is_int32()) {
    _argument = PMATH_AS_INT32(arg.get());
    is_content_loaded(false);
    
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  return false;
}

Expr TemplateBoxSlot::prepare_dynamic(Expr expr) {
  if(TemplateBox *owner = find_owner())
    return owner->prepare_dynamic(std::move(expr));
    
  return base::prepare_dynamic(std::move(expr));
}

MathSequence *TemplateBoxSlot::as_inline_span() {
  return dynamic_cast<MathSequence*>(content());
}

bool TemplateBoxSlot::edit_selection(SelectionReference &selection) {
  if(TemplateBox *owner = find_owner_in_same_document()) {
    if(owner->parent())
      return owner->parent()->edit_selection(selection);
  }
  
  return base::edit_selection(selection);
}

bool TemplateBoxSlot::selectable(int i) {
  if(i >= 0) {
    if(TemplateBox *owner = find_owner_in_same_document()) {
      return owner->selectable();
    }
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

float TemplateBoxSlot::fill_weight() {
  return content()->fill_weight();
}

void TemplateBoxSlot::invalidate() {
  base::invalidate();
  
  if(is_content_loaded())
    has_changed_content(true);
}

void TemplateBoxSlot::resize_default_baseline(Context &context) {
  base::resize_default_baseline(context);
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void TemplateBoxSlot::before_paint_inline(Context &context) {
  Impl(*this).ensure_content_loaded();
  base::before_paint_inline(context);
}

void TemplateBoxSlot::paint_content(Context &context) {
  base::paint_content(context);
  Impl(*this).ensure_content_loaded();
}

void TemplateBoxSlot::after_insertion() {
  if(!is_content_loaded()) 
    Impl(*this).reload_content();
  
  base::after_insertion();
}

void TemplateBoxSlot::on_exit() {
  if(has_changed_content())
    Impl(*this).assign_content();
}

void TemplateBoxSlot::on_finish_editing() {
  if(has_changed_content())
    Impl(*this).assign_content();
}

Expr TemplateBoxSlot::get_current_value_of_TemplateSlotCount(FrontEndObject *obj, Expr item) {
  if(!item.is_string())
    return Symbol(richmath_System_DollarFailed);
  
  TemplateBox *tb = Impl::find_owner_or_self(dynamic_cast<Box*>(obj));
  if(!tb)
    return Symbol(richmath_System_DollarFailed);
  
  return Expr(tb->arguments.expr_length());
}

Expr TemplateBoxSlot::get_current_value_of_HeldTemplateSlot(FrontEndObject *obj, Expr item) {
  if(item[0] != richmath_System_List)
    return Symbol(richmath_System_DollarFailed);
  
  if(item[1] != strings::HeldTemplateSlot)
    return Symbol(richmath_System_DollarFailed);
  
  TemplateBox *tb = Impl::find_owner_or_self(dynamic_cast<Box*>(obj));
  if(!tb)
    return Symbol(richmath_System_DollarFailed);
  
  // TODO: register observer only for requested index
  tb->register_observer();
  
  Expr expr = tb->arguments;
  for(size_t depth = 2; depth <= item.expr_length(); ++depth) 
    expr = CurrentValueOfTemplateSlot::get_next(std::move(expr), item[depth], depth == 2);
  
  expr = EvaluationContexts::prepare_namespace_for(std::move(expr), tb);
  return Call(Symbol(richmath_System_HoldComplete), std::move(expr));
}

Expr TemplateBoxSlot::get_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item) {
  if(item[0] != richmath_System_List)
    return Symbol(richmath_System_DollarFailed);
  
  if(item[1] != richmath_System_TemplateSlot)
    return Symbol(richmath_System_DollarFailed);
  
  TemplateBox *tb = Impl::find_owner_or_self(dynamic_cast<Box*>(obj));
  if(!tb)
    return Symbol(richmath_System_DollarFailed);
  
  // TODO: register observer only for requested index
  tb->register_observer();
  
  Expr expr = tb->arguments;
  for(size_t depth = 2; depth <= item.expr_length(); ++depth) 
    expr = CurrentValueOfTemplateSlot::get_next(std::move(expr), item[depth], depth == 2);
  
  expr = Dynamic(tb, std::move(expr)).get_value_unevaluated();
  if(expr[0] == richmath_Internal_DynamicEvaluateMultiple)
    expr.set(2, obj->id().to_pmath_raw());
  
  return expr;
}

bool TemplateBoxSlot::put_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item, Expr rhs) {
  if(item[0] != richmath_System_List)
    return false;
  
  if(item[1] != richmath_System_TemplateSlot)
    return false;
  
  TemplateBox *tb = Impl::find_owner_or_self(dynamic_cast<Box*>(obj));
  if(!tb)
    return false;
  
  Expr expr = tb->arguments;
  for(size_t depth = 2; depth <= item.expr_length(); ++depth) 
    expr = CurrentValueOfTemplateSlot::get_next(std::move(expr), item[depth], depth == 2);
  
  Dynamic dyn{tb, expr};
  if(dyn.is_dynamic()) {
    dyn.assign(std::move(rhs));
    return true;
  }
  
  expr = tb->arguments;
  if(CurrentValueOfTemplateSlot::set(expr, item, 2, std::move(rhs))) {
    if(expr[0] == richmath_System_List) {
      TemplateBoxImpl(*tb).reset_arguments(expr);
      return true;
    }
  }
  return false;
}

//} ... class TemplateBoxSlot

//{ class TemplateBoxImpl ...

TemplateBoxImpl::TemplateBoxImpl(TemplateBox &_self)
  : self(_self)
{
}

void TemplateBoxImpl::ensure_content_loaded() {
  Expr dispfun = self.get_own_style(DisplayFunction);
  if(!self.is_content_loaded() || dispfun != self._cached_display_function) {
    load_content(dispfun);
    self.is_content_loaded(true);
    if(self.find_parent<Document>(false))
      self.base_after_insertion();
      
    self.invalidate();
  }
}

void TemplateBoxImpl::load_content(Expr dispfun) {
  self._cached_display_function = dispfun;
  self.content()->load_from_object(
    display_function_body(dispfun),
    BoxInputFlags::AllowTemplateSlots | BoxInputFlags::FormatNumbers);
}

bool TemplateBoxImpl::is_valid_display_function(Expr dispfun) {
  return dispfun[0] == richmath_System_Function && dispfun.expr_length() == 1;
}

Expr TemplateBoxImpl::display_function_body(Expr dispfun) {
  if(!is_valid_display_function(dispfun)) {
    Expr head = self._tag.to_string();
    dispfun = Call(Symbol(richmath_FE_Styles_DollarDefaultDisplayFunction), head);
  }
  
  dispfun = Call(Symbol(richmath_System_Private_FlattenTemplateSequence), dispfun, self.arguments.expr_length());
  //dispfun = Application::interrupt_wait(std::move(dispfun), Application::button_timeout);
  dispfun = Call(Symbol(richmath_System_TimeConstrained), std::move(dispfun), Application::button_timeout);
  dispfun = Evaluate(std::move(dispfun));
  
  if(dispfun[0] == richmath_System_Function && dispfun.expr_length() == 1)
    return dispfun[1];
    
  return strings::DollarFailed;
}

void TemplateBoxImpl::reset_argument(int index, Expr new_arg) {
  self.arguments.set(index, std::move(new_arg));
  
  // TODO: We should maybe use Array<ObservableValue<Expr>> instead?
  self.notify_all();
  
  TemplateBoxSlot *slot = search_box<TemplateBoxSlot>(&self, LogicalDirection::Forward, &self);
  while(slot) {
    if(slot->argument() == index && slot->find_owner() == &self) {
      //slot->_is_content_loaded = false;
      slot->invalidate();
      TemplateBoxSlotImpl(*slot).reload_content();
    }
    slot = search_next_box<TemplateBoxSlot>(slot, LogicalDirection::Forward, &self);
  }
}

void TemplateBoxImpl::reset_arguments(Expr new_args) {
  using std::swap;
  swap(new_args, self.arguments);
  
  // TODO: We should maybe use Array<ObservableValue<Expr>> instead?
  self.notify_all();
  
  TemplateBoxSlot *slot = search_box<TemplateBoxSlot>(&self, LogicalDirection::Forward, &self);
  while(slot) {
    auto num = slot->argument();
    if(slot->find_owner() == &self) {
      if(num <= 0 || new_args[num] != self.arguments[num]) {
        //slot->_is_content_loaded = false;
        slot->invalidate();
        TemplateBoxSlotImpl(*slot).reload_content();
      }
    }
    slot = search_next_box<TemplateBoxSlot>(slot, LogicalDirection::Forward, &self);
  }
}

//} ... class TemplateBoxImpl

//{ class TemplateBoxSlotImpl ...

TemplateBoxSlotImpl::TemplateBoxSlotImpl(TemplateBoxSlot &_self)
  : self(_self)
{
}

TemplateBox *TemplateBoxSlotImpl::find_owner_or_self(Box *box, bool same_document_only) {
  int nesting = 0;
  int max_doc_recursion = 10;
  while(box) {
    if(dynamic_cast<TemplateBoxSlot*>(box)) {
      ++nesting;
    }
    else if(auto tb = dynamic_cast<TemplateBox*>(box)) {
      if(nesting-- == 0)
        return tb;
    }
    else if(auto doc = dynamic_cast<Document*>(box)) {
      if(same_document_only)
        break;
      
      if(--max_doc_recursion < 0) {
        pmath_debug_print("[too deep document -> source_box nesting]\n");
        break;
      }
      
      box = doc->native()->source_box();
      continue;
    }
    box = box->parent();
  }
  return nullptr;
}

void TemplateBoxSlotImpl::ensure_content_loaded() {
  if(!self.is_content_loaded()) {
    self.invalidate();
    reload_content();
    if(self.find_parent<Document>(false))
      self.base_after_insertion();
  }
}

void TemplateBoxSlotImpl::reload_content() {
  BoxInputFlags flags = BoxInputFlags::Default;
  if(self._argument < 0) // temporarily detached inside assign_content()
    return;
  
  TemplateBox *owner = self.find_owner();
  if(owner) {
    flags |= BoxInputFlags::FormatNumbers;
    if(owner->find_parent<TemplateBox>(false))
      flags |= BoxInputFlags::AllowTemplateSlots;
  }
  
  self.content()->load_from_object(get_content(), flags);
  self.is_content_loaded(true);
  self.has_changed_content(false);
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
  self.has_changed_content(false);
  
  TemplateBox *tb = self.find_owner();
  if(!tb)
    return;
  
  auto arg = self._argument;
  self._argument = -1;
  tb->reset_argument(arg, self.content()->to_pmath(BoxOutputFlags::Default));
  self._argument = arg;
}

Expr TemplateBoxSlotImpl::prepare_boxes(Expr boxes) {
  if(boxes[0] == richmath_System_PureArgument)
    return prepare_pure_arg(boxes);
    
  if(boxes[0] == richmath_System_List) {
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
    return Call(Symbol(richmath_System_TemplateSlot), num);
    
  return expr;
}

//} ... class TemplateBoxSlotImpl

//{ class CurrentValueOfTemplateSlot ...

Expr CurrentValueOfTemplateSlot::get_next(Expr expr, Expr key, bool allow_options) {
  if(key.is_int32())
    return get_next_by_index(std::move(expr), PMATH_AS_INT32(key.get()));
  
  if(key[0] == richmath_System_Key && key.expr_length() == 1)
    return get_next_by_lookup(std::move(expr), key[1], allow_options);
  
  return Symbol(richmath_System_DollarFailed);
}

Expr CurrentValueOfTemplateSlot::get_next_by_index(Expr expr, int index) {
  if(expr[0] != richmath_System_List)
    return Symbol(richmath_System_DollarFailed);
    
  if(index <= 0 || (size_t)index > expr.expr_length())
    return Symbol(richmath_System_DollarFailed);
  
  return expr[index];
}

Expr CurrentValueOfTemplateSlot::get_next_by_lookup(Expr expr, Expr name, bool allow_options) {
  if(expr.is_list_of_rules()) 
    return expr.lookup(name, Symbol(richmath_System_Inherited));
  
  if(!allow_options)
    return Symbol(richmath_System_DollarFailed);
  
  size_t i = expr.expr_length();
  Expr last = expr[i];
  if(last.is_list_of_rules()) 
    return last.lookup(name, Symbol(richmath_System_Inherited));
  
  for(; i > 0; last = expr[--i]) {
    if(!last.is_rule())
      return Symbol(richmath_System_Inherited);
    
    if(last[1] == name) 
      return last[2];
  }
  
  return Symbol(richmath_System_Inherited);
}

bool CurrentValueOfTemplateSlot::set(Expr &expr, const Expr &items, size_t depth, Expr value) {
  if(depth > items.expr_length()) {
    expr = std::move(value);
    return true;
  }
  
  if(depth > 10)
    return false;
  
  Expr key = items[depth];
  if(key.is_int32()) 
    return set_by_index(expr, PMATH_AS_INT32(key.get()), items, depth, std::move(value));
  
  if(key[0] == richmath_System_Key && key.expr_length() == 1)
    return set_by_lookup(expr, key[1], items, depth, std::move(value));
  
  return false;
}

bool CurrentValueOfTemplateSlot::set_by_index(Expr &expr, int index, const Expr &items, size_t depth, Expr value) {
  if(expr[0] != richmath_System_List)
    return false;
    
  if(index <= 0 || (size_t)index > expr.expr_length())
    return false;
  
  Expr rest = expr[index];
  bool success = set(rest, items, depth + 1, std::move(value));
  if(success)
    expr.set(index, std::move(rest));
  return success;
}

bool CurrentValueOfTemplateSlot::set_by_lookup(Expr &expr, Expr name, const Expr &items, size_t depth, Expr value) {
  if(expr.is_list_of_rules()) {
    Expr rest = expr.lookup(name, Symbol(richmath_System_Inherited));
    bool success = set(rest, items, depth + 1, std::move(value));
    if(success) {
      if(rest == richmath_System_Inherited)
        rest = Expr(PMATH_UNDEFINED);
      expr.set_lookup(std::move(name), std::move(rest));
    }
    return success;
  }
  
  bool allow_options = (depth == 2 && items[1] == richmath_System_TemplateSlot) || depth == 1;
  if(!allow_options)
    return false;
  
  size_t i = expr.expr_length();
  Expr last = expr[i];
  if(last.is_list_of_rules()) {
    bool success = set_by_lookup(last, std::move(name), items, depth, std::move(value));
    if(success)
      expr.set(i, std::move(last));
    return success;
  }
  
  for(; i > 0; last = expr[--i]) {
    if(!last.is_rule())
      break;
    
    if(last[1] == name) {
      Expr rhs = last[2];
      bool success = set(rhs, items, depth + 1, std::move(value));
      if(success) {
        if(rhs == richmath_System_Inherited) {
          expr.set(i, Expr(PMATH_UNDEFINED));
          expr.expr_remove_all(Expr(PMATH_UNDEFINED));
        }
        else{
          last.set(2, std::move(rhs));
          expr.set(i, std::move(last));
        }
      }
      return success;
    }
  }
  
  last = Expr();
  bool success = set(last, items, depth + 1, std::move(value));
  if(success && last != richmath_System_Inherited)
    expr.append(Rule(std::move(name), std::move(last)));
  return success;
}

//} ... class CurrentValueOfTemplateSlot
