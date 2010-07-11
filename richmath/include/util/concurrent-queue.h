#ifndef __UTIL__CONCURRENT_QUEUE_H__
#define __UTIL__CONCURRENT_QUEUE_H__

#include <util/pmath-extra.h>

#include <pmath-util/concurrency/atomic.h>

namespace richmath{
  template<typename T> class ConcurrentQueue: public Base{
    private:
      class Item{
        public:
          Item(): next(0){
          }
          
        public:
          Item *next;
          T     value;
      };
    public:
      ConcurrentQueue()
      : Base(),
        head_spin(0),
        tail_spin(0),
        head(new Item),
        tail(head)
      {
      }
      
      ~ConcurrentQueue(){
        while(head){
          Item *next = head->next;
          delete head;
          head = next;
        }
      }
      
      void put(const T &t){
        pmath_atomic_lock(&tail_spin);
        
        tail->value = t;
        tail = tail->next = new Item;
        
        pmath_atomic_unlock(&tail_spin);
      }
      
      bool get(T *result){
        Item *item = 0;
        pmath_atomic_lock(&head_spin);
        
        if(head != tail){
          item = head;
          head = head->next;
        }
        
        pmath_atomic_unlock(&head_spin);
        
        if(item){
          *result = item->value;
          delete item;
          return true;
        }
        return false;
      }
      
    private:
      PMATH_DECLARE_ATOMIC(head_spin);
      PMATH_DECLARE_ATOMIC(tail_spin);
      Item *head;
      Item *tail;
  };
}

#endif // __UTIL__CONCURRENT_QUEUE_H__
