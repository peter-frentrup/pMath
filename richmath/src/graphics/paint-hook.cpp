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
  : Shareable()
{
}

//} ... class PaintHook

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
  HOOK_ASSERT(!hook->_next.is_valid());
  
  Entry<Box *, SharedPtr<PaintHook> > *entry = _hooks.search_entry(box);
  if(entry) {
    hook->_next = entry->value;
    entry->value = hook;
    return;
  }
  
  _hooks.set(box, hook);
  return;
}

void PaintHookManager::run(Box *box, Context *context) {
  HOOK_ASSERT(box);
  HOOK_ASSERT(context);
  
  Entry<Box *, SharedPtr<PaintHook> > *entry = _hooks.search_entry(box);
  if(entry) {
    SharedPtr<PaintHook> hook = entry->value;
    
    float x0, y0;
    context->canvas->current_pos(&x0, &y0);
    
    context->canvas->save();
    while(hook.is_valid()) {
      hook->run(box, context);
      hook = hook->_next;
      
      context->canvas->move_to(x0, y0);
    }
    context->canvas->restore();
    
    _hooks.remove(box);
  }
}

void PaintHookManager::move_into(PaintHookManager &other) {
  for(unsigned i = 0, u = 0; u < _hooks.size(); ++i) {
    Entry<Box *, SharedPtr<PaintHook> > *my = _hooks.entry(i);
    
    if(my) {
      ++u;
      
      Entry<Box *, SharedPtr<PaintHook> > *their = other._hooks.search_entry(my->key);
      if(their) {
        SharedPtr<PaintHook> hook = my->value;
        
        while(hook.is_valid()) {
          SharedPtr<PaintHook> next = hook->_next;
          hook->_next = their->value;
          their->value = hook;
          
          hook = next;
        }
      }
      else
        other._hooks.set(my->key, my->value);
        
      my->value = 0;
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
