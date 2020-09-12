#ifndef RICHMATH__BOXES__ERRORBOX_H__INCLUDED
#define RICHMATH__BOXES__ERRORBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class ErrorBox final : public Box {
    protected:
      virtual ~ErrorBox();
    public:
      ErrorBox(const Expr object);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override { return _object; }
      
    private:
      Expr _object;
  };
}

#endif // RICHMATH__BOXES__ERRORBOX_H__INCLUDED
