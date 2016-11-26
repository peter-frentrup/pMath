#ifndef __BOXES__ERRORBOX_H__
#define __BOXES__ERRORBOX_H__

#include <boxes/box.h>


namespace richmath {
  class ErrorBox: public Box {
    public:
      ErrorBox(const Expr object);
      virtual ~ErrorBox();
      
      virtual bool try_load_from_object(Expr expr, int options) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(int flags) override { return _object; }
      
    private:
      Expr _object;
  };
}

#endif // __BOXES__ERRORBOX_H__
