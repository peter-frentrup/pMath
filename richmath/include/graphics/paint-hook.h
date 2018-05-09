#ifndef __GRAPHICS__PAINT_HOOK_H__
#define __GRAPHICS__PAINT_HOOK_H__

#include <util/hashtable.h>
#include <util/sharedptr.h>


namespace richmath {
  class Box;
  class Context;
  class PaintHookList;
  
  class PaintHook: public Shareable {
      friend class PaintHookList;
    public:
      PaintHook();
      
      virtual void run(Box *box, Context *context) = 0;
      
    private:
      PaintHook *_next;
  };
  
  class PaintHookList {
    public:
      PaintHookList();
      PaintHookList(const PaintHookList &other) = delete;
      PaintHookList(PaintHookList &&other);
      virtual ~PaintHookList();
      
      PaintHookList &operator=(const PaintHookList &other) = delete;
      PaintHookList &operator=(PaintHookList &&other);
      
      void push(SharedPtr<PaintHook> hook);
      SharedPtr<PaintHook> pop();
      
    private:
      PaintHook * _first;
  };
  
  class PaintHookManager: public Base {
    public:
      PaintHookManager();
      
      void clear();
      void add(Box *box, SharedPtr<PaintHook> hook);
      void run(Box *box, Context *context); // also removes all hooks for the box
      
      void move_into(PaintHookManager &other);
      
    private:
      Hashtable<Box *, PaintHookList, cast_hash> _hooks;
  };
  
  
  
  class SelectionFillHook: public PaintHook {
    public:
      SelectionFillHook(int _start, int _end, int _color, float _alpha = 1.0);
      
      virtual void run(Box *box, Context *context) override;
      
    public:
      int start;
      int end;
      int color;
      float alpha;
  };
}


#endif // __GRAPHICS__PAINT_HOOK_H__
