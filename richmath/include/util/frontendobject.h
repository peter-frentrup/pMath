#ifndef RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
#define RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED

#include <util/pmath-extra.h>
#include <util/hashtable.h>


namespace richmath {
  class FrontEndObject;
  
  class FrontEndReference {
      friend class FrontEndObject;
      friend class FrontEndReferenceImpl;
    public:
      int hash() const { return default_hash(_id) ^ 0xBADC0DE; }
      
      bool is_valid() const { return !!_id; }
      explicit operator bool() const { return is_valid(); }
      
      static FrontEndReference of(FrontEndObject *obj);
      static FrontEndReference from_pmath(Expr expr);
      static FrontEndReference from_pmath_raw(Expr expr);
      
      Expr to_pmath() const;
      Expr to_pmath_raw() const { return Expr((int32_t)_id); }
      
      friend bool operator==(FrontEndReference left, FrontEndReference right) { return left._id == right._id; }
      friend bool operator!=(FrontEndReference left, FrontEndReference right) { return left._id != right._id; }
      friend bool operator<( FrontEndReference left, FrontEndReference right) { return left._id <  right._id; }
      friend bool operator<=(FrontEndReference left, FrontEndReference right) { return left._id <= right._id; }
      friend bool operator>( FrontEndReference left, FrontEndReference right) { return left._id >  right._id; }
      friend bool operator>=(FrontEndReference left, FrontEndReference right) { return left._id >= right._id; }
      
      static const FrontEndReference None;
      
      static void *unsafe_cast_to_pointer(FrontEndReference ref) {
        return (void*)(uintptr_t)ref._id;
      }
      
      static FrontEndReference unsafe_cast_from_pointer(void *p) {
        FrontEndReference ref;
        ref._id = (uint32_t)(uintptr_t)p;
        return ref;
      }
    
    private:
      uint32_t _id;
  };
  
  /** Suspending deletions of objects.
  
      While destruction suspended is in effect, objects will be remembered in a free
      list (the limbo). When destruction mode is resumed, all objects in the limbo are
      actually deleted.
  
      During Box mouse_down()/paint()/... handlers, the document might change.
      This could cause a parent box to be removed. Since it is still referenced
      on the stack, such a box should not be wiped out until the call stack is clean.
  
      Hence, the widget which forwards all calls to Box/Document should suppress
      destruction of objects during event handling.
   */
  class AutoMemorySuspension {
    public:
      AutoMemorySuspension() { suspend_deletions(); }
      ~AutoMemorySuspension() { resume_deletions(); }
      
      static bool are_deletions_suspended();
      
    private:
      static void suspend_deletions();
      static void resume_deletions();
  };
  
  class ObjectWithLimbo: public Base {
      friend class AutoMemorySuspension;
    public:
      virtual ~ObjectWithLimbo();
    
      /// Mark the object for deletion.
      ///
      /// You should normally use this function instead of delete.
      /// \see AutoMemorySuspension
      virtual void safe_destroy();
      
    protected:
      virtual ObjectWithLimbo *next_in_limbo() = 0;
      virtual void next_in_limbo(ObjectWithLimbo *next) = 0;
  };

  class FrontEndObject: public ObjectWithLimbo {
    public:
      FrontEndObject();
      virtual ~FrontEndObject();
      
      FrontEndReference id() { return _id; }
      virtual Expr to_pmath_id() { return _id.to_pmath(); }
      
      static FrontEndObject *find(FrontEndReference id);
      static FrontEndObject *find_box_reference(Expr boxref);
      
      template<class T>
      static T *find_cast(FrontEndReference id) {
        return dynamic_cast<T*>(find(id));
      }
      
      void swap_id(FrontEndObject *other);
      
      virtual Expr update_cause() { return Expr(); }
      virtual void update_cause(Expr cause) {}
      
      /// Notifies that a Dynamic value changed which this object is tracking.
      virtual void dynamic_updated() = 0;
    
      /// Modify an expression before it gets evaluated. 
      virtual Expr prepare_dynamic(Expr expr) { return expr; }

      /// An asynchronous dynamic evaluation was finished.
      ///
      /// \param info   The info argument given to the corresponding DynamicEvaluationJob. Usually null.
      /// \param result The evaluation result.
      virtual void dynamic_finished(Expr info, Expr result) {}
      
      bool has_box_id() { return get_flag(HasBoxID); }
      void has_box_id(bool value) { change_flag(HasBoxID, value); } // to be used by Stylesheet only
    
    protected:
      bool get_flag(unsigned i) { return (_flags & (1u << i)) != 0; }
      void change_flag(unsigned i, bool value) { if(value) { set_flag(i); } else { clear_flag(i); } }
      void set_flag(unsigned i) {    _flags |=  (1u << i); }
      void clear_flag(unsigned i) {  _flags &= ~(1u << i);}
    private:
      FrontEndReference _id;
      unsigned          _flags;
    
    protected:
      enum {
        HasBoxID = 0,
        
        NumFlagsBits,
      };
      
      enum {
        MaximumFlagsBits = 8 * sizeof(_flags)
      };
  };
  
  inline FrontEndReference FrontEndReference::of(FrontEndObject *obj) {
    return obj ? obj->id() : FrontEndReference::None;
  }
  
  // http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
  static struct FrontEndObjectInitializer {
    FrontEndObjectInitializer();
    ~FrontEndObjectInitializer();
  } TheFrontEndObjectInitializer;
}

#endif // RICHMATH__UTIL__FRONTENDOBJECT_H__INCLUDED
