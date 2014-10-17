#include <pmath-util/stacks-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/memory.h>

#include <string.h>


static pmath_bool_t have_cas2;

PMATH_API
pmath_stack_t pmath_stack_new(void) {
/* Need 16 byte aligned pointer on x64 for CMPXCHG16B but pmath_mem_alloc only 
   gives 8 byte.
   Memory returned by memalign() is not free()-able on some machines. So we do 
   alignment manually.
   
   If uint8_t *p = pmath_stack_new() then (p - p[-1]) is the location returned by malloc()
*/
  const size_t size = sizeof(struct _pmath_stack_t) + 1 + 2 * sizeof(void*);
  
  uint8_t *ptr = pmath_mem_alloc(size);
  size_t addr;
  uint8_t offset;
                     
  if(!ptr)
    return NULL;
    
  memset(ptr, 0, size);
  
  // round ptr+1 to next multiple of 2*sizeof(void*)
  addr = (size_t)ptr;
  addr = (1 + addr/(2 * sizeof(void*))) * (2 * sizeof(void*));
  
  offset = (uint8_t)(addr - (size_t)ptr);
  ptr = (void*)addr;
  ptr[-1] = offset;
  
  pmath_atomic_barrier();
  return (pmath_stack_t)ptr;
}

PMATH_API
void pmath_stack_free(pmath_stack_t stack) {
  uint8_t *ptr = (void*)stack;
  uint8_t offset = ptr[-1];
  
  pmath_mem_free(ptr - offset);
}

static void  (*stack_push)(pmath_stack_t, void*);
static void *(*stack_pop)( pmath_stack_t);

//{ threadsafe push/pop with CAS and CAS2 (lockfree) ...
static void lockfree_push(pmath_stack_t stack, void *item) {
  assert(have_cas2);
  do {
    ((struct _pmath_stack_item_t*)item)->next = stack->u.s.top;
  } while(!pmath_atomic_compare_and_set(
            (pmath_atomic_t*)&stack->u.s.top,
            (intptr_t)((struct _pmath_stack_item_t*)item)->next,
            (intptr_t)item));
}

static void *lockfree_pop(pmath_stack_t stack) {
  struct _pmath_stack_item_t *head;
  struct _pmath_stack_item_t *next;
  intptr_t oc;
  
  assert(have_cas2);
  
  do {
    head = stack->u.s.top;
    oc   = stack->u.s.operation_counter_or_spinlock;
    if(!head)
      return NULL;
    next = head->next; // What is if we already freed head?
  } while(!pmath_atomic_compare_and_set_2(
            &stack->u.as_atomic2,
            (intptr_t)head, oc,
            (intptr_t)next, oc + 1));
            
  return head;
}
//}

//{ threadsafe push/pop through locks ...
static void locking_push(pmath_stack_t stack, void *item) {
  pmath_atomic_lock((pmath_atomic_t*)&stack->u.s.operation_counter_or_spinlock);
  
  ((struct _pmath_stack_item_t*)item)->next = stack->u.s.top;
  stack->u.s.top = item;
  
  pmath_atomic_unlock((pmath_atomic_t*)&stack->u.s.operation_counter_or_spinlock);
}

static void *locking_pop(pmath_stack_t stack) {
  void *head;
  
  pmath_atomic_lock((pmath_atomic_t*)&stack->u.s.operation_counter_or_spinlock);
  
  head = stack->u.s.top;
  if(head)
    stack->u.s.top = ((struct _pmath_stack_item_t*)head)->next;
    
  pmath_atomic_unlock((pmath_atomic_t*)&stack->u.s.operation_counter_or_spinlock);
  return head;
}
//}

PMATH_API
PMATH_ATTRIBUTE_NONNULL(1, 2)
void pmath_stack_push(pmath_stack_t stack, void *item) {
  assert(stack != NULL);
  assert(item != NULL);
  
  stack_push(stack, item);
}

PMATH_API
void *pmath_stack_pop(pmath_stack_t stack) {
  assert(stack != NULL);
  
  return stack_pop(stack);
}

PMATH_PRIVATE
pmath_bool_t _pmath_stacks_init(void) {
  have_cas2 = pmath_atomic_have_cas2();
  
  if(have_cas2) {
    stack_push = lockfree_push;
    stack_pop  = lockfree_pop;
  }
  else {
    stack_push = locking_push;
    stack_pop  = locking_pop;
  }
  
  return TRUE;
}
