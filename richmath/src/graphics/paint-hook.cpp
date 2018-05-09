#include<graphics/paint-hook.h>

#include<boxes/mathsequence.h>
#include<graphics/context.h>


using namespace richmath;

#define HOOK_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)

//{ class PaintHook ...

PaintHook::PaintHook()
  : Shareable(),
    _next(nullptr)
{
}

//} ... class PaintHook

//{ class PaintHookList ...

PaintHookList::PaintHookList()
  : _first(nullptr)
{
}

PaintHookList::PaintHookList(PaintHookList &&other)
  : _first(nullptr)
{
  _first = other._first;
  other._first = nullptr;
}

PaintHookList::~PaintHookList() {
  while(_first) {
    auto next = _first->_next;
    _first->_next = nullptr;
    _first->unref();
    _first = next;
  }
}

PaintHookList &PaintHookList::operator=(PaintHookList &&other) {
  if(this != &other) {
    while(_first) {
      auto next = _first->_next;
      _first->_next = nullptr;
      _first->unref();
      _first = next;
    }
    
    _first = other._first;
    other._first = nullptr;
  }
  return *this;
}

void PaintHookList::push(SharedPtr<PaintHook> hook) {
  HOOK_ASSERT(hook.is_valid());
  HOOK_ASSERT(hook->_next == nullptr);
  
  hook->_next = _first;
  _first = hook.release();
}

SharedPtr<PaintHook> PaintHookList::pop() {
  if(_first == nullptr)
    return nullptr;
    
  SharedPtr<PaintHook> result = _first;
  _first = _first->_next;
  result->_next = nullptr;
  return result;
}

//} ... class PaintHookList

//{ class PaintHookManager ...

PaintHookManager::PaintHookManager()
  : Base()
{
}

void PaintHookManager::clear() {
  _hooks.clear();
}

void PaintHookManager::add(Box *box, SharedPtr<PaintHook> hook) {
  HOOK_ASSERT(box);
  HOOK_ASSERT(hook.is_valid());
  
  Entry<Box *, PaintHookList > *entry = _hooks.search_entry(box);
  if(entry) {
    entry->value.push(hook);
  }
  else {
    PaintHookList hook_list;
    hook_list.push(hook);
    _hooks.set(box, std::move(hook_list));
  }
  
  return;
}

void PaintHookManager::run(Box *box, Context *context) {
  HOOK_ASSERT(box);
  HOOK_ASSERT(context);
  
  Entry<Box *, PaintHookList> *entry = _hooks.search_entry(box);
  if(!entry)
    return;
    
  PaintHookList &hook_list = entry->value;
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  
  context->canvas->save();
  for(auto hook = hook_list.pop(); hook.is_valid(); hook = hook_list.pop()) {
    hook->run(box, context);
    context->canvas->move_to(x0, y0);
  }
  context->canvas->restore();
  
  _hooks.remove(box);
}

void PaintHookManager::move_into(PaintHookManager &other) {
  for(unsigned i = 0, u = 0; u < _hooks.size(); ++i) {
    Entry<Box *, PaintHookList > *my = _hooks.entry(i);
    
    if(my) {
      ++u;
      
      PaintHookList &my_list = my->value;
      Entry<Box *, PaintHookList> *their = other._hooks.search_entry(my->key);
      
      if(their) {
        PaintHookList &their_list = their->value;
        
        for(auto hook = my_list.pop(); hook.is_valid(); hook = my_list.pop()) {
          their_list.push(hook);
        }
      }
      else
        other._hooks.set(my->key, std::move(my_list));
    }
  }
  
  _hooks.clear();
}

//} ... class PaintHookManager

//{ class SelectionFillHook ...

SelectionFillHook::SelectionFillHook(int _start, int _end, int _color, float _alpha)
  : PaintHook(),
    start(_start),
    end(_end),
    color(_color),
    alpha(_alpha)
{
}

void SelectionFillHook::run(Box *box, Context *context) {
  context->canvas->save();
  
  if(MathSequence *mseq = dynamic_cast<MathSequence*>(box))
    mseq->selection_path(context, context->canvas, start, end);
  else
    box->selection_path(context->canvas, start, end);
    
  int old_c = context->canvas->get_color();
  context->canvas->set_color(color, alpha);
  
  context->canvas->fill();
  
  context->canvas->set_color(old_c);
  context->canvas->restore();
}

//} ... class SelectionFillHook
