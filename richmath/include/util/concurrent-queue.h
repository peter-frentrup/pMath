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
        head(new Item),
        tail(head)
      {
        pmath_atomic_write_release(&head_spin, 0);
        pmath_atomic_write_release(&tail_spin, 0);
      }
      
      ~ConcurrentQueue(){
        while(head){
          Item *next = head->next;
          delete head;
          head = next;
        }
      }
      
      void put(const T &t){
        Item *new_tail = new Item;
        pmath_atomic_lock(&tail_spin);
        
        tail->value = t;
        tail = tail->next = new_tail;
        
        pmath_atomic_unlock(&tail_spin);
      }
      
      void put_front(const T &t){
        Item *new_head = new Item;
        new_head->value = t;
        
        pmath_atomic_lock(&head_spin);
        
        new_head->next = head;
        head = new_head;
        
        pmath_atomic_unlock(&head_spin);
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
      pmath_atomic_t head_spin;
      pmath_atomic_t tail_spin;
      Item *head;
      Item *tail;
  };
}

#endif // __UTIL__CONCURRENT_QUEUE_H__
